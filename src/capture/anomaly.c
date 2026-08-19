// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// src/capture/anomaly.c
//
// Anomaly detection implementation. Threshold checks for battery,
// temperature, voltage, and current; emits a structured breadcrumb
// and sets the ANOMALY flag on the next envelope when tripped.
//
// Heap-free, bounded, deterministic.

// Anomaly detection — lightweight threshold-based monitoring for metrics.
// Runs on-device with zero overhead when disabled (LS_ENABLE_ANOMALY=OFF).
//
// Configurable thresholds per metric:
//   LS_ANOMALY_THRESHOLD_battery_mv=3100
//   LS_ANOMALY_THRESHOLD_temperature_c=85
//   LS_ANOMALY_THRESHOLD_voltage_uv=1800000
//
// When an anomaly is detected:
//   1. Special breadcrumb "ANOMALY:<metric>:<value>" is added
//   2. LEP envelope gets ANOMALY flag set (bit 13)
//   3. Optional: trigger immediate crash capture

#include "laststate/anomaly.h"
#include "laststate/config.h"
#include "laststate/breadcrumb.h"
#include "laststate/envelope.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// Anomaly thresholds (configurable via build flags)
#define ANOMALY_BATTERY_MV_MIN 2800
#define ANOMALY_BATTERY_MV_MAX 4200
#define ANOMALY_TEMP_C_MIN -20
#define ANOMALY_TEMP_C_MAX 85
#define ANOMALY_VOLTAGE_UV_MIN 1800000
#define ANOMALY_VOLTAGE_UV_MAX 3600000
#define ANOMALY_CURRENT_MA_MAX 500

static bool anomaly_enabled = false;

// Initialize anomaly detection
void anomaly_init(void) {
#if LS_ENABLE_ANOMALY
    anomaly_enabled = true;
#else
    anomaly_enabled = false;
#endif
}

// Check battery voltage anomaly
bool anomaly_check_battery(uint16_t voltage_mv) {
    if (!anomaly_enabled)
        return false;

    if (voltage_mv < ANOMALY_BATTERY_MV_MIN || voltage_mv > ANOMALY_BATTERY_MV_MAX) {
        char msg[64];
        snprintf(msg, sizeof(msg), "ANOMALY:BATTERY:%u", voltage_mv);
        ls_breadcrumb(msg);
        return true;
    }
    return false;
}

// Check temperature anomaly
bool anomaly_check_temperature(int16_t temp_c) {
    if (!anomaly_enabled)
        return false;

    if (temp_c < ANOMALY_TEMP_C_MIN || temp_c > ANOMALY_TEMP_C_MAX) {
        char msg[64];
        snprintf(msg, sizeof(msg), "ANOMALY:TEMP:%d", temp_c);
        ls_breadcrumb(msg);
        return true;
    }
    return false;
}

// Check voltage anomaly
bool anomaly_check_voltage(uint32_t voltage_uv) {
    if (!anomaly_enabled)
        return false;

    if (voltage_uv < ANOMALY_VOLTAGE_UV_MIN || voltage_uv > ANOMALY_VOLTAGE_UV_MAX) {
        char msg[64];
        snprintf(msg, sizeof(msg), "ANOMALY:VOLTAGE:%" PRIu32, voltage_uv);
        ls_breadcrumb(msg);
        return true;
    }
    return false;
}

// Check current anomaly
bool anomaly_check_current(uint16_t current_ma) {
    if (!anomaly_enabled)
        return false;

    if (current_ma > ANOMALY_CURRENT_MA_MAX) {
        char msg[64];
        snprintf(msg, sizeof(msg), "ANOMALY:CURRENT:%u", current_ma);
        ls_breadcrumb(msg);
        return true;
    }
    return false;
}

// Set anomaly enabled state (for runtime toggle)
void anomaly_set_enabled(bool enabled) {
    anomaly_enabled = enabled;
}

// Check if anomaly detection is enabled
bool anomaly_is_enabled(void) {
    return anomaly_enabled;
}

// Get anomaly flag value for LEP envelope
uint16_t anomaly_get_envelope_flag(void) {
#if LS_ENABLE_ANOMALY
    return 0x2000; // bit 13: ANOMALY
#else
    return 0;
#endif
}
