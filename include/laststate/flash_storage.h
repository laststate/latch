#ifndef LASTSTATE_FLASH_STORAGE_H
#define LASTSTATE_FLASH_STORAGE_H

#include <stddef.h>
#include <stdint.h>

#include "storage.h"

typedef struct {
    ls_storage_backend_t backend;
    ls_storage_backend_t *raw;
    uint8_t *workspace;
    size_t logical_capacity;
    size_t bank_size;
    size_t payload_offset;
    size_t commit_offset;
    uint32_t generation;
    uint8_t active_bank;
} ls_flash_mirror_t;

size_t ls_flash_mirror_physical_size(size_t logical_capacity, size_t erase_size);
size_t ls_flash_mirror_physical_size_for_write(size_t logical_capacity, size_t erase_size,
                                               size_t write_size);
ls_result_t ls_flash_mirror_init(ls_flash_mirror_t *mirror, ls_storage_backend_t *raw,
                                 uint8_t *workspace, size_t logical_capacity);

#define LS_FLASH_WEAR_MAX_SLOTS 32u

typedef struct {
    ls_storage_backend_t backend;
    ls_storage_backend_t *raw;
    uint8_t *workspace;
    size_t logical_capacity;
    size_t slot_size;
    size_t payload_offset;
    size_t commit_offset;
    size_t slot_count;
    uint32_t generation;
    uint32_t erase_counts[LS_FLASH_WEAR_MAX_SLOTS];
    uint32_t failed_commits;
    uint8_t active_slot;
} ls_flash_wear_level_t;

typedef struct {
    uint32_t generation;
    uint32_t total_erases;
    uint32_t minimum_erases;
    uint32_t maximum_erases;
    uint32_t failed_commits;
    size_t slots;
    uint8_t active_slot;
} ls_flash_wear_stats_t;

/* Conservative endurance telemetry. `estimated_commits_remaining` is the
 * aggregate remaining erase budget assuming the current wear-level algorithm
 * continues to erase one slot per committed logical update. Datasheet erase
 * ratings do not account for temperature, retention or radiation; product
 * safety logic must apply its own derating. */
typedef struct {
    uint32_t rated_erase_cycles;
    uint32_t maximum_erases;
    uint16_t wear_permyriad;
    bool exhausted;
    uint64_t estimated_commits_remaining;
} ls_flash_wear_lifetime_t;

size_t ls_flash_wear_physical_size(size_t logical_capacity, size_t erase_size, size_t slots);
size_t ls_flash_wear_physical_size_for_write(size_t logical_capacity, size_t erase_size,
                                             size_t write_size, size_t slots);
ls_result_t ls_flash_wear_init(ls_flash_wear_level_t *wear, ls_storage_backend_t *raw,
                               uint8_t *workspace, size_t logical_capacity, size_t slots);
ls_flash_wear_stats_t ls_flash_wear_stats(const ls_flash_wear_level_t *wear);
ls_result_t ls_flash_wear_lifetime(const ls_flash_wear_level_t *wear, uint32_t rated_erase_cycles,
                                   ls_flash_wear_lifetime_t *lifetime);

#endif
