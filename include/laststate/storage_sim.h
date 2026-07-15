#ifndef LASTSTATE_STORAGE_SIM_H
#define LASTSTATE_STORAGE_SIM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "storage.h"
typedef struct {
    uint8_t *data;
    size_t size;
    bool enforce_nor;
    uint32_t operation_count, fail_operation;
    size_t partial_bytes;
} ls_storage_sim_t;
void ls_storage_sim_reset_faults(ls_storage_sim_t *simulator);
void ls_storage_sim_fail_at(ls_storage_sim_t *simulator, uint32_t operation, size_t partial_bytes);
ls_result_t ls_storage_sim_read(void *context, size_t offset, void *dst, size_t length);
ls_result_t ls_storage_sim_write(void *context, size_t offset, const void *src, size_t length);
ls_result_t ls_storage_sim_erase(void *context, size_t offset, size_t length);
ls_result_t ls_storage_sim_sync(void *context);
#endif
