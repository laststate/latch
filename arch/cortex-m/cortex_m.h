#ifndef LASTSTATE_CORTEX_M_H
#define LASTSTATE_CORTEX_M_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "laststate/capture.h"

typedef struct {
    uint32_t r4_r11[8];
    uint32_t msp;
    uint32_t psp;
    uint32_t control;
    uint32_t primask;
    uint32_t basepri;
    uint32_t faultmask;
    uint32_t exc_return;
    uint32_t fault_kind;
} ls_cortex_m_saved_t;

#if defined(__cplusplus)
#define LS_CORTEX_M_STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define LS_CORTEX_M_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif

LS_CORTEX_M_STATIC_ASSERT(offsetof(ls_cortex_m_saved_t, msp) == 32u,
                          "fault assembly expects msp at offset 32");
LS_CORTEX_M_STATIC_ASSERT(offsetof(ls_cortex_m_saved_t, psp) == 36u,
                          "fault assembly expects psp at offset 36");
LS_CORTEX_M_STATIC_ASSERT(offsetof(ls_cortex_m_saved_t, exc_return) == 56u,
                          "fault assembly expects exc_return at offset 56");
LS_CORTEX_M_STATIC_ASSERT(offsetof(ls_cortex_m_saved_t, fault_kind) == 60u,
                          "fault assembly expects fault_kind at offset 60");
LS_CORTEX_M_STATIC_ASSERT(sizeof(ls_cortex_m_saved_t) == 64u,
                          "fault assembly expects a 64-byte saved context");

#undef LS_CORTEX_M_STATIC_ASSERT

typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
    uint32_t fpscr;
    uint32_t flags;
} ls_cortex_m_exception_frame_t;

extern volatile ls_cortex_m_saved_t ls_cortex_m_saved_context;
extern volatile uint32_t ls_cortex_m_fault_active;
extern uintptr_t ls_cortex_m_emergency_stack_top;

void ls_cortex_m_init(void);
bool ls_cortex_m_emergency_stack_ok(void);
size_t ls_cortex_m_emergency_stack_usage(void);
size_t ls_cortex_m_emergency_stack_high_water_mark(void);

/* Bounds are exclusive at the top and may be omitted for an unused stack. */
ls_result_t ls_cortex_m_stack_bounds_set(const void *msp_lower, const void *msp_upper,
                                         const void *psp_lower, const void *psp_upper);
void ls_cortex_m_stack_bounds_clear(void);

ls_result_t ls_cortex_m_configure_emergency_stack_mpu(uint8_t region_number);
void ls_cortex_m_enable_configurable_faults(bool secure_fault);
void ls_cortex_m_configure_fpu_lazy_stacking(bool enabled);

/* The decoder reads the frame only after it is wholly inside configured bounds. */
ls_result_t ls_cortex_m_decode_exception_frame(const uint32_t *raw_frame,
                                               const volatile ls_cortex_m_saved_t *saved,
                                               ls_cortex_m_exception_frame_t *frame);

void ls_cortex_m_fault_from_saved(const uint32_t *raw_frame,
                                  const volatile ls_cortex_m_saved_t *saved);
void ls_cortex_m_fault_recursive(uint32_t fault_kind, uint32_t exc_return, uint32_t msp,
                                 uint32_t psp);

void ls_cortex_m_hardfault_handler(void);
void ls_cortex_m_memmanage_handler(void);
void ls_cortex_m_busfault_handler(void);
void ls_cortex_m_usagefault_handler(void);
void ls_cortex_m_nmi_handler(void);
void ls_cortex_m_securefault_handler(void);

#endif
