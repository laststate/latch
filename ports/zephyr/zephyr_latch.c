#include "zephyr_latch.h"
#include "laststate/latch.h"
void ls_zephyr_fatal(uint32_t reason, const char *thread_name, uintptr_t stack_pointer) {
    ls_health_set_active_task(thread_name);
    ls_error_t error = {"zephyr", (int32_t)reason, LS_SEVERITY_FATAL,
                        thread_name ? thread_name : "kernel_fatal"};
    ls_capture_error(&error);
    (void)stack_pointer;
}
void ls_zephyr_thread_sample(const char *name, uint32_t stack_unused, uint32_t runtime_cycles) {
    ls_health_set_active_task(name);
    ls_metric_u32("thread_stack_unused", stack_unused);
    ls_metric_u32("thread_runtime_cycles", runtime_cycles);
}
void ls_zephyr_heap_failure(size_t requested_size) {
    ls_memory_allocation_failed(requested_size);
}
