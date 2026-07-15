#ifndef LASTSTATE_ZEPHYR_LATCH_H
#define LASTSTATE_ZEPHYR_LATCH_H
#include <stddef.h>
#include <stdint.h>
void ls_zephyr_fatal(uint32_t reason, const char *thread_name, uintptr_t stack_pointer);
void ls_zephyr_thread_sample(const char *name, uint32_t stack_unused, uint32_t runtime_cycles);
void ls_zephyr_heap_failure(size_t requested_size);
#endif
