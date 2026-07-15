#include "../core/internal.h"

#include "laststate/flash_storage.h"

#define FLASH_MAGIC 0x464d534cu
#define FLASH_VERSION 2u
#define FLASH_COMMITTED 0xa5u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t generation;
    uint32_t logical_capacity;
    uint32_t data_crc;
    uint32_t header_crc;
} flash_header_t;

_Static_assert(sizeof(flash_header_t) == 24u, "flash header layout changed");

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

static bool flash_layout(size_t logical_capacity, size_t erase_size, size_t write_size,
                         size_t *payload_offset, size_t *commit_offset, size_t *bank_size) {
    if (!logical_capacity || !erase_size || !write_size || write_size > erase_size ||
        erase_size % write_size || write_size > LS_STORAGE_MAX_WRITE_SIZE) {
        return false;
    }

    size_t commit;
    size_t metadata_end;
    size_t payload;
    size_t bank;
    if (!align_size(sizeof(flash_header_t), write_size, &commit) ||
        !add_size(commit, write_size, &metadata_end) ||
        !align_size(metadata_end, write_size, &payload) ||
        !add_size(payload, logical_capacity, &bank) || !align_size(bank, erase_size, &bank)) {
        return false;
    }

    *payload_offset = payload;
    *commit_offset = commit;
    *bank_size = bank;
    return true;
}

size_t ls_flash_mirror_physical_size(size_t logical_capacity, size_t erase_size) {
    return ls_flash_mirror_physical_size_for_write(logical_capacity, erase_size, 1u);
}

size_t ls_flash_mirror_physical_size_for_write(size_t logical_capacity, size_t erase_size,
                                               size_t write_size) {
    size_t payload_offset;
    size_t commit_offset;
    size_t bank_size;
    size_t physical_size;
    if (!write_size) {
        write_size = 1u;
    }
    if (!flash_layout(logical_capacity, erase_size, write_size, &payload_offset, &commit_offset,
                      &bank_size) ||
        !add_size(bank_size, bank_size, &physical_size)) {
        return 0;
    }

    return physical_size;
}

static size_t bank_offset(const ls_flash_mirror_t *mirror, uint8_t bank) {
    return (size_t)bank * mirror->bank_size;
}

static uint32_t header_crc(const flash_header_t *header) {
    return ls_crc32(header, offsetof(flash_header_t, header_crc));
}

static bool generation_is_newer(uint32_t candidate, uint32_t reference) {
    return (int32_t)(candidate - reference) > 0;
}

static ls_result_t raw_read(const ls_flash_mirror_t *mirror, size_t offset, void *destination,
                            size_t length) {
    return mirror->raw->read(mirror->raw->context, offset, destination, length);
}

static ls_result_t raw_sync(const ls_flash_mirror_t *mirror) {
    return mirror->raw->sync ? mirror->raw->sync(mirror->raw->context) : LS_OK;
}

static bool valid_header(const ls_flash_mirror_t *mirror, const flash_header_t *header) {
    return header->magic == FLASH_MAGIC && header->version == FLASH_VERSION &&
           header->logical_capacity == mirror->logical_capacity &&
           header->header_crc == header_crc(header);
}

static ls_result_t valid_bank(ls_flash_mirror_t *mirror, uint8_t bank, flash_header_t *header,
                              int *valid) {
    size_t offset = bank_offset(mirror, bank);
    uint8_t committed = 0xffu;
    if (!valid) {
        return LS_EINVAL;
    }
    *valid = 0;

    ls_result_t result = raw_read(mirror, offset, header, sizeof(*header));
    if (result != LS_OK) {
        return result;
    }
    if (!valid_header(mirror, header)) {
        return LS_OK;
    }

    result = raw_read(mirror, offset + mirror->commit_offset, &committed, sizeof(committed));
    if (result != LS_OK) {
        return result;
    }
    if (committed != FLASH_COMMITTED) {
        return LS_OK;
    }

    result = raw_read(mirror, offset + mirror->payload_offset, mirror->workspace,
                      mirror->logical_capacity);
    if (result != LS_OK) {
        return result;
    }

    *valid = header->data_crc == ls_crc32(mirror->workspace, mirror->logical_capacity);
    return LS_OK;
}

static ls_result_t write_commit_marker(ls_flash_mirror_t *mirror, size_t offset) {
    const uint8_t committed = FLASH_COMMITTED;
    return ls_storage_program(mirror->raw, offset + mirror->commit_offset, &committed,
                              sizeof(committed));
}

static ls_result_t commit_image(ls_flash_mirror_t *mirror, uint8_t bank, uint32_t generation) {
    size_t offset = bank_offset(mirror, bank);
    ls_result_t result = mirror->raw->erase(mirror->raw->context, offset, mirror->bank_size);
    if (result != LS_OK) {
        return result;
    }

    flash_header_t header = {
        .magic = FLASH_MAGIC,
        .version = FLASH_VERSION,
        .generation = generation,
        .logical_capacity = (uint32_t)mirror->logical_capacity,
        .data_crc = ls_crc32(mirror->workspace, mirror->logical_capacity),
        .header_crc = 0,
    };
    header.header_crc = header_crc(&header);

    result = ls_storage_program(mirror->raw, offset, &header, sizeof(header));
    if (result == LS_OK) {
        result = ls_storage_program(mirror->raw, offset + mirror->payload_offset, mirror->workspace,
                                    mirror->logical_capacity);
    }
    if (result == LS_OK) {
        result = raw_sync(mirror);
    }
    if (result == LS_OK) {
        result = write_commit_marker(mirror, offset);
    }
    if (result == LS_OK) {
        result = raw_sync(mirror);
    }
    if (result == LS_OK) {
        mirror->active_bank = bank;
        mirror->generation = generation;
    }

    return result;
}

static ls_result_t mirror_read(void *context, size_t offset, void *destination, size_t length) {
    ls_flash_mirror_t *mirror = (ls_flash_mirror_t *)context;
    if (!mirror || (!destination && length) || offset > mirror->logical_capacity ||
        length > mirror->logical_capacity - offset) {
        return LS_EINVAL;
    }

    return raw_read(mirror,
                    bank_offset(mirror, mirror->active_bank) + mirror->payload_offset + offset,
                    destination, length);
}

static ls_result_t read_active_image(ls_flash_mirror_t *mirror) {
    return raw_read(mirror, bank_offset(mirror, mirror->active_bank) + mirror->payload_offset,
                    mirror->workspace, mirror->logical_capacity);
}

static ls_result_t update_image(ls_flash_mirror_t *mirror, size_t offset, const void *source,
                                size_t length, bool erase) {
    if (!mirror || (!erase && !source && length) || offset > mirror->logical_capacity ||
        length > mirror->logical_capacity - offset) {
        return LS_EINVAL;
    }

    ls_result_t result = read_active_image(mirror);
    if (result != LS_OK) {
        return result;
    }

    if (erase) {
        ls_memset(mirror->workspace + offset, 0xff, length);
    } else if (length) {
        ls_memcpy(mirror->workspace + offset, source, length);
    }

    return commit_image(mirror, (uint8_t)(mirror->active_bank ^ 1u), mirror->generation + 1u);
}

static ls_result_t mirror_write(void *context, size_t offset, const void *source, size_t length) {
    return update_image((ls_flash_mirror_t *)context, offset, source, length, false);
}

static ls_result_t mirror_erase(void *context, size_t offset, size_t length) {
    return update_image((ls_flash_mirror_t *)context, offset, NULL, length, true);
}

static ls_result_t mirror_sync(void *context) {
    ls_flash_mirror_t *mirror = (ls_flash_mirror_t *)context;
    if (!mirror) {
        return LS_EINVAL;
    }

    return raw_sync(mirror);
}

ls_result_t ls_flash_mirror_init(ls_flash_mirror_t *mirror, ls_storage_backend_t *raw,
                                 uint8_t *workspace, size_t logical_capacity) {
    if (!mirror || !raw || !raw->read || !raw->write || !raw->erase || !workspace ||
        !logical_capacity || logical_capacity > UINT32_MAX) {
        return LS_EINVAL;
    }

    size_t write_size = raw->write_size ? raw->write_size : 1u;
    size_t payload_offset;
    size_t commit_offset;
    size_t bank_size;
    if (!flash_layout(logical_capacity, raw->erase_size, write_size, &payload_offset,
                      &commit_offset, &bank_size)) {
        return LS_EINVAL;
    }
    if (bank_size > SIZE_MAX / 2u || raw->capacity < bank_size * 2u) {
        return LS_ENOSPACE;
    }

    ls_memset(mirror, 0, sizeof(*mirror));
    mirror->raw = raw;
    mirror->workspace = workspace;
    mirror->logical_capacity = logical_capacity;
    mirror->bank_size = bank_size;
    mirror->payload_offset = payload_offset;
    mirror->commit_offset = commit_offset;

    flash_header_t first;
    flash_header_t second;
    int first_valid = 0;
    int second_valid = 0;
    ls_result_t result = valid_bank(mirror, 0, &first, &first_valid);
    if (result != LS_OK) {
        return result;
    }
    result = valid_bank(mirror, 1, &second, &second_valid);
    if (result != LS_OK) {
        return result;
    }
    if (first_valid || second_valid) {
        if (second_valid &&
            (!first_valid || generation_is_newer(second.generation, first.generation))) {
            mirror->active_bank = 1;
            mirror->generation = second.generation;
        } else {
            mirror->active_bank = 0;
            mirror->generation = first.generation;
        }
    } else {
        ls_memset(workspace, 0xff, logical_capacity);
        result = commit_image(mirror, 0, 1u);
        if (result != LS_OK) {
            return result;
        }
    }

    mirror->backend = (ls_storage_backend_t){
        .name = "flash-mirror",
        .context = mirror,
        .capacity = logical_capacity,
        .erase_size = 1u,
        .write_size = 1u,
        .read = mirror_read,
        .write = mirror_write,
        .erase = mirror_erase,
        .sync = mirror_sync,
    };
    return LS_OK;
}
