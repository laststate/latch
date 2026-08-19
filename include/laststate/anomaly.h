// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/anomaly.h
//
// Anomaly detection API for Latch — threshold-based monitoring of
// battery, temperature, voltage, and current. Zero overhead when
// disabled (LS_ENABLE_ANOMALY=OFF in config.h).
//
// Heap-free, bounded, deterministic.
#ifndef LASTSTATE_ANOMALY_H
#define LASTSTATE_ANOMALY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize anomaly detection.
 * Must be called before any check functions.
 */
void anomaly_init(void);

/**
 * Check battery voltage anomaly.
 * @param voltage_mv Battery voltage in millivolts
 * @return true if anomalous
 */
bool anomaly_check_battery(uint16_t voltage_mv);

/**
 * Check temperature anomaly.
 * @param temp_c Temperature in Celsius
 * @return true if anomalous
 */
bool anomaly_check_temperature(int16_t temp_c);

/**
 * Check supply voltage anomaly.
 * @param voltage_uv Voltage in microvolts
 * @return true if anomalous
 */
bool anomaly_check_voltage(uint32_t voltage_uv);

/**
 * Check current draw anomaly.
 * @param current_ma Current in milliamperes
 * @return true if anomalous
 */
bool anomaly_check_current(uint16_t current_ma);

/**
 * Enable/disable anomaly detection at runtime.
 * @param enabled true to enable, false to disable
 */
void anomaly_set_enabled(bool enabled);

/**
 * Check if anomaly detection is currently enabled.
 * @return true if enabled
 */
bool anomaly_is_enabled(void);

/**
 * Get the LEP envelope flag value for anomaly events.
 * @return Flag bitmask (0x2000 = bit 13) or 0 if disabled
 */
uint16_t anomaly_get_envelope_flag(void);

#ifdef __cplusplus
}
#endif

#endif /* LASTSTATE_ANOMALY_H */
