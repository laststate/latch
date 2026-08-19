// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// src/core/fault_injection.c
//
// Fault-injection implementation. Bounded table of synthetic fault
// triggers consulted by host tests; compiled out in release
// unless LS_ENABLE_FAULT_INJECTION is set.
//
// Heap-free, bounded, deterministic.

#include "internal.h"
#include "laststate/fault_injection.h"

static ls_fault_injection_state_t *find_state(uint32_t hash) {
    for (size_t index = 0u; index < LS_FAULT_INJECTION_CAPACITY; ++index) {
        if (ls_runtime.fault_injections[index].armed &&
            ls_runtime.fault_injections[index].name_hash == hash) {
            return &ls_runtime.fault_injections[index];
        }
    }
    return 0;
}

ls_result_t ls_fault_injection_arm(const char *name, uint32_t trigger_hit, ls_result_t result) {
    if (!name || !name[0] || trigger_hit == 0u || result == LS_OK) {
        return LS_EINVAL;
    }
    uint32_t hash = ls_hash_string(name);
    ls_fault_injection_state_t *state = find_state(hash);
    if (!state) {
        for (size_t index = 0u; index < LS_FAULT_INJECTION_CAPACITY; ++index) {
            if (!ls_runtime.fault_injections[index].armed) {
                state = &ls_runtime.fault_injections[index];
                break;
            }
        }
    }
    if (!state) {
        return LS_ENOSPACE;
    }
    *state = (ls_fault_injection_state_t){hash, 0u, trigger_hit, result, true};
    ls_runtime.fault_injection_active = true;
    return LS_OK;
}

void ls_fault_injection_disarm(const char *name) {
    if (!name) {
        return;
    }
    ls_fault_injection_state_t *state = find_state(ls_hash_string(name));
    if (state) {
        ls_memset(state, 0, sizeof(*state));
    }
    ls_runtime.fault_injection_active = false;
    for (size_t index = 0u; index < LS_FAULT_INJECTION_CAPACITY; ++index) {
        if (ls_runtime.fault_injections[index].armed) {
            ls_runtime.fault_injection_active = true;
            break;
        }
    }
}

void ls_fault_injection_clear(void) {
    ls_memset(ls_runtime.fault_injections, 0, sizeof(ls_runtime.fault_injections));
    ls_runtime.fault_injection_active = false;
}

ls_result_t ls_fault_injection_hit(const char *name) {
    if (!name) {
        return LS_EINVAL;
    }
    if (!ls_runtime.fault_injection_active) {
        return LS_OK;
    }
    ls_fault_injection_state_t *state = find_state(ls_hash_string(name));
    if (!state) {
        return LS_OK;
    }
    if (state->hits != UINT32_MAX) {
        state->hits++;
    }
    return state->hits == state->trigger_hit ? state->result : LS_OK;
}

ls_result_t ls_fault_injection_get(const char *name, ls_fault_injection_state_t *state) {
    if (!name || !state) {
        return LS_EINVAL;
    }
    ls_fault_injection_state_t *found = find_state(ls_hash_string(name));
    if (!found) {
        return LS_EAGAIN;
    }
    *state = *found;
    return LS_OK;
}
