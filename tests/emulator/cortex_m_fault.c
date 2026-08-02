#include <stdint.h>

#define SCB_ICSR (*(volatile uint32_t *)UINT32_C(0xe000ed04))
#define SCB_HFSR (*(volatile uint32_t *)UINT32_C(0xe000ed2c))
#define SCB_HFSR_FORCED (UINT32_C(1) << 30)
#define SCB_ICSR_NMIPENDSET (UINT32_C(1) << 31)

extern uint32_t _stack_top;

static volatile uint32_t scenario_marker;
static volatile uint32_t stack_canary;
static uint32_t emergency_stack[64];

/* Symbols consumed by the production arch/cortex-m/cortex_m_fault.S entry. */
volatile uint32_t ls_cortex_m_saved_context[16];
volatile uint32_t ls_cortex_m_fault_active;
uintptr_t ls_cortex_m_emergency_stack_top = (uintptr_t)&emergency_stack[64];

static void semihost_exit(uint32_t status) __attribute__((noreturn));
static void default_handler(void) __attribute__((noreturn));
void Reset_Handler(void) __attribute__((noreturn));
void ls_cortex_m_hardfault_handler(void);
void ls_cortex_m_nmi_handler(void);

static void semihost_exit(uint32_t status) {
    uint32_t arguments[2] = {UINT32_C(0x20026), status};
    register uint32_t operation __asm("r0") = UINT32_C(0x20);
    register uint32_t *parameters __asm("r1") = arguments;
    __asm volatile("bkpt 0xab" : : "r"(operation), "r"(parameters) : "memory");
    for (;;) {
    }
}

static void default_handler(void) {
    semihost_exit(90u);
}

void ls_cortex_m_fault_from_saved(const uint32_t *raw_frame, const volatile uint32_t *saved) {
    if (!raw_frame || saved != ls_cortex_m_saved_context)
        semihost_exit(9u);
#if defined(LS_EMULATOR_HARDFAULT)
    semihost_exit((SCB_HFSR & SCB_HFSR_FORCED) && saved[15] == 1u &&
                          scenario_marker == UINT32_C(0x48415244)
                      ? 0u
                      : 11u);
#elif defined(LS_EMULATOR_STACK_CANARY)
    semihost_exit((SCB_HFSR & SCB_HFSR_FORCED) && saved[15] == 1u &&
                          scenario_marker == UINT32_C(0x53544143) &&
                          stack_canary != UINT32_C(0x51acce55)
                      ? 0u
                      : 12u);
#if defined(LS_EMULATOR_WATCHDOG_MODEL)
    semihost_exit(saved[15] == 5u && scenario_marker == UINT32_C(0x57415443) ? 0u : 20u);
#endif
}

void ls_cortex_m_fault_recursive(uint32_t fault_kind, uint32_t exc_return, uint32_t msp,
                                 uint32_t psp) {
    (void)fault_kind;
    (void)exc_return;
    (void)msp;
    (void)psp;
    semihost_exit(30u);
}

void Reset_Handler(void) {
#if defined(LS_EMULATOR_HARDFAULT)
    scenario_marker = UINT32_C(0x48415244);
    __asm volatile("udf #0");
#elif defined(LS_EMULATOR_WATCHDOG_MODEL)
    scenario_marker = UINT32_C(0x57415443);
    SCB_ICSR = SCB_ICSR_NMIPENDSET;
    __asm volatile("dsb\n isb" ::: "memory");
#elif defined(LS_EMULATOR_STACK_CANARY)
    scenario_marker = UINT32_C(0x53544143);
    stack_canary = UINT32_C(0x51acce55);
    stack_canary ^= UINT32_C(1);
    if (stack_canary != UINT32_C(0x51acce55)) {
        __asm volatile("udf #0");
    }
#else
#error "Select one LS_EMULATOR_* scenario"
#endif
    semihost_exit(99u);
}

__attribute__((section(".isr_vector"), used)) const uintptr_t vector_table[16] = {
    (uintptr_t)&_stack_top,
    (uintptr_t)Reset_Handler,
    (uintptr_t)ls_cortex_m_nmi_handler,
    (uintptr_t)ls_cortex_m_hardfault_handler,
    (uintptr_t)default_handler,
    (uintptr_t)default_handler,
    (uintptr_t)default_handler,
    0u,
    0u,
    0u,
    0u,
    (uintptr_t)default_handler,
    (uintptr_t)default_handler,
    0u,
    (uintptr_t)default_handler,
    (uintptr_t)default_handler,
};
