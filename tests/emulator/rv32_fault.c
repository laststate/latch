#include <stdint.h>

#define UART0_BASE UINT32_C(0x10013000)
#define UART_TXDATA (*(volatile uint32_t *)(uintptr_t)(UART0_BASE + UINT32_C(0x00)))
#define UART_TXCTRL (*(volatile uint32_t *)(uintptr_t)(UART0_BASE + UINT32_C(0x08)))
#define UART_TXFULL (UINT32_C(1) << 31)

extern uint32_t _stack_top;

static void uart_puts(const char *message) {
    UART_TXCTRL = 1u;
    while (*message) {
        while (UART_TXDATA & UART_TXFULL) {
        }
        UART_TXDATA = (uint8_t)*message++;
    }
}

void trap_handler(void) __attribute__((interrupt("machine"), aligned(4)));

void trap_handler(void) {
    uintptr_t cause;
    __asm volatile("csrr %0, mcause" : "=r"(cause));
    if (cause == 2u) {
        uart_puts("LATCH:PASS:RV32_ILLEGAL_INSTRUCTION\n");
    } else {
        uart_puts("LATCH:FAIL:RV32_UNEXPECTED_TRAP\n");
    }
    for (;;) {
        __asm volatile("wfi");
    }
}

int run(void) __attribute__((used));

int run(void) {
    uintptr_t vector = (uintptr_t)trap_handler;
    __asm volatile("csrw mtvec, %0" : : "r"(vector) : "memory");
    __asm volatile(".word 0xffffffff");
    return 1;
}

void _start(void) __attribute__((naked, section(".start"), noreturn));

void _start(void) {
    __asm volatile("la sp, _stack_top\ncall run\n1: j 1b");
}
