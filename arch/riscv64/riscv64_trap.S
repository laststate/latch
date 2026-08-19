// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// arch/riscv64/riscv64_trap.S
//
// Latch runtime source. Part of the heap-free, deterministic,
// embedded failure-capture runtime that ships fault state over LEP
// to the Relay.
//
// Heap-free, bounded, deterministic.

.option norvc
.section .text
.global ls_riscv64_trap_handler
.type ls_riscv64_trap_handler, @function
.extern ls_riscv64_trap_from_saved
.extern ls_riscv64_emergency_stack_top

/*
 * Switch to the preconfigured emergency stack before touching the interrupted
 * stack. The fixed 288-byte frame is ls_riscv64_saved_t: x0..x31 followed by
 * mstatus, mcause, mtval, and mepc. 288 is a multiple of the RV64 psABI's
 * required 16-byte stack alignment.
 */
ls_riscv64_trap_handler:
  csrrw sp, mscratch, sp
  addi sp, sp, -288
  sd zero, 0(sp)
  sd ra, 8(sp)
  sd gp, 24(sp)
  sd tp, 32(sp)
  sd t0, 40(sp)
  sd t1, 48(sp)
  sd t2, 56(sp)
  sd s0, 64(sp)
  sd s1, 72(sp)
  sd a0, 80(sp)
  sd a1, 88(sp)
  sd a2, 96(sp)
  sd a3, 104(sp)
  sd a4, 112(sp)
  sd a5, 120(sp)
  sd a6, 128(sp)
  sd a7, 136(sp)
  sd s2, 144(sp)
  sd s3, 152(sp)
  sd s4, 160(sp)
  sd s5, 168(sp)
  sd s6, 176(sp)
  sd s7, 184(sp)
  sd s8, 192(sp)
  sd s9, 200(sp)
  sd s10, 208(sp)
  sd s11, 216(sp)
  sd t3, 224(sp)
  sd t4, 232(sp)
  sd t5, 240(sp)
  sd t6, 248(sp)
  csrr t0, mscratch
  sd t0, 16(sp)
  /* Restore the emergency top before entering C. A nested trap therefore
     restarts on known-good emergency memory instead of swapping back to the
     interrupted stack. */
  la t0, ls_riscv64_emergency_stack_top
  ld t0, 0(t0)
  csrw mscratch, t0
  csrr t0, mstatus
  sd t0, 256(sp)
  csrr t0, mcause
  sd t0, 264(sp)
  csrr t0, mtval
  sd t0, 272(sp)
  csrr t0, mepc
  sd t0, 280(sp)
  mv a0, sp
  tail ls_riscv64_trap_from_saved
