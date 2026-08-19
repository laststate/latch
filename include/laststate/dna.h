// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/dna.h
//
// DNA (Device Fingerprint) capture API. Extracts hardware-unique
// runtime characteristics (MCU ID, boot-time variance, flash wear,
// bootloader signature) for clone and batch analysis.
//
// Heap-free, bounded, deterministic.
#ifndef LASTSTATE_DNA_H
#define LASTSTATE_DNA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// DNA capture status
typedef enum { DNA_OK = 0, DNA_NOT_SUPPORTED, DNA_NO_DATA, DNA_ERROR } dna_status_t;

// DNA capture result
typedef struct {
    bool has_mcu_id;
    uint32_t mcu_id[3]; // MCU unique ID (3x 32-bit)
    bool has_boot_sig;
    uint16_t boot_signature; // Bootloader signature
    bool has_flash_wear;
    uint16_t flash_wear_percent; // 0-100%
    bool captured;               // True when enough samples collected
} dna_capture_result_t;

// DNA capture configuration
typedef struct {
    uint16_t boot_sample_count; // Number of boot time samples to collect
    uint32_t boot_samples[16];  // Ring buffer of boot times
    uint16_t sample_index;      // Current write position
    uint16_t boot_count;        // Total boot count
    bool captured;              // True when enough samples collected
} dna_capture_config_t;

/**
 * Capture device DNA fingerprint.
 * Must be called after ls_boot().
 *
 * @return dna_capture_result_t with fingerprint data
 */
dna_capture_result_t dna_capture(void);

/**
 * Compute boot time statistics (mean, stddev).
 *
 * @param samples Array of boot time samples in ms
 * @param count Number of samples
 * @param mean_ms Output: mean boot time
 * @param stddev_ms Output: standard deviation
 */
void dna_compute_boot_stats(const uint32_t *samples, uint16_t count, float *mean_ms,
                            float *stddev_ms);

#ifdef __cplusplus
}
#endif

#endif /* LASTSTATE_DNA_H */
