// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/ota.h
//
// Firmware-update / OTA API. Tracks pending releases, confirms
// successful boot on the new image, and triggers rollback when
// boot_loop detection fires.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_OTA_H
#define LASTSTATE_OTA_H

#include <stddef.h>
#include <stdint.h>

#include "event.h"

typedef struct {
    void *context;
    /* Verify the candidate digest/signature against the product trust store. */
    ls_result_t (*authenticate)(void *context, uint32_t version_counter, uint32_t signing_key_id,
                                const uint8_t digest[32], const uint8_t *signature,
                                size_t signature_length);
    /* Write the already-authenticated candidate to the inactive slot. */
    ls_result_t (*stage)(void *context, const uint8_t *image, size_t image_length);
    /* Configure the bootloader to try the staged candidate on next boot. */
    ls_result_t (*request_boot)(void *context, uint32_t version_counter);
    /* Permanently accept the currently running candidate. */
    ls_result_t (*confirm)(void *context, uint32_t version_counter);
    /* Request the bootloader to return to the last confirmed image. */
    ls_result_t (*rollback)(void *context, uint32_t failed_version);
} ls_ota_backend_t;

ls_result_t ls_ota_stage_candidate(const ls_ota_backend_t *backend, uint32_t version_counter,
                                   uint32_t signing_key_id, const uint8_t *image,
                                   size_t image_length, const uint8_t *signature,
                                   size_t signature_length);
ls_result_t ls_ota_confirm_running(const ls_ota_backend_t *backend, uint32_t version_counter);
ls_result_t ls_ota_request_rollback(const ls_ota_backend_t *backend, uint32_t failed_version);

#endif
