.option norvc
.section .text
.global ls_riscv_trap_handler
.type ls_riscv_trap_handler, @function
.extern ls_riscv_trap_from_saved
ls_riscv_trap_handler:
  csrrw sp, mscratch, sp
  addi sp, sp, -144
  sw zero, 0(sp)
  sw ra, 4(sp)
  sw gp, 12(sp)
  sw tp, 16(sp)
  sw t0, 20(sp)
  sw t1, 24(sp)
  sw t2, 28(sp)
  sw s0, 32(sp)
  sw s1, 36(sp)
  sw a0, 40(sp)
  sw a1, 44(sp)
  sw a2, 48(sp)
  sw a3, 52(sp)
  sw a4, 56(sp)
  sw a5, 60(sp)
  sw a6, 64(sp)
  sw a7, 68(sp)
  sw s2, 72(sp)
  sw s3, 76(sp)
  sw s4, 80(sp)
  sw s5, 84(sp)
  sw s6, 88(sp)
  sw s7, 92(sp)
  sw s8, 96(sp)
  sw s9, 100(sp)
  sw s10, 104(sp)
  sw s11, 108(sp)
  sw t3, 112(sp)
  sw t4, 116(sp)
  sw t5, 120(sp)
  sw t6, 124(sp)
  csrr t0, mscratch
  sw t0, 8(sp)
  csrr t0, mstatus
  sw t0, 128(sp)
  csrr t0, mcause
  sw t0, 132(sp)
  csrr t0, mtval
  sw t0, 136(sp)
  csrr t0, mepc
  sw t0, 140(sp)
  mv a0, sp
  tail ls_riscv_trap_from_saved
