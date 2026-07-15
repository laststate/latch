#include "freertos_latch.h"
#include "laststate/latch.h"
void ls_freertos_task_switched_in(const ls_freertos_task_snapshot_t *task) {
    if (!task)
        return;
    ls_health_set_active_task(task->name);
    ls_watchdog_checkpoint((uint16_t)((uintptr_t)task->handle & 0xffffu));
    ls_metric_u32("task_stack_watermark", task->stack_high_watermark);
}
void ls_freertos_stack_overflow(const ls_freertos_task_snapshot_t *task) {
    ls_error_t error = {"freertos", task ? (int32_t)task->stack_high_watermark : 0,
                        LS_SEVERITY_FATAL, task ? task->name : "stack_overflow"};
    ls_capture_error(&error);
}
void ls_freertos_malloc_failed(size_t requested_size) {
    ls_memory_allocation_failed(requested_size);
}
void ls_freertos_scheduler_state(uint32_t state, uint32_t task_count, uint32_t free_heap,
                                 uint32_t minimum_free_heap) {
    ls_metric_u32("scheduler_state", state);
    ls_metric_u32("task_count", task_count);
    ls_heap_stats_t stats = ls_heap_stats_get();
    stats.free_bytes = free_heap;
    stats.minimum_free_bytes = minimum_free_heap;
    ls_heap_stats_update(&stats);
}
