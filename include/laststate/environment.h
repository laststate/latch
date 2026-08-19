// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/environment.h
//
// Runtime environment sampling API (battery, temperature, supply
// voltage). Feeds health, anomaly, and DNA modules with the
// on-device telemetry they consume.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_ENVIRONMENT_H
#define LASTSTATE_ENVIRONMENT_H

#include <stdbool.h>
#include <stdint.h>
#include "event.h"

enum {
    LS_ENV_LEAK_DETECTED = 1u << 0,
    LS_ENV_WATER_INGRESS = 1u << 1,
    LS_ENV_PRESSURE_SENSOR_FAULT = 1u << 2,
    LS_ENV_VIBRATION_LIMIT = 1u << 3
};

typedef struct {
    uint32_t timestamp_ms;
    uint32_t pressure_pa;
    int32_t depth_cm;
    int16_t internal_temperature_c;
    uint16_t humidity_permyriad;
    uint16_t vibration_mg_rms;
    uint16_t flags;
} ls_environment_sample_t;

typedef struct {
    ls_environment_sample_t last;
    uint32_t sample_count;
    uint32_t maximum_pressure_pa;
    int32_t maximum_depth_cm;
    int16_t maximum_temperature_c;
    uint16_t maximum_humidity_permyriad;
    uint16_t maximum_vibration_mg_rms;
    uint32_t leak_events;
} ls_environment_summary_t;

/* Records environmental evidence only. Safety actions such as abort/ascent must
 * remain in an independent vehicle safety controller. */
void ls_environment_sample(const ls_environment_sample_t *sample);
ls_result_t ls_environment_get_summary(ls_environment_summary_t *summary);
bool ls_environment_leak_detected(void);

#endif
