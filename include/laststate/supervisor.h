// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/supervisor.h
//
// Supervisor / watchdog API. Owns the watchdog feed, lockup
// detection, and the reset reason reported on the next boot.
// Designed to be ticked from a low-priority task.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_SUPERVISOR_H
#define LASTSTATE_SUPERVISOR_H

#include <stdbool.h>
#include <stdint.h>

#include "event.h"

enum {
    LS_SUPERVISOR_WATCHDOG_STALE = 1u << 0,
    LS_SUPERVISOR_VDD_LOW = 1u << 1,
    LS_SUPERVISOR_BATTERY_LOW = 1u << 2,
    LS_SUPERVISOR_TEMPERATURE_HIGH = 1u << 3,
    LS_SUPERVISOR_HEAP_LOW = 1u << 4,
    LS_SUPERVISOR_SPOOL_HIGH = 1u << 5,
    LS_SUPERVISOR_BOOT_LOOP = 1u << 6,
    LS_SUPERVISOR_HEALTH_DEADLINE = 1u << 7,
    LS_SUPERVISOR_ENVIRONMENT_LEAK = 1u << 8,
    LS_SUPERVISOR_VIBRATION_HIGH = 1u << 9,
    LS_SUPERVISOR_ENV_SENSOR_FAULT = 1u << 10
};

typedef struct {
    uint32_t watchdog_max_stale_ms;
    uint16_t minimum_vdd_mv;
    uint16_t minimum_battery_mv;
    int16_t maximum_temperature_c;
    uint32_t minimum_heap_free_bytes;
    uint8_t maximum_spool_percent;
    uint16_t maximum_vibration_mg_rms;
    uint32_t anomaly_hold_ms;
} ls_supervisor_config_t;

typedef struct {
    uint32_t active_alarms;
    uint32_t previous_alarms;
    uint32_t transitions;
    uint32_t last_change_ms;
    bool configured;
} ls_supervisor_status_t;

ls_result_t ls_supervisor_configure(const ls_supervisor_config_t *config);
ls_result_t ls_supervisor_poll(void);
ls_supervisor_status_t ls_supervisor_get_status(void);

#endif
