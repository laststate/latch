#ifndef LASTSTATE_MEMORY_H
#define LASTSTATE_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "event.h"

typedef enum {
    LS_CAPTURE_METADATA,
    LS_CAPTURE_SNAPSHOT,
    LS_CAPTURE_STACK,
    LS_CAPTURE_SELECTIVE,
    LS_CAPTURE_FULL
} ls_capture_level_t;
typedef enum { LS_REDACT_ZERO, LS_REDACT_HASH, LS_REDACT_EXCLUDE } ls_redaction_mode_t;
typedef struct {
    uint32_t free_bytes, minimum_free_bytes, largest_block, allocation_failures, pool_exhaustions;
} ls_heap_stats_t;

ls_result_t ls_dump_region_register(const char *name, const void *address, size_t length,
                                    uint32_t flags);
void ls_memory_exclude(const void *address, size_t length);
ls_result_t ls_memory_redact(const void *address, size_t length, ls_redaction_mode_t mode);
ls_result_t ls_stack_bounds_set(const void *lower, const void *upper);
ls_result_t ls_capture_coredump(ls_capture_level_t level);
void ls_memory_allocation_failed(size_t size);
void ls_pool_exhausted(const char *pool);
void ls_heap_stats_update(const ls_heap_stats_t *stats);
ls_heap_stats_t ls_heap_stats_get(void);
#endif
