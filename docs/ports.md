# Port integration

## Cortex-M

The assembly handler preserves R4–R11, MSP, PSP, CONTROL, PRIMASK, BASEPRI, FAULTMASK and EXC_RETURN before moving to the emergency stack. The C decoder selects the basic or extended FPU frame, captures SCB fault registers and passes a normalized context to the portable capture engine. Baseline cores omit unavailable BASEPRI/FAULTMASK instructions.

Install handlers for HardFault, MemManage, BusFault, UsageFault, NMI and SecureFault where the selected core implements them. Configure `ls_cortex_m_stack_bounds_set()` with the authorized MSP and PSP ranges so the fault decoder can validate the stacked exception frame. If normal-runtime stack snapshots are also desired, separately call `ls_stack_bounds_set()` with the authorized application range.

## RISC-V

The RV32 trap entry exchanges the failing stack pointer with the emergency pointer in `mscratch`, preserves x0–x31 and the machine trap CSRs, and enters C on the emergency stack. The application owns `mtvec`, PMP policy and reset behavior.

RV64 uses a separate trap entry and a CRC-protected retained wide-context sidecar. The supplied CMake target requires `LS_ENABLE_WIDE_CONTEXT=1`; its LEP output appends a CPU64 TLV so a receiver never has to infer upper address/register bits from a 32-bit CPU record. Existing LEP v1 readers skip the additive TLV safely. Both RISC-V ports are integration boundaries: install the vector, preserve `mscratch` ownership and qualify the exact ABI/linker script on hardware.

## Xtensa and ESP-IDF

Xtensa exception frames vary between windowed and call0 ABIs, so the port accepts a normalized `ls_xtensa_frame_t`. Normal-context integrations call `ls_xtensa_capture_frame()`; panic handlers call the retained-memory-only `ls_xtensa_capture_minimal_frame()`.

The copyable ESP32 example contains an opt-in ESP-IDF 5.5 adapter. Because
ESP-IDF does not expose a stable application panic hook, it uses the GNU linker
`--wrap=esp_panic_handler` contract, performs only a bounded retained-memory
write, then delegates to ESP-IDF. Treat the exact vendor ABI as part of the
board/toolchain qualification and do not carry the wrapper across ESP-IDF
upgrades without rebuilding and rerunning HIL.

## STM32 and RP2040/RP2350

Reset ports accept register addresses and masks instead of depending on a vendor HAL. This keeps the libraries freestanding and lets one implementation cover RCC variants and both RP reset controllers.

## Nordic, NXP, Microchip, TI and Silicon Labs

`ports/nordic`, `ports/nxp`, `ports/microchip`, `ports/ti` and `ports/silabs` provide small reset-reason normalization adapters. They intentionally take register pointers/masks rather than pulling in an SDK: nRF52/nRF53, i.MX RT105x/Kinetis, SAM D/E RCause and SAM RSTTYP, Tiva/MSP432E4, EFM32 Series 0 and EFM32/EFR32 Series 2 profiles are covered by host tests.

They do not install a fault handler, reserve retained RAM, configure Flash or know a board's clock/startup sequence. Treat the profiles as a documented starting point and retain the exact device-header values/HIL evidence with the product.

## Linux

The Linux signal boundary is deliberately split from normal Latch runtime work. When configured with a caller-owned alternate stack and a pre-opened nonblocking pipe/FIFO, the fatal handler writes one fixed-size, CRC-protected native raw record and exits. It does not construct LEP, invoke callbacks, access Latch storage, allocate memory, or try to make a filesystem durable from a signal context. The record preserves the signal metadata plus full-width x86_64 or AArch64 register state and can be handed to a supervisor for normal-process conversion.

The separate file backend is for normal runtime/spool use: initialize and preallocate it before capture, then use its callbacks as the persistent spool storage and synchronize them outside the fatal-signal handler. `ls_file_transport_send()` can separately export validated LEP envelopes as `.lst` files in a caller-owned directory. AArch64 support is Linux user space only, not a bare-metal Cortex-A exception, MMU, cache, EL3/EL2, or secure-monitor port. See [Linux signal capture](linux-signal-capture.md) for the exact API and safety boundary.

See [platform support](platform-support.md) for the evidence tier and board/product responsibilities for every port family.
