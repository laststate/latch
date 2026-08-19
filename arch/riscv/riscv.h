// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// arch/riscv/riscv.h
//
// Latch runtime source. Part of the heap-free, deterministic,
// embedded failure-capture runtime that ships fault state over LEP
// to the Relay.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_RISCV_H
#define LASTSTATE_RISCV_H
#include <stdint.h>
typedef struct {
    uint32_t x[32];
    uint32_t mstatus, mcause, mtval, mepc;
} ls_riscv_saved_t;
void ls_riscv_init(void);
void ls_riscv_trap_handler(void);
void ls_riscv_trap_from_saved(const ls_riscv_saved_t *saved);
#endif
