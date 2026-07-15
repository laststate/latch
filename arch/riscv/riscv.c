#include "riscv.h"
#include "laststate/latch.h"
#include "laststate/noinit.h"
LS_NOINIT static uint8_t riscv_emergency_stack[LS_EMERGENCY_STACK_SIZE];
uintptr_t ls_riscv_emergency_stack_top =
    (uintptr_t)(riscv_emergency_stack + sizeof riscv_emergency_stack);
void ls_riscv_init(void) {
    uintptr_t top = ls_riscv_emergency_stack_top;
    __asm__ volatile("csrw mscratch, %0" ::"r"(top));
}
void ls_riscv_trap_from_saved(const ls_riscv_saved_t *saved) {
    ls_arch_context_t context = {0};
    context.architecture = LS_ARCH_RISCV;
    context.fault = LS_FAULT_TRAP;
    for (unsigned i = 0; i < 32; i++)
        context.registers[i] = saved->x[i];
    context.lr = saved->x[1];
    context.msp = saved->x[2];
    context.pc = saved->mepc;
    context.mstatus = saved->mstatus;
    context.mcause = saved->mcause;
    context.mtval = saved->mtval;
    context.mepc = saved->mepc;
    (void)ls_capture_cpu_context(&context);
    for (;;) {
    }
}
