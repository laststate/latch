#include "../core/internal.h"

#include "laststate/flash_storage.h"

#define WEAR_MAGIC 0x574c534cu
#define WEAR_VERSION 2u
#define WEAR_COMMITTED 0xa5u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t generation;
    uint32_t erase_count;
    uint32_t logical_capacity;
    uint32_t data_crc;
    uint32_t header_crc;
} wear_header_t;

_Static_assert(sizeof(wear_header_t) == 28u, "wear header layout changed");

static bool add_size(size_t left, size_t right, size_t *result) {
    if (left > SIZE_MAX - right) {
        return false;
    }

    *result = left + right;
    return true;
}

static bool align_size(size_t value, size_t alignment, size_t *result) {
    if (!alignment) {
        return false;
    }

    size_t remainder = value % alignment;
    if (!remainder) {
        *result = value;
        return true;
    }

    return add_size(value, alignment - remainder, result);
}

static bool wear_layout(size_t logical_capacity, size_t erase_size, size_t write_size,
                        size_t *payload_offset, size_t *commit_offset, size_t *slot_size) {
    if (!logical_capacity || !erase_size || !write_size || write_size > erase_size ||
        erase_size % write_size || write_size > LS_STORAGE_MAX_WRITE_SIZE) {
        return false;
    }

    size_t commit;
    size_t metadata_end;
    size_t payload;
    size_t slot;
    if (!align_size(sizeof(wear_header_t), write_size, &commit) ||
        !add_size(commit, write_size, &metadata_end) ||
        !align_size(metadata_end, write_size, &payload) ||
        !add_size(payload, logical_capacity, &slot) || !align_size(slot, erase_size, &slot)) {
        return false;
    }

    *payload_offset = payload;
    *commit_offset = commit;
    *slot_size = slot;
    return true;
}

size_t ls_flash_wear_physical_size(size_t logical_capacity, size_t erase_size, size_t slots) {
    return ls_flash_wear_physical_size_for_write(logical_capacity, erase_size, 1u, slots);
}

size_t ls_flash_wear_physical_size_for_write(size_t logical_capacity, size_t erase_size,
                                             size_t write_size, size_t slots) {
    size_t payload_offset;
    size_t commit_offset;
    size_t slot_size;
    size_t total;
    if (!write_size) {
        write_size = 1u;
    }
    if (slots < 2u || slots > LS_FLASH_WEAR_MAX_SLOTS ||
        !wear_layout(logical_capacity, erase_size, write_size, &payload_offset, &commit_offset,
                     &slot_size) ||
        slot_size > SIZE_MAX / slots || !add_size(0u, slot_size * slots, &total)) {
        return 0;
    }

    return total;
}

static size_t slot_offset(const ls_flash_wear_level_t *wear, size_t slot) {
    return slot * wear->slot_size;
}

static uint32_t header_crc(const wear_header_t *header) {
    return ls_crc32(header, offsetof(wear_header_t, header_crc));
}

static bool generation_is_newer(uint32_t candidate, uint32_t reference) {
    return (int32_t)(candidate - reference) > 0;
}

static bool valid_header(const ls_flash_wear_level_t *wear, const wear_header_t *header) {
    return header->magic == WEAR_MAGIC && header->version == WEAR_VERSION &&
           header->logical_capacity == wear->logical_capacity &&
           header->header_crc == header_crc(header);
}

static ls_result_t valid_slot(ls_flash_wear_level_t *wear, size_t slot, wear_header_t *header,
                              int *valid) {
    size_t offset = slot_offset(wear, slot);
    uint8_t committed = 0xffu;
    if (!valid) {
        return LS_EINVAL;
    }
    *valid = 0;

    ls_result_t result = wear->raw->read(wear->raw->context, offset, header, sizeof(*header));
    if (result != LS_OK) {
        return result;
    }
    if (!valid_header(wear, header)) {
        return LS_OK;
    }

    result = wear->raw->read(wear->raw->context, offset + wear->commit_offset, &committed,
                             sizeof(committed));
    if (result != LS_OK) {
        return result;
    }
    if (committed != WEAR_COMMITTED) {
        return LS_OK;
    }

    result = wear->raw->read(wear->raw->context, offset + wear->payload_offset, wear->workspace,
                             wear->logical_capacity);
    if (result != LS_OK) {
        return result;
    }

    *valid = header->data_crc == ls_crc32(wear->workspace, wear->logical_capacity);
    return LS_OK;
}

static size_t next_slot(const ls_flash_wear_level_t *wear) {
    size_t selected = (wear->active_slot + 1u) % wear->slot_count;
    uint32_t minimum = UINT32_MAX;
    for (size_t step = 1u; step <= wear->slot_count; step++) {
        size_t slot = (wear->active_slot + step) % wear->slot_count;
        if (slot == wear->active_slot) {
            continue;
        }

        if (wear->erase_counts[slot] < minimum) {
            minimum = wear->erase_counts[slot];
            selected = slot;
        }
    }

    return selected;
}

static ls_result_t raw_sync(const ls_flash_wear_level_t *wear) {
    return wear->raw->sync ? wear->raw->sync(wear->raw->context) : LS_OK;
}

static ls_result_t commit(ls_flash_wear_level_t *wear, size_t slot, uint32_t generation) {
    if (wear->erase_counts[slot] == UINT32_MAX) {
        wear->failed_commits++;
        return LS_EOVERFLOW;
    }

    size_t offset = slot_offset(wear, slot);
    ls_result_t result = wear->raw->erase(wear->raw->context, offset, wear->slot_size);
    if (result != LS_OK) {
        wear->failed_commits++;
        return result;
    }

    wear->erase_counts[slot]++;
    wear_header_t header = {
        .magic = WEAR_MAGIC,
        .version = WEAR_VERSION,
        .generation = generation,
        .erase_count = wear->erase_counts[slot],
        .logical_capacity = (uint32_t)wear->logical_capacity,
        .data_crc = ls_crc32(wear->workspace, wear->logical_capacity),
        .header_crc = 0,
    };
    header.header_crc = header_crc(&header);

    result = ls_storage_program(wear->raw, offset, &header, sizeof(header));
    if (result == LS_OK) {
        result = ls_storage_program(wear->raw, offset + wear->payload_offset, wear->workspace,
                                    wear->logical_capacity);
    }
    if (result == LS_OK) {
        result = raw_sync(wear);
    }
    if (result == LS_OK) {
        const uint8_t committed = WEAR_COMMITTED;
        result = ls_storage_program(wear->raw, offset + wear->commit_offset, &committed,
                                    sizeof(committed));
    }
    if (result == LS_OK) {
        result = raw_sync(wear);
    }
    if (result == LS_OK) {
        wear->active_slot = (uint8_t)slot;
        wear->generation = generation;
    } else {
        wear->failed_commits++;
    }

    return result;
}

static ls_result_t read_active(ls_flash_wear_level_t *wear, void *destination) {
    return wear->raw->read(wear->raw->context,
                           slot_offset(wear, wear->active_slot) + wear->payload_offset, destination,
                           wear->logical_capacity);
}

static ls_result_t wear_read(void *context, size_t offset, void *destination, size_t length) {
    ls_flash_wear_level_t *wear = (ls_flash_wear_level_t *)context;
    if (!wear || (!destination && length) || offset > wear->logical_capacity ||
        length > wear->logical_capacity - offset) {
        return LS_EINVAL;
    }

    return wear->raw->read(wear->raw->context,
                           slot_offset(wear, wear->active_slot) + wear->payload_offset + offset,
                           destination, length);
}

static ls_result_t update(ls_flash_wear_level_t *wear, size_t offset, const void *source,
                          size_t length, bool erase) {
    if (!wear || (!erase && !source && length) || offset > wear->logical_capacity ||
        length > wear->logical_capacity - offset) {
        return LS_EINVAL;
    }

    ls_result_t result = read_active(wear, wear->workspace);
    if (result != LS_OK) {
        return result;
    }

    if (erase) {
        ls_memset(wear->workspace + offset, 0xff, length);
    } else if (length) {
        ls_memcpy(wear->workspace + offset, source, length);
    }

    return commit(wear, next_slot(wear), wear->generation + 1u);
}

static ls_result_t wear_write(void *context, size_t offset, const void *source, size_t length) {
    return update((ls_flash_wear_level_t *)context, offset, source, length, false);
}

static ls_result_t wear_erase(void *context, size_t offset, size_t length) {
    return update((ls_flash_wear_level_t *)context, offset, NULL, length, true);
}

static ls_result_t wear_sync(void *context) {
    ls_flash_wear_level_t *wear = (ls_flash_wear_level_t *)context;
    if (!wear) {
        return LS_EINVAL;
    }

    return raw_sync(wear);
}

ls_result_t ls_flash_wear_init(ls_flash_wear_level_t *wear, ls_storage_backend_t *raw,
                               uint8_t *workspace, size_t logical_capacity, size_t slots) {
    if (!wear || !raw || !raw->read || !raw->write || !raw->erase || !workspace ||
        !logical_capacity || logical_capacity > UINT32_MAX || slots < 2u ||
        slots > LS_FLASH_WEAR_MAX_SLOTS) {
        return LS_EINVAL;
    }

    size_t write_size = raw->write_size ? raw->write_size : 1u;
    size_t payload_offset;
    size_t commit_offset;
    size_t slot_size;
    if (!wear_layout(logical_capacity, raw->erase_size, write_size, &payload_offset, &commit_offset,
                     &slot_size)) {
        return LS_EINVAL;
    }
    if (slot_size > SIZE_MAX / slots || raw->capacity < slot_size * slots) {
        return LS_ENOSPACE;
    }

    ls_memset(wear, 0, sizeof(*wear));
    wear->raw = raw;
    wear->workspace = workspace;
    wear->logical_capacity = logical_capacity;
    wear->slot_size = slot_size;
    wear->payload_offset = payload_offset;
    wear->commit_offset = commit_offset;
    wear->slot_count = slots;

    bool found = false;
    wear_header_t newest = {0};
    for (size_t slot = 0; slot < slots; slot++) {
        wear_header_t header;
        ls_memset(&header, 0xff, sizeof(header));
        int valid = 0;
        ls_result_t result = valid_slot(wear, slot, &header, &valid);
        if (result != LS_OK) {
            return result;
        }
        if (valid_header(wear, &header)) {
            wear->erase_counts[slot] = header.erase_count;
        }
        if (valid && (!found || generation_is_newer(header.generation, newest.generation))) {
            found = true;
            newest = header;
            wear->active_slot = (uint8_t)slot;
            wear->generation = header.generation;
        }
    }

    if (!found) {
        ls_memset(workspace, 0xff, logical_capacity);
        ls_result_t result = commit(wear, 0u, 1u);
        if (result != LS_OK) {
            return result;
        }
    }

    wear->backend = (ls_storage_backend_t){
        .name = "flash-wear-level",
        .context = wear,
        .capacity = logical_capacity,
        .erase_size = 1u,
        .write_size = 1u,
        .read = wear_read,
        .write = wear_write,
        .erase = wear_erase,
        .sync = wear_sync,
    };
    return LS_OK;
}

ls_flash_wear_stats_t ls_flash_wear_stats(const ls_flash_wear_level_t *wear) {
    ls_flash_wear_stats_t stats = {0};
    if (!wear) {
        return stats;
    }

    stats.generation = wear->generation;
    stats.failed_commits = wear->failed_commits;
    stats.slots = wear->slot_count;
    stats.active_slot = wear->active_slot;
    stats.minimum_erases = UINT32_MAX;
    for (size_t slot = 0; slot < wear->slot_count; slot++) {
        uint32_t count = wear->erase_counts[slot];
        if (UINT32_MAX - stats.total_erases < count) {
            stats.total_erases = UINT32_MAX;
        } else {
            stats.total_erases += count;
        }
        if (count < stats.minimum_erases) {
            stats.minimum_erases = count;
        }
        if (count > stats.maximum_erases) {
            stats.maximum_erases = count;
        }
    }
    if (!wear->slot_count) {
        stats.minimum_erases = 0;
    }

    return stats;
}
