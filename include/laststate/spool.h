// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/spool.h
//
// Bounded spool API. Holds serialized LEP envelopes awaiting
// transport, with a drop policy that keeps errors and drops the
// oldest healthy entries when space is tight.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_SPOOL_H
#define LASTSTATE_SPOOL_H

#include <stddef.h>
#include <stdint.h>

#include "event.h"

typedef struct {
    size_t capacity;
    size_t committed;
    size_t reserved_critical;
    size_t reserved_emergency;
    size_t high_watermark;
    uint32_t dropped_records;
    uint32_t corrupt_records;
    uint32_t transport_failures;
    uint32_t retry_saturated;
} ls_spool_stats_t;

/* Normal-runtime observability for the durable event spool. The counters are
 * process-lifetime counters; committed/capacity are read from the currently
 * registered storage backend. */
ls_result_t ls_spool_get_stats(ls_spool_stats_t *stats);

#endif
