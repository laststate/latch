// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// src/core/trace.c
//
// Execution trace implementation. Bounded event ring used to
// reconstruct the path leading to a fault without the cost of a
// full debugger trace.
//
// Heap-free, bounded, deterministic.

#include "internal.h"
#include "laststate/trace.h"
#include "laststate/blackbox.h"

void ls_trace_task_switch(uint16_t previous_task, uint16_t next_task) {
    (void)ls_blackbox_record_values(LS_BLACKBOX_TASK, next_task, 0u, previous_task, next_task, 0,
                                    0);
}

void ls_trace_irq_enter(uint16_t irq) {
    (void)ls_blackbox_record_values(LS_BLACKBOX_IRQ, irq, 0u, 1, 0, 0, 0);
}

void ls_trace_irq_exit(uint16_t irq) {
    (void)ls_blackbox_record_values(LS_BLACKBOX_IRQ, irq, 0u, 0, 0, 0, 0);
}

void ls_trace_mutex_timeout(uint16_t mutex_id, uint32_t waited_ms) {
    (void)ls_blackbox_record_values(LS_BLACKBOX_TASK, mutex_id, LS_BLACKBOX_ERROR,
                                    (int32_t)waited_ms, -1, 0, 0);
    ls_performance_report(LS_PERF_DEADLINE_MISS, waited_ms, 1u);
}

void ls_trace_dma(uint16_t channel, bool started, uint32_t status) {
    uint16_t flags = status ? LS_BLACKBOX_ERROR : 0u;
    (void)ls_blackbox_record_values(LS_BLACKBOX_BUS, channel, flags, started ? 1 : 0,
                                    (int32_t)status, 0, 0);
}

void ls_trace_state(uint16_t machine_id, int32_t previous_state, int32_t next_state) {
    (void)ls_blackbox_record_values(LS_BLACKBOX_STATE, machine_id, 0u, previous_state, next_state,
                                    0, 0);
}

void ls_trace_link(uint16_t link_id, bool up, uint32_t detail) {
    (void)ls_blackbox_record_values(LS_BLACKBOX_BUS, link_id, up ? 0u : LS_BLACKBOX_ERROR,
                                    up ? 1 : 0, (int32_t)detail, 0, 0);
}
