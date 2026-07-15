#ifndef LASTSTATE_CORTEX_M_H
#define LASTSTATE_CORTEX_M_H

#include <stdbool.h>
#include <stdint.h>
#include "laststate/event.h"
typedef struct {
    uint32_t r4_r11[8];
    uint32_t msp,psp,control,primask,basepri,faultmask,exc_return,fault_kind;
} ls_cortex_m_saved_t;
extern ls_cortex_m_saved_t ls_cortex_m_saved_context;
extern uintptr_t ls_cortex_m_emergency_stack_top;
void ls_cortex_m_init(void);
bool ls_cortex_m_emergency_stack_ok(void);
ls_result_t ls_cortex_m_configure_emergency_stack_mpu(uint8_t region_number);
void ls_cortex_m_enable_configurable_faults(bool secure_fault);
void ls_cortex_m_configure_fpu_lazy_stacking(bool enabled);
void ls_cortex_m_fault_from_saved(const uint32_t *raw_frame,const ls_cortex_m_saved_t *saved);
void ls_cortex_m_hardfault_handler(void);
void ls_cortex_m_memmanage_handler(void);
void ls_cortex_m_busfault_handler(void);
void ls_cortex_m_usagefault_handler(void);
void ls_cortex_m_nmi_handler(void);
void ls_cortex_m_securefault_handler(void);
#endif
