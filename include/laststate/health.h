// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/health.h
//
// Device health API. Aggregates environment samples, anomaly events,
// and boot counters into a single health snapshot the LEP envelope
// can carry alongside a fault.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_HEALTH_H
#define LASTSTATE_HEALTH_H

#include <stdbool.h>
#include <stdint.h>
#include "event.h"
typedef struct {
    const char *name;
    uint32_t deadline_ms;
    uint32_t last_touch_ms;
    uint32_t misses;
    uint32_t max_lateness_ms;
    bool expired;
} ls_health_t;
typedef struct {
    uint32_t timestamp_ms;
    uint16_t vdd_mv, battery_mv;
    int16_t current_ma, temperature_c;
    uint16_t charger_status, power_flags;
} ls_power_sample_t;

typedef struct {
    uint32_t sample_count;
    ls_power_sample_t last;
    uint16_t minimum_vdd_mv;
    uint16_t minimum_battery_mv;
    int16_t maximum_temperature_c;
    int16_t maximum_current_ma;
} ls_power_summary_t;

void ls_health_register(const char *name, uint32_t deadline_ms);
void ls_health_touch(const char *name);
ls_result_t ls_health_get(const char *name, ls_health_t *health);
size_t ls_health_snapshot(ls_health_t *health, size_t capacity);
ls_result_t ls_health_poll(void);
void ls_health_set_active_task(const char *name);
void ls_watchdog_fed(void);
uint32_t ls_watchdog_last_feed_ms(void);
void ls_watchdog_checkpoint(uint16_t checkpoint_id);
uint16_t ls_watchdog_last_checkpoint(void);
void ls_power_sample(const ls_power_sample_t *sample);
size_t ls_power_sample_count(void);
ls_result_t ls_power_get_summary(ls_power_summary_t *summary);
bool ls_power_brownout_suspected(uint16_t threshold_mv, uint32_t within_ms);
#endif
