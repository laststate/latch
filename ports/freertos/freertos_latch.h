#ifndef LASTSTATE_FREERTOS_LATCH_H
#define LASTSTATE_FREERTOS_LATCH_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    const char *name;
    void *handle;
    uintptr_t stack_pointer;
    uint32_t stack_high_watermark, runtime_counter, state;
} ls_freertos_task_snapshot_t;
void ls_freertos_task_switched_in(const ls_freertos_task_snapshot_t *task);
void ls_freertos_stack_overflow(const ls_freertos_task_snapshot_t *task);
void ls_freertos_malloc_failed(size_t requested_size);
void ls_freertos_scheduler_state(uint32_t state, uint32_t task_count, uint32_t free_heap,
                                 uint32_t minimum_free_heap);
#endif
