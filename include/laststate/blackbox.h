// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/blackbox.h
//
// Retained flight/mission recorder. Captures bounded recent events
// across power cycles using .noinit memory; diagnostic only and
// must never be used as a safety-control state store.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_BLACKBOX_H
#define LASTSTATE_BLACKBOX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "event.h"

typedef enum {
    LS_BLACKBOX_STATE = 1,
    LS_BLACKBOX_TASK,
    LS_BLACKBOX_IRQ,
    LS_BLACKBOX_WATCHDOG,
    LS_BLACKBOX_POWER,
    LS_BLACKBOX_SENSOR,
    LS_BLACKBOX_BUS,
    LS_BLACKBOX_CONTROL,
    LS_BLACKBOX_MISSION,
    LS_BLACKBOX_USER
} ls_blackbox_kind_t;

typedef enum {
    LS_BLACKBOX_PROFILE_QUIET = 0,
    LS_BLACKBOX_PROFILE_NORMAL = 1,
    LS_BLACKBOX_PROFILE_ANOMALY = 2
} ls_blackbox_profile_t;

enum {
    LS_BLACKBOX_IMPORTANT = 1u << 0,
    LS_BLACKBOX_ERROR = 1u << 1,
    LS_BLACKBOX_SENSITIVE = 1u << 2
};

typedef struct {
    uint32_t timestamp_ms;
    uint16_t kind;
    uint16_t source_id;
    uint16_t flags;
    uint16_t reserved;
    int32_t value[4];
} ls_blackbox_record_t;

typedef struct {
    size_t capacity;
    size_t count;
    uint32_t total_records;
    uint32_t overwritten_records;
    bool frozen;
    ls_blackbox_profile_t profile;
    uint32_t anomaly_until_ms;
} ls_blackbox_stats_t;

/* Retained flight/mission recorder. The implementation uses .noinit when the
 * toolchain supports it, so a CPU reset can preserve the most recent records.
 * It is diagnostic evidence only and must never be used as a safety-control
 * state store. */
void ls_blackbox_init(void);
ls_result_t ls_blackbox_record(const ls_blackbox_record_t *record);
ls_result_t ls_blackbox_record_values(ls_blackbox_kind_t kind, uint16_t source_id, uint16_t flags,
                                      int32_t v0, int32_t v1, int32_t v2, int32_t v3);
void ls_blackbox_freeze(void);
void ls_blackbox_thaw(void);
void ls_blackbox_clear(void);
void ls_blackbox_set_profile(ls_blackbox_profile_t profile);
void ls_blackbox_anomaly_begin(uint32_t duration_ms);
void ls_blackbox_poll(void);
ls_result_t ls_blackbox_copy(ls_blackbox_record_t *records, size_t capacity, size_t *count);
/* age=0 returns the newest valid record, age=1 the previous one. This avoids
 * copying the full retained ring onto a constrained task stack. */
ls_result_t ls_blackbox_get_recent(size_t age, ls_blackbox_record_t *record);
ls_result_t ls_blackbox_get_stats(ls_blackbox_stats_t *stats);

#endif
