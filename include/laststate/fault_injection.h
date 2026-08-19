// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/fault_injection.h
//
// Fault-injection hooks used by host tests to exercise the crash
// capture path deterministically without a real hardware fault.
// Compiled out in release unless LS_ENABLE_FAULT_INJECTION is on.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_FAULT_INJECTION_H
#define LASTSTATE_FAULT_INJECTION_H

#include <stdbool.h>
#include <stdint.h>

#include "event.h"

typedef struct {
    uint32_t name_hash;
    uint32_t hits;
    uint32_t trigger_hit;
    ls_result_t result;
    bool armed;
} ls_fault_injection_state_t;

ls_result_t ls_fault_injection_arm(const char *name, uint32_t trigger_hit, ls_result_t result);
void ls_fault_injection_disarm(const char *name);
void ls_fault_injection_clear(void);
ls_result_t ls_fault_injection_hit(const char *name);
ls_result_t ls_fault_injection_get(const char *name, ls_fault_injection_state_t *state);

#endif
