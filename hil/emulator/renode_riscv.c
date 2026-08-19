#include <stdint.h>

#ifndef LS_RENODE_MARKER_ADDR
#define LS_RENODE_MARKER_ADDR UINT32_C(0x10000000)
#endif
#define LS_RENODE_MARKER_MAGIC UINT32_C(0x4c415443) /* "LATC" */

extern uint32_t _stack_top;

static void marker_report(uint32_t status) {
    volatile uint32_t *marker = (volatile uint32_t *)LS_RENODE_MARKER_ADDR;
    marker[0] = LS_RENODE_MARKER_MAGIC;
    marker[1] = status;
    marker[2] = UINT32_C(0);
    for (;;) {
    }
}

void trap_handler(void) __attribute__((interrupt("machine"), aligned(4)));

void trap_handler(void) {
    uintptr_t cause;
    __asm volatile("csrr %0, mcause" : "=r"(cause));
    if (cause == 2u) {
        marker_report(0u);
    } else {
        marker_report((uint32_t)(cause | UINT32_C(0x80000000)));
    }
    for (;;) {
        __asm volatile("wfi");
    }
}

void _start(void) __attribute__((naked, section(".start"), noreturn));

void _start(void) {
    __asm volatile(
        "la sp, _stack_top\n"
        "la t0, trap_handler\n"
        "csrw mtvec, t0\n"
        ".word 0xffffffff\n"
        "1: j 1b");
}