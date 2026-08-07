#include "internal.h"
#include "laststate/blackbox.h"
#include "laststate/noinit.h"

#define LS_BLACKBOX_MAGIC 0x5842424cu /* LBBX */
#define LS_BLACKBOX_VERSION 1u

typedef struct {
    uint32_t sequence;
    ls_blackbox_record_t record;
    uint32_t crc;
} ls_blackbox_retained_record_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t next_sequence;
    uint32_t next_sequence_inv;
    uint32_t total_records;
    uint32_t total_records_inv;
    uint32_t overwritten_records;
    uint32_t overwritten_records_inv;
    uint32_t anomaly_until_ms;
    uint32_t anomaly_until_ms_inv;
    uint8_t frozen;
    uint8_t frozen_inv;
    uint8_t profile;
    uint8_t profile_inv;
    ls_blackbox_retained_record_t records[LS_BLACKBOX_CAPACITY];
} ls_blackbox_retained_t;

static LS_NOINIT volatile ls_blackbox_retained_t retained_blackbox;
static ls_blackbox_profile_t previous_profile = LS_BLACKBOX_PROFILE_NORMAL;

static uint32_t record_crc(const ls_blackbox_retained_record_t *record) {
    return ls_crc32(record, offsetof(ls_blackbox_retained_record_t, crc));
}

static bool record_valid(const volatile ls_blackbox_retained_record_t *record) {
    ls_blackbox_retained_record_t copy;
    ls_memcpy(&copy, (const void *)record, sizeof(copy));
    return copy.sequence != 0u && copy.crc == record_crc(&copy);
}

static void metadata_store_u32(volatile uint32_t *value, volatile uint32_t *inverse,
                               uint32_t updated) {
    *value = updated;
    *inverse = ~updated;
}

static bool metadata_u32_valid(volatile const uint32_t *value,
                               volatile const uint32_t *inverse) {
    return *inverse == ~(*value);
}

static void metadata_store_u8(volatile uint8_t *value, volatile uint8_t *inverse,
                              uint8_t updated) {
    *value = updated;
    *inverse = (uint8_t)~updated;
}

static bool metadata_u8_valid(volatile const uint8_t *value, volatile const uint8_t *inverse) {
    return *inverse == (uint8_t)~(*value);
}

static void blackbox_reset(void) {
    ls_memset((void *)&retained_blackbox, 0, sizeof(retained_blackbox));
    retained_blackbox.magic = LS_BLACKBOX_MAGIC;
    retained_blackbox.version = LS_BLACKBOX_VERSION;
    metadata_store_u32(&retained_blackbox.next_sequence, &retained_blackbox.next_sequence_inv, 1u);
    metadata_store_u32(&retained_blackbox.total_records, &retained_blackbox.total_records_inv, 0u);
    metadata_store_u32(&retained_blackbox.overwritten_records,
                       &retained_blackbox.overwritten_records_inv, 0u);
    metadata_store_u32(&retained_blackbox.anomaly_until_ms,
                       &retained_blackbox.anomaly_until_ms_inv, 0u);
    metadata_store_u8(&retained_blackbox.frozen, &retained_blackbox.frozen_inv, 0u);
    metadata_store_u8(&retained_blackbox.profile, &retained_blackbox.profile_inv,
                      (uint8_t)LS_BLACKBOX_PROFILE_NORMAL);
    previous_profile = LS_BLACKBOX_PROFILE_NORMAL;
}

void ls_blackbox_init(void) {
    if (retained_blackbox.magic != LS_BLACKBOX_MAGIC ||
        retained_blackbox.version != LS_BLACKBOX_VERSION) {
        blackbox_reset();
        return;
    }

    uint32_t highest = 0u;
    size_t valid = 0u;
    for (size_t index = 0u; index < LS_BLACKBOX_CAPACITY; ++index) {
        if (!record_valid(&retained_blackbox.records[index])) {
            continue;
        }
        ls_blackbox_retained_record_t copy;
        ls_memcpy(&copy, (const void *)&retained_blackbox.records[index], sizeof(copy));
        valid++;
        if ((int32_t)(copy.sequence - highest) > 0) {
            highest = copy.sequence;
        }
    }

    if (!metadata_u32_valid(&retained_blackbox.next_sequence,
                            &retained_blackbox.next_sequence_inv) ||
        retained_blackbox.next_sequence == 0u) {
        metadata_store_u32(&retained_blackbox.next_sequence, &retained_blackbox.next_sequence_inv,
                           highest + 1u ? highest + 1u : 1u);
    }
    if (!metadata_u32_valid(&retained_blackbox.total_records,
                            &retained_blackbox.total_records_inv)) {
        metadata_store_u32(&retained_blackbox.total_records, &retained_blackbox.total_records_inv,
                           highest > (uint32_t)valid ? highest : (uint32_t)valid);
    }
    if (!metadata_u32_valid(&retained_blackbox.overwritten_records,
                            &retained_blackbox.overwritten_records_inv)) {
        uint32_t total = retained_blackbox.total_records;
        uint32_t overwritten = total > LS_BLACKBOX_CAPACITY ? total - LS_BLACKBOX_CAPACITY : 0u;
        metadata_store_u32(&retained_blackbox.overwritten_records,
                           &retained_blackbox.overwritten_records_inv, overwritten);
    }
    if (!metadata_u32_valid(&retained_blackbox.anomaly_until_ms,
                            &retained_blackbox.anomaly_until_ms_inv)) {
        metadata_store_u32(&retained_blackbox.anomaly_until_ms,
                           &retained_blackbox.anomaly_until_ms_inv, 0u);
    }
    if (!metadata_u8_valid(&retained_blackbox.frozen, &retained_blackbox.frozen_inv)) {
        metadata_store_u8(&retained_blackbox.frozen, &retained_blackbox.frozen_inv, 0u);
    }
    if (!metadata_u8_valid(&retained_blackbox.profile, &retained_blackbox.profile_inv) ||
        retained_blackbox.profile > LS_BLACKBOX_PROFILE_ANOMALY) {
        metadata_store_u8(&retained_blackbox.profile, &retained_blackbox.profile_inv,
                          (uint8_t)LS_BLACKBOX_PROFILE_NORMAL);
    }
    previous_profile = (ls_blackbox_profile_t)retained_blackbox.profile;
}

static bool profile_accepts(const ls_blackbox_record_t *record) {
    if (retained_blackbox.profile != LS_BLACKBOX_PROFILE_QUIET) {
        return true;
    }
    return (record->flags & (LS_BLACKBOX_IMPORTANT | LS_BLACKBOX_ERROR)) != 0u ||
           record->kind == LS_BLACKBOX_WATCHDOG || record->kind == LS_BLACKBOX_POWER;
}

ls_result_t ls_blackbox_record(const ls_blackbox_record_t *record) {
    if (!record || record->kind < LS_BLACKBOX_STATE || record->kind > LS_BLACKBOX_USER) {
        return LS_EINVAL;
    }
    if (retained_blackbox.magic != LS_BLACKBOX_MAGIC) {
        ls_blackbox_init();
    }

    /* Normal-runtime multi-producer serialization is delegated to the same
     * critical-section callbacks used by the rest of Latch. Per-record CRCs
     * still protect against reset/power loss in the middle of the write. */
    ls_enter_critical();
    if (retained_blackbox.frozen) {
        ls_leave_critical();
        return LS_EBUSY;
    }
    if (!profile_accepts(record)) {
        ls_leave_critical();
        return LS_OK;
    }

    uint32_t sequence = retained_blackbox.next_sequence;
    if (sequence == 0u) {
        sequence = 1u;
    }
    size_t slot = (size_t)((sequence - 1u) % LS_BLACKBOX_CAPACITY);
    ls_blackbox_retained_record_t stored;
    ls_memset(&stored, 0, sizeof(stored));
    stored.sequence = sequence;
    stored.record = *record;
    if (!stored.record.timestamp_ms) {
        stored.record.timestamp_ms = ls_runtime.initialized ? ls_uptime_ms() : 0u;
    }
    stored.crc = record_crc(&stored);
    ls_memcpy((void *)&retained_blackbox.records[slot], &stored, sizeof(stored));

    uint32_t total = retained_blackbox.total_records;
    if (total != UINT32_MAX) {
        total++;
    }
    metadata_store_u32(&retained_blackbox.total_records, &retained_blackbox.total_records_inv,
                       total);
    if (total > LS_BLACKBOX_CAPACITY) {
        uint32_t overwritten = retained_blackbox.overwritten_records;
        if (overwritten != UINT32_MAX) {
            overwritten++;
        }
        metadata_store_u32(&retained_blackbox.overwritten_records,
                           &retained_blackbox.overwritten_records_inv, overwritten);
    }
    uint32_t next = sequence == UINT32_MAX ? 1u : sequence + 1u;
    metadata_store_u32(&retained_blackbox.next_sequence, &retained_blackbox.next_sequence_inv,
                       next);
    ls_leave_critical();
    return LS_OK;
}

ls_result_t ls_blackbox_record_values(ls_blackbox_kind_t kind, uint16_t source_id, uint16_t flags,
                                      int32_t v0, int32_t v1, int32_t v2, int32_t v3) {
    ls_blackbox_record_t record = {
        .timestamp_ms = ls_runtime.initialized ? ls_uptime_ms() : 0u,
        .kind = (uint16_t)kind,
        .source_id = source_id,
        .flags = flags,
        .reserved = 0u,
        .value = {v0, v1, v2, v3},
    };
    return ls_blackbox_record(&record);
}

void ls_blackbox_freeze(void) {
    if (retained_blackbox.magic == LS_BLACKBOX_MAGIC) {
        metadata_store_u8(&retained_blackbox.frozen, &retained_blackbox.frozen_inv, 1u);
    }
}

void ls_blackbox_thaw(void) {
    if (retained_blackbox.magic != LS_BLACKBOX_MAGIC) {
        ls_blackbox_init();
    }
    metadata_store_u8(&retained_blackbox.frozen, &retained_blackbox.frozen_inv, 0u);
}

void ls_blackbox_clear(void) {
    blackbox_reset();
}

void ls_blackbox_set_profile(ls_blackbox_profile_t profile) {
    if (profile > LS_BLACKBOX_PROFILE_ANOMALY) {
        return;
    }
    if (retained_blackbox.magic != LS_BLACKBOX_MAGIC) {
        ls_blackbox_init();
    }
    previous_profile = profile;
    metadata_store_u8(&retained_blackbox.profile, &retained_blackbox.profile_inv,
                      (uint8_t)profile);
    if (profile != LS_BLACKBOX_PROFILE_ANOMALY) {
        metadata_store_u32(&retained_blackbox.anomaly_until_ms,
                           &retained_blackbox.anomaly_until_ms_inv, 0u);
    }
}

void ls_blackbox_anomaly_begin(uint32_t duration_ms) {
    if (!duration_ms) {
        return;
    }
    if (retained_blackbox.magic != LS_BLACKBOX_MAGIC) {
        ls_blackbox_init();
    }
    if (retained_blackbox.profile != LS_BLACKBOX_PROFILE_ANOMALY) {
        previous_profile = (ls_blackbox_profile_t)retained_blackbox.profile;
    }
    metadata_store_u8(&retained_blackbox.profile, &retained_blackbox.profile_inv,
                      (uint8_t)LS_BLACKBOX_PROFILE_ANOMALY);
    uint32_t until = (ls_runtime.initialized ? ls_uptime_ms() : 0u) + duration_ms;
    metadata_store_u32(&retained_blackbox.anomaly_until_ms,
                       &retained_blackbox.anomaly_until_ms_inv, until);
}

void ls_blackbox_poll(void) {
    if (retained_blackbox.magic != LS_BLACKBOX_MAGIC ||
        retained_blackbox.profile != LS_BLACKBOX_PROFILE_ANOMALY ||
        retained_blackbox.anomaly_until_ms == 0u || !ls_runtime.initialized) {
        return;
    }
    if ((int32_t)(ls_uptime_ms() - retained_blackbox.anomaly_until_ms) >= 0) {
        metadata_store_u8(&retained_blackbox.profile, &retained_blackbox.profile_inv,
                          (uint8_t)previous_profile);
        metadata_store_u32(&retained_blackbox.anomaly_until_ms,
                           &retained_blackbox.anomaly_until_ms_inv, 0u);
    }
}

ls_result_t ls_blackbox_copy(ls_blackbox_record_t *records, size_t capacity, size_t *count) {
    if (!count || (!records && capacity)) {
        return LS_EINVAL;
    }
    if (retained_blackbox.magic != LS_BLACKBOX_MAGIC) {
        ls_blackbox_init();
    }

    uint32_t newest = 0u;
    size_t valid = 0u;
    for (size_t i = 0u; i < LS_BLACKBOX_CAPACITY; ++i) {
        if (record_valid(&retained_blackbox.records[i])) {
            ls_blackbox_retained_record_t copy;
            ls_memcpy(&copy, (const void *)&retained_blackbox.records[i], sizeof(copy));
            valid++;
            if ((int32_t)(copy.sequence - newest) > 0) {
                newest = copy.sequence;
            }
        }
    }
    size_t wanted = valid < capacity ? valid : capacity;
    *count = 0u;
    if (!wanted) {
        return LS_OK;
    }
    uint32_t first = newest >= (uint32_t)(wanted - 1u) ? newest - (uint32_t)(wanted - 1u) : 1u;
    for (uint32_t sequence = first; *count < wanted; ++sequence) {
        for (size_t slot = 0u; slot < LS_BLACKBOX_CAPACITY; ++slot) {
            if (!record_valid(&retained_blackbox.records[slot])) {
                continue;
            }
            ls_blackbox_retained_record_t copy;
            ls_memcpy(&copy, (const void *)&retained_blackbox.records[slot], sizeof(copy));
            if (copy.sequence == sequence) {
                records[(*count)++] = copy.record;
                break;
            }
        }
        if (sequence == newest) {
            break;
        }
    }
    return LS_OK;
}

ls_result_t ls_blackbox_get_recent(size_t age, ls_blackbox_record_t *record) {
    if (!record) {
        return LS_EINVAL;
    }
    if (retained_blackbox.magic != LS_BLACKBOX_MAGIC) {
        ls_blackbox_init();
    }

    uint32_t newest = 0u;
    size_t valid = 0u;
    for (size_t i = 0u; i < LS_BLACKBOX_CAPACITY; ++i) {
        if (!record_valid(&retained_blackbox.records[i])) {
            continue;
        }
        ls_blackbox_retained_record_t copy;
        ls_memcpy(&copy, (const void *)&retained_blackbox.records[i], sizeof(copy));
        valid++;
        if (newest == 0u || (int32_t)(copy.sequence - newest) > 0) {
            newest = copy.sequence;
        }
    }
    if (age >= valid || newest == 0u) {
        return LS_EAGAIN;
    }

    uint32_t target = newest;
    for (size_t step = 0u; step < age; ++step) {
        target = target == 1u ? UINT32_MAX : target - 1u;
    }
    for (size_t slot = 0u; slot < LS_BLACKBOX_CAPACITY; ++slot) {
        if (!record_valid(&retained_blackbox.records[slot])) {
            continue;
        }
        ls_blackbox_retained_record_t copy;
        ls_memcpy(&copy, (const void *)&retained_blackbox.records[slot], sizeof(copy));
        if (copy.sequence == target) {
            *record = copy.record;
            return LS_OK;
        }
    }
    return LS_EAGAIN;
}

ls_result_t ls_blackbox_get_stats(ls_blackbox_stats_t *stats) {
    if (!stats) {
        return LS_EINVAL;
    }
    if (retained_blackbox.magic != LS_BLACKBOX_MAGIC) {
        ls_blackbox_init();
    }
    size_t count = 0u;
    for (size_t i = 0u; i < LS_BLACKBOX_CAPACITY; ++i) {
        if (record_valid(&retained_blackbox.records[i])) {
            count++;
        }
    }
    *stats = (ls_blackbox_stats_t){
        .capacity = LS_BLACKBOX_CAPACITY,
        .count = count,
        .total_records = retained_blackbox.total_records,
        .overwritten_records = retained_blackbox.overwritten_records,
        .frozen = retained_blackbox.frozen != 0u,
        .profile = (ls_blackbox_profile_t)retained_blackbox.profile,
        .anomaly_until_ms = retained_blackbox.anomaly_until_ms,
    };
    return LS_OK;
}
