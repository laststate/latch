#include "internal.h"

ls_result_t ls_dump_region_register(const char *name, const void *address, size_t length,
                                    uint32_t flags) {
    if (!name || !address || !length || !(flags & LS_DUMP_SAFE) ||
        ls_runtime.dump_region_count >= LS_MAX_DUMP_REGIONS)
        return LS_EINVAL;
    ls_runtime.dump_regions[ls_runtime.dump_region_count++] =
        (ls_dump_region_t){name, address, length, flags};
    return LS_OK;
}
void ls_memory_exclude(const void *address, size_t length) {
    (void)ls_memory_redact(address, length, LS_REDACT_EXCLUDE);
}
ls_result_t ls_memory_redact(const void *address, size_t length, ls_redaction_mode_t mode) {
    if (!address || !length || ls_runtime.redaction_count >= LS_MAX_REDACTIONS)
        return LS_EINVAL;
    ls_runtime.redactions[ls_runtime.redaction_count++] =
        (ls_redaction_t){(const uint8_t *)address, length, mode};
    return LS_OK;
}
ls_redaction_mode_t ls_redaction_for(const void *address, size_t length, bool *matched) {
    uintptr_t start = (uintptr_t)address, end = start + length;
    if (matched)
        *matched = false;
    for (size_t i = 0; i < ls_runtime.redaction_count; i++) {
        uintptr_t red_start = (uintptr_t)ls_runtime.redactions[i].address,
                  red_end = red_start + ls_runtime.redactions[i].length;
        if (start < red_end && red_start < end) {
            if (matched)
                *matched = true;
            return ls_runtime.redactions[i].mode;
        }
    }
    return LS_REDACT_ZERO;
}
ls_result_t ls_stack_bounds_set(const void *lower, const void *upper) {
    if (!lower || !upper || (uintptr_t)lower >= (uintptr_t)upper)
        return LS_EINVAL;
    ls_runtime.stack_lower = (const uint8_t *)lower;
    ls_runtime.stack_upper = (const uint8_t *)upper;
    return LS_OK;
}
ls_result_t ls_capture_coredump(ls_capture_level_t level) {
    if (level < LS_CAPTURE_METADATA || level > LS_CAPTURE_FULL)
        return LS_EINVAL;
    ls_event_t event = {.type = LS_EVENT_COREDUMP,
                        .priority =
                            level == LS_CAPTURE_FULL ? LS_PRIORITY_ERROR : LS_PRIORITY_DIAGNOSTIC,
                        .timestamp_ms = ls_uptime_ms(),
                        .domain = "coredump",
                        .code = (int32_t)level,
                        .severity = LS_SEVERITY_INFO,
                        .message = "manual_coredump",
                        .capture_level = level};
    return ls_capture_event(&event);
}
void ls_memory_allocation_failed(size_t size) {
    ls_runtime.heap_stats.allocation_failures++;
    ls_error_t error = {"memory", (int32_t)size, LS_SEVERITY_ERROR, "allocation_failed"};
    ls_capture_error(&error);
}
void ls_pool_exhausted(const char *pool) {
    ls_runtime.heap_stats.pool_exhaustions++;
    ls_error_t error = {"memory", 0, LS_SEVERITY_ERROR, pool};
    ls_capture_error(&error);
}
void ls_heap_stats_update(const ls_heap_stats_t *stats) {
    if (stats)
        ls_runtime.heap_stats = *stats;
}
ls_heap_stats_t ls_heap_stats_get(void) {
    return ls_runtime.heap_stats;
}
