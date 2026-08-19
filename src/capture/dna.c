// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// src/capture/dna.c
//
// DNA capture implementation. Reads MCU ID, boot-time samples, and
// flash wear through the port-supplied hooks; computes the boot
// time mean and standard deviation.
//
// Heap-free, bounded, deterministic.

// DNA capture — extracts hardware fingerprint from device runtime characteristics.
// Captured on first boot and periodically thereafter.
//
// Components:
//   - Boot time variance (measured over N boots)
//   - Clock frequency micro-variations
//   - Flash wear indicators
//   - MCU unique ID + bootloader signature
//
// This module is heap-free: all buffers are static or stack-allocated.

#include "laststate/dna.h"
#include "laststate/config.h"
#include <string.h>

// DNA capture configuration
static dna_capture_config_t dna_config = {
    .boot_sample_count = 5,
    .boot_samples = {0},
    .sample_index = 0,
    .boot_count = 0,
    .captured = false,
};

// Extract DNA from runtime
dna_capture_result_t dna_capture(void) {
    dna_capture_result_t result = {0};

#if LS_ENABLE_DNA
    // Read MCU unique ID (if available)
    uint32_t mcu_id[3] = {0};
    if (dna_read_mcu_id(mcu_id, 3) == DNA_OK) {
        result.has_mcu_id = true;
        memcpy(result.mcu_id, mcu_id, sizeof(mcu_id));
    }

    // Read bootloader signature
    uint16_t boot_sig = 0;
    if (dna_read_bootloader_sig(&boot_sig) == DNA_OK) {
        result.has_boot_sig = true;
        result.boot_signature = boot_sig;
    }

    // Read flash wear indicator (if available)
    uint16_t flash_wear = 0;
    if (dna_read_flash_wear(&flash_wear) == DNA_OK) {
        result.has_flash_wear = true;
        result.flash_wear_percent = flash_wear;
    }

    // Record boot time
    uint32_t boot_time_ms = dna_read_boot_time_ms();
    dna_record_boot_time(boot_time_ms);

    // Mark as captured if we have enough samples
    if (dna_config.sample_index >= dna_config.boot_sample_count) {
        dna_config.captured = true;
    }

    result.captured = dna_config.captured;
#else
    result.captured = false;
#endif

    return result;
}

// Record a boot time sample
static void dna_record_boot_time(uint32_t boot_time_ms) {
    if (dna_config.boot_sample_count <= 0) return;

    dna_config.boot_samples[dna_config.sample_index] = boot_time_ms;
    dna_config.sample_index++;

    if (dna_config.sample_index >= dna_config.boot_sample_count) {
        dna_config.sample_index = 0; // wrap around
    }
    dna_config.boot_count++;
}

// Compute mean and standard deviation of boot times
void dna_compute_boot_stats(const uint32_t *samples, uint16_t count,
                            float *mean_ms, float *stddev_ms) {
    if (count == 0) {
        *mean_ms = 0;
        *stddev_ms = 0;
        return;
    }

    // Compute mean
    uint32_t sum = 0;
    for (uint16_t i = 0; i < count; i++) {
        sum += samples[i];
    }
    *mean_ms = (float)sum / (float)count;

    // Compute stddev
    uint32_t var_sum = 0;
    float m = *mean_ms;
    for (uint16_t i = 0; i < count; i++) {
        float diff = (float)samples[i] - m;
        var_sum += (uint32_t)(diff * diff);
    }
    *stddev_ms = sqrtf((float)var_sum / (float)count);
}

// Platform-specific hooks (implemented by port)

#if LS_ENABLE_DNA
dna_status_t dna_read_mcu_id(uint32_t *id, uint16_t length) {
    // Port-specific: read from MCU unique ID register
    return DNA_NOT_SUPPORTED;
}

dna_status_t dna_read_bootloader_sig(uint16_t *sig) {
    // Port-specific: read bootloader version/signature
    return DNA_NOT_SUPPORTED;
}

dna_status_t dna_read_flash_wear(uint16_t *wear_percent) {
    // Port-specific: read flash erase cycle count
    return DNA_NOT_SUPPORTED;
}

uint32_t dna_read_boot_time_ms(void) {
    // Port-specific: measure time between power-on and LS_READY
    return 0;
}
#endif
