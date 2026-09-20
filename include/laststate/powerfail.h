// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/powerfail.h
//
// Early-warning power-fail seal API ("Last-Microjoule commit"). A PVD/NMI
// handler seals a small fixed record into retained memory with bounded,
// heap-free stores only. Normal boot promotes a valid seal into the spool
// before clearing it. Not hardware-tested.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_POWERFAIL_H
#define LASTSTATE_POWERFAIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "event.h"

typedef enum {
    LS_POWERFAIL_NONE = 0,
    LS_POWERFAIL_BROWNOUT = 1,
    LS_POWERFAIL_POWER_LOSS = 2,
    LS_POWERFAIL_PVD = 3
} ls_powerfail_reason_t;

typedef enum {
    LS_POWERFAIL_TIER_RAM = 0,
    LS_POWERFAIL_TIER_BACKUP_RAM = 1
} ls_powerfail_tier_t;

typedef struct {
    ls_powerfail_reason_t reason;
    ls_powerfail_tier_t tier;
    uint16_t vcap_mv;
    uint32_t boot_id;
    uint32_t fault;
    bool sealed_ok;
} ls_powerfail_seal_info_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Normal-runtime only. Registers a VBAT-retained RAM window (STM32 backup
 * SRAM, nRF backup registers, ESP32 RTC FAST, or a host test buffer).
 * The NMI path writes it with direct bounded stores; no callbacks run in
 * the NMI path. Pass NULL/0 to use the internal .noinit record only. */
void ls_powerfail_install_backup(uint8_t *ram, size_t size);

/*
 * NMI/PVD-context safe. Seals the current fault evidence plus the power-fail
 * reason into retained memory. Touches only retained memory and the installed
 * backup window with bounded loops. Never allocates, blocks, logs, or calls
 * storage, transport, crypto, or reset callbacks. Idempotent: a second NMI
 * during the same brownout seals once and stops.
 *
 * Intended call: ls_powerfail_seal(LS_POWERFAIL_BROWNOUT, vcap_mv);
 */
void ls_powerfail_seal(ls_powerfail_reason_t reason, uint16_t vcap_mv);

/* Normal-runtime only. Returns a copy of the last promoted seal carried in
 * the LEP stream (TLV 23). Cleared by ls_boot_mark_successful(). */
ls_powerfail_seal_info_t ls_powerfail_last_seal(void);

/* Normal-runtime only. Reads the retained seal without promoting it. */
bool ls_powerfail_sealed_read(ls_powerfail_seal_info_t *out);

/* Normal-runtime only. Discards the retained seal without promoting it. */
void ls_powerfail_sealed_clear(void);

/* Normal-runtime only. Promotes a valid retained seal into the spool as an
 * EMERGENCY reset event and clears it only after the append succeeds.
 * Called automatically by ls_boot() before minimal-snapshot recovery. */
ls_result_t ls_powerfail_recover(void);

#ifdef __cplusplus
}
#endif

#endif
