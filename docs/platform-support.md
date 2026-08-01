# Platform support and qualification

Latch separates a portable evidence runtime from architecture fault entry and vendor storage/reset integration. That distinction is intentional: compiling the portable runtime for a family is useful, but it is not evidence that a fault handler, linker placement, Flash geometry, or reset register is correct on a particular board.

## Support tiers

| Tier | Meaning | Evidence required before a product relies on it |
| --- | --- | --- |
| Qualified | A reproducible hardware-in-the-loop (HIL) record exists for the exact board, SDK, compiler, linker script and scenario. | Repeat the recorded HIL procedure after any relevant hardware, SDK, linker or bootloader change. |
| Integration boundary | The source interface is implemented and has compile-time and/or host tests. It is **not** a board qualification. | Own the vendor register values, Flash backend, vector/trap installation and HIL matrix for the selected target. |
| Portable/cooperative | The heap-free runtime API can be used from normal firmware code. No automatic fault-entry claim is made. | Measure the linked image and run the application's error, reset and storage tests. |

The only qualified capture-to-durable-ACK path at this time is the recorded ESP32-D0WD-V3 / ESP-IDF 5.5.0 fixture. It captures an event in normal runtime before an intentional panic, then proves reboot recovery and delivery; it does not qualify automatic Xtensa panic-frame capture. See [the fixture and exact boundary](../hil/esp32_relay/README.md). It is evidence for that configuration only.

## Architecture coverage

| Architecture or environment | Tier | What is included | Important boundary |
| --- | --- | --- | --- |
| Cortex-M0 through M33 | Integration boundary | Emergency-stack fault entry and normalized Cortex-M context, including basic/extended FPU frame decoding where applicable. | Install vectors, select secure/non-secure placement, define stack bounds and qualify the selected core/toolchain. |
| RV32 machine mode | Integration boundary | `mscratch` emergency-stack trap entry and machine CSR/context capture. | The application owns `mtvec`, PMP and reset behavior; real trap ownership must be tested on hardware. |
| RV64 machine mode | Integration boundary | A separate RV64 trap/context path preserves 64-bit general registers and machine CSRs in the retained snapshot/LEP extension. The supplied CMake RV64 target requires `LS_ENABLE_WIDE_CONTEXT=1`. | It needs an RV64 linker script, ABI-specific trap-vector ownership and HIL before it can be treated as qualified. |
| Xtensa / ESP-IDF | Integration boundary; one ESP32 normal-runtime/recovery fixture qualified | Normalized frame capture and an opt-in ESP-IDF 5.5 panic wrapper. The fixture qualifies normal-runtime capture, reboot recovery and delivery, not the wrapper. | Xtensa frame layout and the panic wrapper are vendor-ABI-sensitive. Rebuild and rerun HIL on every ESP-IDF change before treating automatic panic capture as qualified. |
| Linux x86_64 and AArch64 (Cortex-A under Linux) | Integration boundary | A configured fatal handler writes one fixed-size native raw record to a caller-provided nonblocking pipe/FIFO on an alternate stack; normal runtime can use the file-backed LEP spool. AArch64 is a Linux user-space signal integration, not a bare-metal Cortex-A exception vector. | The raw handler record is not LEP and does not prove filesystem durability. The product owns supervision, descriptor lifecycle, conversion, permissions and crash policy. |
| Bare-metal Cortex-A / AArch32 | Not supplied | The portable runtime can be integrated by an application. | Latch does not ship a bare-metal exception-vector, MMU/cache, EL3/EL2 or secure-monitor port. |
| AVR, PIC and MSP430-class 8/16-bit MCUs | Portable/cooperative | The `LS_CONSTRAINED_PROFILE` baseline is selected automatically on AVR, and a compile/link-checked Arduino Mega 2560 example captures one RAM-backed explicit event. PIC/MSP430 builds can select the same profile through global compiler definitions. | These families do not share Cortex-M-style fault entry, and no automatic crash/Flash port is claimed. AVR is not hardware-tested; size must be measured in the final linked firmware. Cryptography and even the constrained buffers may be unsuitable for a device with only a few KiB of RAM. |

## Vendor port surfaces

The following reset-reason adapters are source-level integration boundaries. They accept raw vendor reset-register values and normalize them into the portable `ls_reset_reason_t` API without requiring a vendor SDK in the core. Their bit mappings are host-tested, but no entry below is a hardware qualification.

| Vendor families | Library target | What the adapter covers | Product work still required |
| --- | --- | --- | --- |
| Nordic nRF52/nRF53 | `latch-port-nordic` | Reset-reason normalization for the Nordic RESETREAS model. | Retained region, NVMC/Flash backend, vector table and BLE lifecycle. |
| NXP i.MX RT105x / Kinetis | `latch-port-nxp` | Reset-status normalization for the i.MX RT105x SRSR and Kinetis SRS0/SRS1 layouts. | Exact register map, FlexSPI/Flash backend, cache/linker behavior and vectors. |
| Microchip SAM D/E / SAM | `latch-port-microchip` | Reset-cause normalization for SAM D/E RCause and SAM RSTTYP layouts. | Family-specific startup, NVM controller, linker and fault vectors. |
| TI Tiva / MSP432E4 | `latch-port-ti` | Reset/watchdog/brownout flag normalization for the shared SYSCTL RESC layout. | DriverLib/startup choice, Flash backend and exception vector ownership. |
| Silicon Labs EFM32 Series 0 / EFM32-EFR32 Series 2 | `latch-port-silabs` | Reset-cause normalization for the supported RMU/EMU layouts. | EMU/Flash configuration, Gecko SDK lifecycle and vectors. |

The generic Cortex-M fault library is reusable by the Cortex-M vendor rows; it is deliberately kept separate from reset and Flash policy. Use the per-port headers for the documented raw-mask inputs instead of copying assumptions from a different chip revision.

## Adding a board to the qualified tier

Use the [hardware qualification checklist](hil.md) and capture the board, revision, compiler, SDK, linker script, bootloader, retained-memory section, Flash geometry, reset flags and every executed scenario. A concise community report is more valuable than a broad compatibility claim: it gives the next integrator a reproducible starting point and lets maintainers promote only the evidence that exists.
