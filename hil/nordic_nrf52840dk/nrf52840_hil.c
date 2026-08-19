#include <stdint.h>

#define UARTE0 0x40002000u
#define REG(a) (*(volatile uint32_t *)(UARTE0 + (a)))
#define TASKS_STARTTX 0x008u
#define EVENTS_ENDTX 0x120u
#define ENABLE 0x500u
#define PSELTXD 0x50Cu
#define BAUDRATE 0x524u
#define TXD_PTR 0x544u
#define TXD_MAXCNT 0x548u
#define CONFIG 0x56Cu
#define SCB_SHCSR (*(volatile uint32_t *)0xE000ED24u)
#define SCB_CFSR (*(volatile uint32_t *)0xE000ED28u)

static void uart_init(void) {
    REG(PSELTXD) = 6u;
    REG(BAUDRATE) = 0x01D7E000u; /* 115200 */
    REG(CONFIG) = 0u;
    REG(ENABLE) = 8u; /* UARTE */
}

static void uart_puts(const char *s) {
    static uint8_t tx_buffer[64];
    uint32_t length = 0u;
    while (s[length] != '\0' && length < sizeof(tx_buffer)) {
        tx_buffer[length] = (uint8_t)s[length];
        ++length;
    }
    REG(EVENTS_ENDTX) = 0u;
    REG(TXD_PTR) = (uint32_t)(uintptr_t)tx_buffer;
    REG(TXD_MAXCNT) = length;
    REG(TASKS_STARTTX) = 1u;
}

extern uint8_t __stack_top__;

void HardFault_Handler(void) {
    uart_puts("HIL:PASS:HARDFAULT\n");
    for (;;) {
    }
}

void UsageFault_Handler(void) {
    if ((SCB_CFSR & (1u << 16)) != 0u) {
        uart_puts("HIL:PASS:USAGEFAULT\n");
    } else {
        uart_puts("HIL:FAIL:USAGEFAULT:CFSR\n");
    }
    for (;;) {
    }
}

void Reset_Handler(void) {
    uart_init();
    uart_puts("HIL:ARMED:USAGEFAULT\n");
    SCB_SHCSR |= (1u << 18); /* USGFAULTENA */
    __asm volatile("udf #0");
    for (;;) {
    }
}

void Default_Handler(void) {
    for (;;) {
    }
}

__attribute__((section(".vectors"), used)) const uintptr_t vectors[] = {
    (uintptr_t)&__stack_top__,     (uintptr_t)Reset_Handler,   (uintptr_t)Default_Handler,
    (uintptr_t)HardFault_Handler,  (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)UsageFault_Handler,
};
