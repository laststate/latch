.syntax unified
.thumb

.extern ls_cortex_m_fault_from_saved
.extern ls_cortex_m_fault_recursive
.extern ls_cortex_m_fault_active
.extern ls_cortex_m_saved_context
.extern ls_cortex_m_emergency_stack_top

.macro LS_HANDLER name, kind
  .global \name
  .type \name, %function
\name:
  movs r2, #\kind
  b ls_cortex_m_fault_common
.endm

LS_HANDLER ls_cortex_m_hardfault_handler, 1
LS_HANDLER ls_cortex_m_memmanage_handler, 2
LS_HANDLER ls_cortex_m_busfault_handler, 3
LS_HANDLER ls_cortex_m_usagefault_handler, 4
LS_HANDLER ls_cortex_m_nmi_handler, 5
LS_HANDLER ls_cortex_m_securefault_handler, 6

.type ls_cortex_m_fault_common, %function
ls_cortex_m_fault_common:
  ldr r3, =ls_cortex_m_fault_active
  ldr r1, [r3]
  cmp r1, #0
  beq ls_cortex_m_fault_first_entry
  cmp r1, #1
  beq ls_cortex_m_fault_recursive_entry
  b ls_cortex_m_fault_terminal

ls_cortex_m_fault_first_entry:
  movs r1, #1
  str r1, [r3]

  ldr r3, =ls_cortex_m_saved_context
  str r4, [r3, #0]
  str r5, [r3, #4]
  str r6, [r3, #8]
  str r7, [r3, #12]
  mov r0, r8
  str r0, [r3, #16]
  mov r0, r9
  str r0, [r3, #20]
  mov r0, r10
  str r0, [r3, #24]
  mov r0, r11
  str r0, [r3, #28]
  mrs r0, msp
  str r0, [r3, #32]
  mrs r0, psp
  str r0, [r3, #36]
  mrs r0, control
  str r0, [r3, #40]
  mrs r0, primask
  str r0, [r3, #44]
#if defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_8M_BASE__)
  movs r0, #0
  str r0, [r3, #48]
  str r0, [r3, #52]
#else
  mrs r0, basepri
  str r0, [r3, #48]
  mrs r0, faultmask
  str r0, [r3, #52]
#endif
  str lr, [r3, #56]
  str r2, [r3, #60]

#if defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_8M_BASE__)
  mov r0, lr
  movs r1, #4
  tst r0, r1
  beq 1f
  mrs r0, psp
  b 2f
1:
  mrs r0, msp
2:
#else
  tst lr, #4
  ite eq
  mrseq r0, msp
  mrsne r0, psp
#endif
  mov r1, r3
  cpsid i
  ldr r3, =ls_cortex_m_emergency_stack_top
  ldr r3, [r3]
  msr msp, r3
  b ls_cortex_m_fault_from_saved

ls_cortex_m_fault_recursive_entry:
  movs r1, #2
  str r1, [r3]
  mov r0, r2
  mov r1, lr
  mrs r2, msp
  mrs r3, psp
  cpsid i
  ldr r4, =ls_cortex_m_emergency_stack_top
  ldr r4, [r4]
  msr msp, r4
  b ls_cortex_m_fault_recursive

ls_cortex_m_fault_terminal:
  cpsid i
  ldr r4, =ls_cortex_m_emergency_stack_top
  ldr r4, [r4]
  msr msp, r4
1:
  b 1b
