#include <stdint.h>

#define SCB_ICSR (*(volatile uint32_t *)UINT32_C(0xe000ed04))
#define SCB_SHCSR (*(volatile uint32_t *)UINT32_C(0xe000ed24))
#define SCB_CFSR (*(volatile uint32_t *)UINT32_C(0xe000ed28))
#define SCB_HFSR (*(volatile uint32_t *)UINT32_C(0xe000ed2c))
#define SCB_HFSR_FORCED (UINT32_C(1) << 30)
#define SCB_ICSR_NMIPENDSET (UINT32_C(1) << 31)
#define SCB_SHCSR_USGFAULTENA (UINT32_C(1) << 18)

#ifndef LS_RENODE_MARKER_ADDR
#define LS_RENODE_MARKER_ADDR UINT32_C(0x20000000)
#endif
#define LS_RENODE_MARKER_MAGIC UINT32_C(0x4c415443) /* "LATC" */

extern uint32_t _stack_top;

static volatile uint32_t scenario_marker;
static volatile uint32_t stack_canary;
static volatile uint32_t fault_result;
static uint32_t emergency_stack[64];

/* Symbols consumed by the production arch/cortex-m/cortex_m_fault.S entry. */
volatile uint32_t ls_cortex_m_saved_context[16];
volatile uint32_t ls_cortex_m_fault_active;
uintptr_t ls_cortex_m_emergency_stack_top = (uintptr_t)&emergency_stack[64];

static void marker_report(uint32_t status) __attribute__((noreturn));
static void thread_report(void) __attribute__((noreturn));
static void default_handler(void) __attribute__((noreturn));
void Reset_Handler(void) __attribute__((noreturn));
void ls_cortex_m_hardfault_handler(void);
void ls_cortex_m_usagefault_handler(void);
void ls_cortex_m_nmi_handler(void);

static void marker_report(uint32_t status) {
    volatile uint32_t *marker = (volatile uint32_t *)LS_RENODE_MARKER_ADDR;
    marker[0] = LS_RENODE_MARKER_MAGIC;
    marker[1] = status;
    marker[2] = fault_result;
    for (;;) {
    }
}

static void default_handler(void) {
    marker_report(90u);
}

static void thread_report(void) {
    marker_report(fault_result);
}

void __attribute__((noreturn)) ls_cortex_m_fault_from_saved(uint32_t *raw_frame,
                                                            const volatile uint32_t *saved) {
    if (!raw_frame || saved != ls_cortex_m_saved_context) {
        fault_result = 9u;
    } else {
#if defined(LS_EMULATOR_HARDFAULT)
        fault_result = (SCB_HFSR & SCB_HFSR_FORCED) && saved[15] == 1u &&
                               scenario_marker == UINT32_C(0x48415244)
                           ? 0u
                           : 11u;
#elif defined(LS_EMULATOR_USAGEFAULT)
        fault_result = (SCB_CFSR & (UINT32_C(1) << 16)) && saved[15] == 4u &&
                               scenario_marker == UINT32_C(0x55534147)
                           ? 0u
                           : 14u;
#elif defined(LS_EMULATOR_STACK_CANARY)
        fault_result = saved[15] == 4u && scenario_marker == UINT32_C(0x53544143) &&
                               stack_canary != UINT32_C(0x51acce55)
                           ? 0u
                           : 12u;
#elif defined(LS_EMULATOR_WATCHDOG_MODEL)
        fault_result = saved[15] == 5u && scenario_marker == UINT32_C(0x57415443) ? 0u : 20u;
#else
        fault_result = 21u;
#endif
    }
    if (raw_frame)
        raw_frame[6] = (uintptr_t)thread_report;
    __asm volatile("msr msp, %0\n"
                   "msr psp, %1\n"
                   "bx lr" ::"r"(saved[8]),
                   "r"(saved[9])
                   : "memory");
    __builtin_unreachable();
}

void ls_cortex_m_fault_recursive(uint32_t fault_kind, uint32_t exc_return, uint32_t msp,
                                 uint32_t psp) {
    (void)fault_kind;
    (void)exc_return;
    (void)msp;
    (void)psp;
    marker_report(30u);
}

void Reset_Handler(void) {
#if defined(LS_EMULATOR_HARDFAULT)
    scenario_marker = UINT32_C(0x48415244);
    __asm volatile("udf #0");
#elif defined(LS_EMULATOR_USAGEFAULT)
    scenario_marker = UINT32_C(0x55534147);
    SCB_SHCSR |= SCB_SHCSR_USGFAULTENA;
    __asm volatile("udf #0");
#elif defined(LS_EMULATOR_WATCHDOG_MODEL)
    scenario_marker = UINT32_C(0x57415443);
    SCB_ICSR = SCB_ICSR_NMIPENDSET;
    __asm volatile("dsb\n isb" ::: "memory");
#elif defined(LS_EMULATOR_STACK_CANARY)
    scenario_marker = UINT32_C(0x53544143);
    SCB_SHCSR |= SCB_SHCSR_USGFAULTENA;
    stack_canary = UINT32_C(0x51acce55);
    stack_canary ^= UINT32_C(1);
    if (stack_canary != UINT32_C(0x51acce55)) {
        __asm volatile("udf #0");
    }
#else
#error "Select one LS_EMULATOR_* scenario"
#endif
    marker_report(99u);
}

__attribute__((section(".isr_vector"), used)) const uintptr_t vector_table[16] = {
    (uintptr_t)&_stack_top,
    (uintptr_t)Reset_Handler,
    (uintptr_t)ls_cortex_m_nmi_handler,
    (uintptr_t)ls_cortex_m_hardfault_handler,
    (uintptr_t)default_handler,
    (uintptr_t)default_handler,
    (uintptr_t)ls_cortex_m_usagefault_handler,
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