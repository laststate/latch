// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/update.h
//
// Update / changelog API. A small, audited log of release metadata
// and rollbacks that Latch carries in the envelope identity block
// to correlate records to firmware.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_UPDATE_H
#define LASTSTATE_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#include "event.h"

typedef enum {
    LS_UPDATE_NONE = 0,
    LS_UPDATE_PENDING = 1,
    LS_UPDATE_CONFIRMED = 2,
    LS_UPDATE_ROLLED_BACK = 3
} ls_update_state_code_t;

typedef struct {
    uint32_t confirmed_version_floor;
    uint32_t pending_version;
    uint32_t image_fingerprint;
    uint32_t signing_key_id;
    uint32_t rollback_count;
    ls_update_state_code_t state;
} ls_update_state_t;

/* Bootloader integration helpers. `version_counter` is a product-owned,
 * monotonically increasing integer. These functions do not verify firmware
 * signatures; the bootloader must authenticate the image before staging it. */
bool ls_update_version_allowed(uint32_t version_counter);
ls_result_t ls_update_stage(uint32_t version_counter, uint32_t image_fingerprint,
                            uint32_t signing_key_id);
ls_result_t ls_update_confirm_version(uint32_t version_counter);
ls_result_t ls_update_mark_version_rollback(uint32_t failed_version);
/* Clears a pending candidate that was staged in metadata but never handed to
 * the bootloader. Does not lower the confirmed anti-rollback floor. */
ls_result_t ls_update_abort_stage(uint32_t version_counter);
ls_update_state_t ls_update_get_state(void);

#endif
