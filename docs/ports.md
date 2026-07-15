# Port integration

## Cortex-M

The assembly handler preserves R4–R11, MSP, PSP, CONTROL, PRIMASK, BASEPRI, FAULTMASK and EXC_RETURN before moving to the emergency stack. The C decoder selects the basic or extended FPU frame, captures SCB fault registers and passes a normalized context to the portable capture engine. Baseline cores omit unavailable BASEPRI/FAULTMASK instructions.

Install handlers for HardFault, MemManage, BusFault, UsageFault, NMI and SecureFault where the selected core implements them. Call `ls_stack_bounds_set()` with the authorized task/interrupt stack bounds if stack snapshots are desired.

## RISC-V

The RV32 trap entry exchanges the failing stack pointer with the emergency pointer in `mscratch`, preserves x0–x31 and the machine trap CSRs, and enters C on the emergency stack. The application owns `mtvec`, PMP policy and reset behavior.

## Xtensa and ESP-IDF

Xtensa exception frames vary between windowed and call0 ABIs, so the port accepts a normalized `ls_xtensa_frame_t`. An ESP-IDF panic hook should translate the SDK frame and call `ls_xtensa_capture_frame()`.

## STM32 and RP2040/RP2350

Reset ports accept register addresses and masks instead of depending on a vendor HAL. This keeps the libraries freestanding and lets one implementation cover RCC variants and both RP reset controllers.

## Linux

The Linux port installs fatal signal handlers and captures signal number, fault address, instruction pointer and stack pointer. The file backend provides persistent storage and writes individual `.lst` envelopes for offline analysis.
