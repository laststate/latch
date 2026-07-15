#ifndef LASTSTATE_FLASH_STORAGE_H
#define LASTSTATE_FLASH_STORAGE_H
#include <stddef.h>
#include <stdint.h>
#include "storage.h"
typedef struct {ls_storage_backend_t backend;ls_storage_backend_t *raw;uint8_t *workspace;size_t logical_capacity,bank_size;uint32_t generation;uint8_t active_bank;} ls_flash_mirror_t;
size_t ls_flash_mirror_physical_size(size_t logical_capacity,size_t erase_size);
ls_result_t ls_flash_mirror_init(ls_flash_mirror_t *mirror,ls_storage_backend_t *raw,uint8_t *workspace,size_t logical_capacity);
#define LS_FLASH_WEAR_MAX_SLOTS 32u
typedef struct {
    ls_storage_backend_t backend;
    ls_storage_backend_t *raw;
    uint8_t *workspace;
    size_t logical_capacity,slot_size,slot_count;
    uint32_t generation,erase_counts[LS_FLASH_WEAR_MAX_SLOTS],failed_commits;
    uint8_t active_slot;
} ls_flash_wear_level_t;
typedef struct {uint32_t generation,total_erases,minimum_erases,maximum_erases,failed_commits;size_t slots;uint8_t active_slot;} ls_flash_wear_stats_t;
size_t ls_flash_wear_physical_size(size_t logical_capacity,size_t erase_size,size_t slots);
ls_result_t ls_flash_wear_init(ls_flash_wear_level_t *wear,ls_storage_backend_t *raw,uint8_t *workspace,size_t logical_capacity,size_t slots);
ls_flash_wear_stats_t ls_flash_wear_stats(const ls_flash_wear_level_t *wear);
#endif
