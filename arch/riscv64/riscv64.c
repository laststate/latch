// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// arch/riscv64/riscv64.c
//
// Latch runtime source. Part of the heap-free, deterministic,
// embedded failure-capture runtime that ships fault state over LEP
// to the Relay.
//
// Heap-free, bounded, deterministic.

#include "riscv64.h"

#include <stddef.h>

#include "laststate/config.h"
#include "laststate/noinit.h"

#if defined(__GNUC__) || defined(__clang__)
#define LS_ALIGN16 __attribute__((aligned(16)))
#elif defined(_MSC_VER)
#define LS_ALIGN16 __declspec(align(16))
#else
#define LS_ALIGN16
#endif

_Static_assert((LS_EMERGENCY_STACK_SIZE % 16u) == 0u,
               "the RV64 trap stack must preserve the psABI alignment");
_Static_assert(LS_EMERGENCY_STACK_SIZE >= 768u,
               "the RV64 trap path requires the documented emergency-stack baseline");
_Static_assert(offsetof(ls_riscv64_saved_t, x) == 0u, "RV64 assembly frame x offset changed");
_Static_assert(offsetof(ls_riscv64_saved_t, mstatus) == 256u,
               "RV64 assembly frame mstatus offset changed");
_Static_assert(offsetof(ls_riscv64_saved_t, mcause) == 264u,
               "RV64 assembly frame mcause offset changed");
_Static_assert(offsetof(ls_riscv64_saved_t, mtval) == 272u,
               "RV64 assembly frame mtval offset changed");
_Static_assert(offsetof(ls_riscv64_saved_t, mepc) == 280u,
               "RV64 assembly frame mepc offset changed");
_Static_assert(sizeof(ls_riscv64_saved_t) == 288u, "RV64 assembly frame size changed");

LS_NOINIT LS_ALIGN16 static uint8_t riscv64_emergency_stack[LS_EMERGENCY_STACK_SIZE];
uintptr_t ls_riscv64_emergency_stack_top =
    (uintptr_t)(riscv64_emergency_stack + sizeof riscv64_emergency_stack);

void ls_riscv64_init(void) {
    ls_capture_minimal_prepare();
#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
    uintptr_t top = ls_riscv64_emergency_stack_top;
    __asm__ volatile("csrw mscratch, %0" : : "r"(top));
#endif
}

void ls_riscv64_capture_minimal_from_saved(const ls_riscv64_saved_t *saved) {
    if (!saved) {
        return;
    }
    ls_capture_minimal_riscv64_fault(saved->x, saved->mstatus, saved->mcause, saved->mtval,
                                     saved->mepc);
}

/* A host unit test cannot enter this terminal trap tail without hanging the
 * test process. The capture function above is tested directly; real trap
 * entry remains a target HIL responsibility. */
// LCOV_EXCL_START
void ls_riscv64_trap_from_saved(const ls_riscv64_saved_t *saved) {
    ls_riscv64_capture_minimal_from_saved(saved);
    for (;;) {
    }
}
// LCOV_EXCL_STOP
