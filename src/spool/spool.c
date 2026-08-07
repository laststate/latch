#include "../core/internal.h"

#include "laststate/envelope.h"

#define SPOOL_VERSION 3u
#define SPOOL_HEADER_COMMITTED 0xa5u
#define SPOOL_RECORD_MAGIC 0x4c535245u
#define SPOOL_RECORD_COMMITTED 0xa5u
#define SPOOL_RECORD_ACKED 0x00u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t slots;
    uint32_t record_size;
    uint32_t sequence_floor;
    uint32_t header_crc;
    uint8_t committed;
    uint8_t reserved[3];
} spool_header_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t sequence;
    uint32_t data_crc;
    uint8_t priority;
    uint8_t event_type;
    uint16_t reserved;
    uint32_t header_crc;
    uint8_t state;
    uint8_t retry_bits;
    uint16_t reserved_after_state;
} spool_record_t;

_Static_assert(sizeof(spool_header_t) == 24u, "spool header layout changed");
_Static_assert(sizeof(spool_record_t) == 28u, "spool record layout changed");

static bool add_size(size_t left, size_t right, size_t *result) {
    if (left > SIZE_MAX - right) {
        return false;
    }

    *result = left + right;
    return true;
}

static size_t record_size(void) {
    return sizeof(spool_record_t) + LS_MAX_EVENT_SIZE;
}

static size_t raw_storage_size(void) {
    size_t records;
    size_t total;
    if (record_size() > SIZE_MAX / LS_SPOOL_MAX_RECORDS ||
        !add_size(0u, record_size() * LS_SPOOL_MAX_RECORDS, &records) ||
        !add_size(sizeof(spool_header_t), records, &total)) {
        return 0;
    }

    return total;
}

static size_t slot_offset(size_t index) {
    return sizeof(spool_header_t) + index * record_size();
}

size_t ls_spool_storage_size(void) {
    size_t size = raw_storage_size();
    if (!size) {
        return 0;
    }

    size_t remainder = size % LS_STORAGE_MAX_WRITE_SIZE;
    if (!remainder) {
        return size;
    }

    if (size > SIZE_MAX - (LS_STORAGE_MAX_WRITE_SIZE - remainder)) {
        return 0;
    }

    return size + LS_STORAGE_MAX_WRITE_SIZE - remainder;
}

size_t ls_storage_required_size(void) {
    size_t spool_size = ls_spool_storage_size();
    size_t total;
    return spool_size && add_size(spool_size, sizeof(ls_persistent_boot_t), &total) ? total : 0;
}

static bool usable(void) {
    size_t size = ls_spool_storage_size();
    return size && ls_runtime.storage && ls_runtime.storage->read && ls_runtime.storage->write &&
           ls_runtime.storage->capacity >= size;
}

static ls_result_t storage_sync(void) {
    return ls_runtime.storage->sync ? ls_runtime.storage->sync(ls_runtime.storage->context) : LS_OK;
}

static ls_result_t read_header(spool_header_t *header) {
    return ls_runtime.storage->read(ls_runtime.storage->context, 0u, header, sizeof(*header));
}

static uint32_t spool_header_crc(const spool_header_t *header) {
    return ls_crc32(header, offsetof(spool_header_t, header_crc));
}

static uint32_t spool_record_crc(const spool_record_t *record) {
    return ls_crc32(record, offsetof(spool_record_t, header_crc));
}

static bool header_valid(const spool_header_t *header) {
    return header->magic == LS_STORAGE_MAGIC && header->version == SPOOL_VERSION &&
           header->slots == LS_SPOOL_MAX_RECORDS && header->record_size == record_size() &&
           header->committed == SPOOL_HEADER_COMMITTED &&
           header->header_crc == spool_header_crc(header);
}

static bool record_valid(const spool_record_t *record) {
    return record->magic == SPOOL_RECORD_MAGIC && record->version == SPOOL_VERSION &&
           record->length > 0u && record->length <= LS_MAX_EVENT_SIZE &&
           record->priority <= LS_PRIORITY_DIAGNOSTIC && record->event_type >= LS_EVENT_CRASH &&
           record->event_type <= LS_EVENT_COREDUMP &&
           record->header_crc == spool_record_crc(record);
}

static bool record_is_erased(const spool_record_t *record) {
    const uint8_t *bytes = (const uint8_t *)record;
    for (size_t index = 0; index < sizeof(*record); index++) {
        if (bytes[index] != 0xffu) {
            return false;
        }
    }

    return true;
}

static bool generation_is_newer(uint32_t candidate, uint32_t reference) {
    return (int32_t)(candidate - reference) > 0;
}

static ls_result_t write_header(void) {
    spool_header_t header;
    ls_memset(&header, 0xff, sizeof(header));
    header.magic = LS_STORAGE_MAGIC;
    header.version = SPOOL_VERSION;
    header.slots = LS_SPOOL_MAX_RECORDS;
    header.record_size = (uint32_t)record_size();
    header.sequence_floor = ls_runtime.sequence;
    header.header_crc = spool_header_crc(&header);

    ls_result_t result = ls_storage_program(ls_runtime.storage, 0u, &header, sizeof(header));
    if (result == LS_OK) {
        result = storage_sync();
    }
    if (result == LS_OK) {
        const uint8_t committed = SPOOL_HEADER_COMMITTED;
        result = ls_storage_program(ls_runtime.storage, offsetof(spool_header_t, committed),
                                    &committed, sizeof(committed));
    }
    if (result == LS_OK) {
        result = storage_sync();
    }

    return result;
}

static ls_result_t erase_spool(void) {
    size_t size = ls_spool_storage_size();
    if (!ls_runtime.storage->erase) {
        return LS_ECORRUPT;
    }
    if (ls_runtime.storage->erase_size > 1u && size % ls_runtime.storage->erase_size) {
        return LS_ENOTSUP;
    }

    ls_result_t result = ls_runtime.storage->erase(ls_runtime.storage->context, 0u, size);
    if (result == LS_OK) {
        result = storage_sync();
    }

    return result;
}

static ls_result_t scan_records(uint32_t *sequence, int *has_record) {
    if (!sequence || !has_record) {
        return LS_EINVAL;
    }

    *has_record = 0;
    for (size_t slot = 0; slot < LS_SPOOL_MAX_RECORDS; slot++) {
        spool_record_t record;
        ls_result_t result = ls_runtime.storage->read(ls_runtime.storage->context,
                                                      slot_offset(slot), &record, sizeof(record));
        if (result != LS_OK) {
            return result;
        }

        if (record_valid(&record)) {
            *has_record = 1;
            if (generation_is_newer(record.sequence, *sequence)) {
                *sequence = record.sequence;
            }
        }
    }

    return LS_OK;
}

ls_result_t ls_spool_init(void) {
    if (!usable()) {
        return ls_runtime.storage ? LS_ENOSPACE : LS_OK;
    }

    spool_header_t header;
    ls_result_t result = read_header(&header);
    if (result != LS_OK) {
        return result;
    }

    uint32_t sequence = header_valid(&header) ? header.sequence_floor : 0u;
    int has_record = 0;
    result = scan_records(&sequence, &has_record);
    if (result != LS_OK) {
        return result;
    }

    if (header_valid(&header) || has_record) {
        if (generation_is_newer(sequence, ls_runtime.sequence)) {
            ls_runtime.sequence = sequence;
        }
        return LS_OK;
    }

    int erased = 0;
    result = ls_storage_is_erased(ls_runtime.storage, 0u, ls_spool_storage_size(), &erased);
    if (result != LS_OK) {
        return result;
    }
    if (!erased) {
        result = erase_spool();
        if (result != LS_OK) {
            return result;
        }
    }

    return write_header();
}

static size_t normal_slot_limit(void) {
    return LS_SPOOL_MAX_RECORDS - LS_SPOOL_RESERVED_CRITICAL;
}

static size_t critical_slot_limit(void) {
    return LS_SPOOL_MAX_RECORDS - LS_SPOOL_RESERVED_EMERGENCY;
}

static size_t slot_limit_for_priority(ls_priority_t priority) {
    if (priority == LS_PRIORITY_EMERGENCY)
        return LS_SPOOL_MAX_RECORDS;
    if (priority == LS_PRIORITY_CRITICAL)
        return critical_slot_limit();
    return normal_slot_limit();
}

static ls_result_t find_empty_slot(ls_priority_t priority, size_t *slot_out) {
    if (!slot_out) {
        return LS_EINVAL;
    }

    size_t limit = slot_limit_for_priority(priority);
    for (size_t slot = 0; slot < limit; slot++) {
        int erased = 0;
        ls_result_t result = ls_storage_is_erased(ls_runtime.storage, slot_offset(slot),
                                                  sizeof(spool_record_t), &erased);
        if (result != LS_OK) {
            return result;
        }
        if (erased) {
            *slot_out = slot;
            return LS_OK;
        }
    }

    return LS_ENOSPACE;
}

ls_result_t ls_spool_append(const uint8_t *data, size_t length, ls_priority_t priority) {
    if (!data || !length || length > LS_MAX_EVENT_SIZE || priority > LS_PRIORITY_DIAGNOSTIC) {
        return LS_EINVAL;
    }
    if (!usable()) {
        return ls_runtime.storage ? LS_ENOSPACE : LS_OK;
    }

    ls_envelope_info_t info;
    if (ls_envelope_validate(data, length, &info) != LS_OK) {
        return LS_ECORRUPT;
    }

    size_t slot = 0u;
    ls_result_t result = find_empty_slot(priority, &slot);
    if (result != LS_OK) {
        if (result == LS_ENOSPACE) {
            ls_runtime.spool_dropped_records++;
        }
        return result;
    }

    spool_record_t record;
    ls_memset(&record, 0xff, sizeof(record));
    record.magic = SPOOL_RECORD_MAGIC;
    record.version = SPOOL_VERSION;
    record.length = (uint16_t)length;
    record.sequence = info.sequence;
    record.data_crc = ls_crc32(data, length);
    record.priority = (uint8_t)priority;
    record.event_type = info.type;
    record.header_crc = spool_record_crc(&record);

    size_t offset = slot_offset(slot);
    result = ls_fault_injection_hit("spool.before_header");
    if (result == LS_OK) {
        result = ls_storage_program(ls_runtime.storage, offset, &record, sizeof(record));
    }
    if (result == LS_OK) {
        result = ls_fault_injection_hit("spool.before_payload");
    }
    if (result == LS_OK) {
        result = ls_storage_program(ls_runtime.storage, offset + sizeof(record), data, length);
    }
    if (result == LS_OK) {
        result = storage_sync();
    }
    if (result == LS_OK) {
        result = ls_fault_injection_hit("spool.before_commit");
    }
    if (result == LS_OK) {
        const uint8_t committed = SPOOL_RECORD_COMMITTED;
        result = ls_storage_program(ls_runtime.storage, offset + offsetof(spool_record_t, state),
                                    &committed, sizeof(committed));
    }
    if (result == LS_OK) {
        result = storage_sync();
    }

    if (result == LS_OK) {
        size_t committed = 0u;
        for (size_t scan = 0u; scan < LS_SPOOL_MAX_RECORDS; scan++) {
            spool_record_t current;
            if (ls_runtime.storage->read(ls_runtime.storage->context, slot_offset(scan), &current,
                                         sizeof(current)) != LS_OK) {
                break;
            }
            if (record_valid(&current) && current.state == SPOOL_RECORD_COMMITTED) {
                committed++;
            }
        }
        if (committed > ls_runtime.spool_high_watermark) {
            ls_runtime.spool_high_watermark = committed;
        }
    }

    return result;
}

static ls_result_t set_record_state(size_t slot, uint8_t state) {
    return ls_storage_program(ls_runtime.storage,
                              slot_offset(slot) + offsetof(spool_record_t, state), &state,
                              sizeof(state));
}

static ls_result_t acknowledge_record(size_t slot) {
    ls_result_t result = set_record_state(slot, SPOOL_RECORD_ACKED);
    if (result == LS_OK) {
        result = storage_sync();
    }

    return result;
}

static ls_result_t increment_retries(size_t slot, uint8_t retry_bits) {
    uint8_t updated = retry_bits;
    for (uint8_t bit = 0u; bit < 8u; bit++) {
        uint8_t mask = (uint8_t)(1u << bit);
        if (updated & mask) {
            updated = (uint8_t)(updated & (uint8_t)~mask);
            break;
        }
    }

    if (updated == retry_bits) {
        ls_runtime.spool_retry_saturated++;
        return LS_OK;
    }

    ls_result_t result = ls_storage_program(
        ls_runtime.storage, slot_offset(slot) + offsetof(spool_record_t, retry_bits), &updated,
        sizeof(updated));
    if (result == LS_OK) {
        result = storage_sync();
    }

    return result;
}

static ls_result_t reclaim_if_drained(void) {
    int has_consumed_slot = 0;
    for (size_t slot = 0; slot < LS_SPOOL_MAX_RECORDS; slot++) {
        spool_record_t record;
        ls_result_t result = ls_runtime.storage->read(ls_runtime.storage->context,
                                                      slot_offset(slot), &record, sizeof(record));
        if (result != LS_OK) {
            return result;
        }
        if (record_valid(&record) && record.state == SPOOL_RECORD_COMMITTED) {
            return LS_OK;
        }
        if (!record_is_erased(&record)) {
            has_consumed_slot = 1;
        }
    }

    if (!has_consumed_slot) {
        return LS_OK;
    }

    ls_result_t result = erase_spool();
    if (result != LS_OK) {
        return result;
    }

    return write_header();
}

ls_result_t ls_spool_flush(void) {
    if (!usable()) {
        return ls_runtime.storage ? LS_ENOSPACE : LS_OK;
    }

    /* Always drain the most important durable record first. This matters for
     * disconnected vehicles that surface with a large backlog: crash evidence
     * should not wait behind routine diagnostics. */
    for (size_t pass = 0u; pass < LS_SPOOL_MAX_RECORDS; ++pass) {
        bool found = false;
        size_t selected_slot = 0u;
        spool_record_t selected = {0};
        for (size_t slot = 0u; slot < LS_SPOOL_MAX_RECORDS; ++slot) {
            spool_record_t record;
            ls_result_t read_result = ls_runtime.storage->read(
                ls_runtime.storage->context, slot_offset(slot), &record, sizeof(record));
            if (read_result != LS_OK) {
                return read_result;
            }
            if (!record_valid(&record) || record.state != SPOOL_RECORD_COMMITTED) {
                continue;
            }
            if (!found || record.priority < selected.priority ||
                (record.priority == selected.priority &&
                 (int32_t)(record.sequence - selected.sequence) < 0)) {
                selected = record;
                selected_slot = slot;
                found = true;
            }
        }
        if (!found) {
            break;
        }

        uint8_t data[LS_MAX_EVENT_SIZE];
        ls_result_t result = ls_runtime.storage->read(
            ls_runtime.storage->context, slot_offset(selected_slot) + sizeof(selected), data,
            selected.length);
        if (result != LS_OK || selected.data_crc != ls_crc32(data, selected.length)) {
            ls_runtime.spool_corrupt_records++;
            result = acknowledge_record(selected_slot);
            if (result != LS_OK) {
                return result;
            }
            continue;
        }

        ls_envelope_info_t info;
        if (ls_envelope_validate(data, selected.length, &info) != LS_OK) {
            ls_runtime.spool_corrupt_records++;
            result = acknowledge_record(selected_slot);
            if (result != LS_OK) {
                return result;
            }
            continue;
        }

        ls_transport_backend_t *transport =
            ls_transport_select((ls_priority_t)selected.priority, selected.length);
        if (!transport) {
            return LS_EAGAIN;
        }

        result = ls_fault_injection_hit("spool.before_send");
        if (result == LS_OK) {
            result = ls_transport_send(transport, info.event_id, data, selected.length);
        }
        if (result != LS_OK) {
            ls_runtime.spool_transport_failures++;
            ls_result_t retry_result = increment_retries(selected_slot, selected.retry_bits);
            return retry_result == LS_OK ? result : retry_result;
        }

        result = ls_fault_injection_hit("spool.before_ack");
        if (result == LS_OK) {
            result = acknowledge_record(selected_slot);
        }
        if (result != LS_OK) {
            return result;
        }
    }

    return reclaim_if_drained();
}

ls_result_t ls_flush(void) {
    return ls_spool_flush();
}

ls_result_t ls_spool_get_stats(ls_spool_stats_t *stats) {
    if (!stats) {
        return LS_EINVAL;
    }

    *stats = (ls_spool_stats_t){
        .capacity = LS_SPOOL_MAX_RECORDS,
        .reserved_critical = LS_SPOOL_RESERVED_CRITICAL,
        .reserved_emergency = LS_SPOOL_RESERVED_EMERGENCY,
        .high_watermark = ls_runtime.spool_high_watermark,
        .dropped_records = ls_runtime.spool_dropped_records,
        .corrupt_records = ls_runtime.spool_corrupt_records,
        .transport_failures = ls_runtime.spool_transport_failures,
        .retry_saturated = ls_runtime.spool_retry_saturated,
    };

    if (!usable()) {
        return ls_runtime.storage ? LS_ENOSPACE : LS_OK;
    }

    for (size_t slot = 0u; slot < LS_SPOOL_MAX_RECORDS; slot++) {
        spool_record_t record;
        ls_result_t result = ls_runtime.storage->read(ls_runtime.storage->context,
                                                      slot_offset(slot), &record, sizeof(record));
        if (result != LS_OK) {
            return result;
        }
        if (record_valid(&record) && record.state == SPOOL_RECORD_COMMITTED) {
            stats->committed++;
        }
    }
    if (stats->committed > stats->high_watermark) {
        stats->high_watermark = stats->committed;
        ls_runtime.spool_high_watermark = stats->committed;
    }
    return LS_OK;
}
