// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/performance.h
//
// Performance counters API. Cycle and timing measurements that feed
// health and anomaly; safe to call from fault context but only
// with portable back-ends.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_PERFORMANCE_H
#define LASTSTATE_PERFORMANCE_H

#include <stdint.h>
#include "event.h"
typedef enum {
    LS_PERF_LOOP_LATENCY,
    LS_PERF_ISR_LATENCY,
    LS_PERF_DEADLINE_MISS,
    LS_PERF_CPU_SATURATION,
    LS_PERF_QUEUE_CONGESTION,
    LS_PERF_DMA_TIMEOUT,
    LS_PERF_FRAME_DROP,
    LS_PERF_SAMPLING_JITTER
} ls_performance_kind_t;
typedef struct {
    uint16_t id;
    uint32_t started_ms;
    uint32_t elapsed_ms;
    bool active;
} ls_span_t;
ls_result_t ls_span_begin(uint16_t id);
ls_result_t ls_span_end(uint16_t id);
void ls_performance_report(ls_performance_kind_t kind, uint32_t value, uint32_t threshold);
#endif
