# Hardware compatibility matrix

This dashboard separates physical qualification from compilation, host tests,
and emulator evidence. A green entry applies only to the exact board,
toolchain, firmware boundary, and scenarios linked in its evidence. It is not a
family-wide safety certification.

<!-- qualification-matrix:start -->
| Platform | MCU / board | Toolchain | Fault handler | Flash backend | Status | Last physical HIL |
| --- | --- | --- | --- | --- | --- | --- |
| ESP32 | ESP32-D0WD-V3 rev 3.1 / ESP32 DevKit (4 MB flash) | ESP-IDF 5.5.0 | ⚠️ Integration boundary | ✅ Qualified | Qualified (limited scope) | [2026-07-29](../hil/esp32_relay/EVIDENCE.md) |
<!-- qualification-matrix:end -->

## Evidence rules

- **Qualified** requires a dated, reviewable physical HIL record for the exact
  board and toolchain. Only a maintainer may promote an entry after reviewing
  that evidence.
- **Emulator-tested** proves an architecture/toolchain path ran in an emulator;
  it does not validate silicon errata, reset registers, flash timing, power
  loss, linker retention, or a vendor bootloader.
- **Host-tested** and **compile-tested** are regression evidence only.
- A dash in “Last physical HIL” means no accepted physical run is recorded.

The source of truth is
[`hil/qualification-matrix.json`](../hil/qualification-matrix.json). Run
`python tools/check_hil_matrix.py` after changing it. Evidence submission and
destructive-test requirements are documented in [the HIL guide](hil.md).

## Current qualification boundary

The ESP32 row covers explicit capture before an intentional panic, mirrored
flash persistence, reboot recovery, UART stream framing, and a durable ACK. It
does **not** qualify the automatic Xtensa panic-frame wrapper. No other row is
currently physical-hardware-qualified.

## Emulator-tested (Renode)

The following targets have **Renode emulator evidence** under
[`hil/emulator/evidence/`](../hil/emulator/evidence/). Emulator runs validate the
architecture/toolchain path, vector table and fault entry on the Renode board
models. They are **explicitly not physical-board qualification** and do not
validate silicon errata, reset registers, flash timing, power loss, or a vendor
bootloader. Physical HIL is required before any of these may be called
"qualified" (see `hil/AGENTS.md`).

| Platform | MCU / board | Toolchain | Evidence |
| --- | --- | --- | --- |
| STM32 | STM32F407VG / STM32F4-Discovery | Renode + Arm GNU 14.2 | [stm32f407](../hil/emulator/evidence/stm32f407/) |
| SAM4S | SAM4S16C / SAM4S Xplained | Renode + Arm GNU 14.2 | [sam4s](../hil/emulator/evidence/sam4s/) |
| Apollo4 | Apollo4 / Ambiq Apollo4 EVB | Renode + Arm GNU 14.2 | [apollo4](../hil/emulator/evidence/apollo4/) |
| MAX32652 | MAX32652 / MAX32652 EVKIT | Renode + Arm GNU 14.2 | [max32652](../hil/emulator/evidence/max32652/) |
| K6xF | MK66FN2M0VMD18 / NXP FRDM-K66F | Renode + Arm GNU 14.2 | [k6xf](../hil/emulator/evidence/k6xf/) |
| EFR32MG12 | EFR32MG12P / SLTB004A | Renode + Arm GNU 14.2 | [efr32mg12](../hil/emulator/evidence/efr32mg12/) |
| STM32 | STM32F103RB / Nucleo-F103RB | Renode + Arm GNU 14.2 | [stm32f103](../hil/emulator/evidence/stm32f103/) |
| CC2538 | CC2538SF53 / TI CC2538DK | Renode + Arm GNU 14.2 | [cc2538](../hil/emulator/evidence/cc2538/) |
| STM32 | STM32F072RB / STM32F072B Discovery | Renode + Arm GNU 14.2 | [stm32f072](../hil/emulator/evidence/stm32f072/) |
| ATSAMD21 | ATSAMD21J18A / SAM D21 Xplained | Renode + Arm GNU 14.2 | [atsamd21](../hil/emulator/evidence/atsamd21/) |
| S32K118 | S32K118 / NXP S32K118EVB | Renode + Arm GNU 14.2 | [s32k118](../hil/emulator/evidence/s32k118/) |
| STM32 | STM32F746NG / STM32F746G Discovery | Renode + Arm GNU 14.2 | [stm32f746](../hil/emulator/evidence/stm32f746/) |
| STM32 | STM32H743ZI / Nucleo-H743ZI | Renode + Arm GNU 14.2 | [stm32h743](../hil/emulator/evidence/stm32h743/) |
| SAME70 | SAME70Q21 / SAM E70 Xplained | Renode + Arm GNU 14.2 | [sam_e70](../hil/emulator/evidence/sam_e70/) |
| i.MX RT | MIMXRT1064 / NXP MIMXRT1064-EVK | Renode + Arm GNU 14.2 | [imxrt1064](../hil/emulator/evidence/imxrt1064/) |
| STM32 | STM32L552ZE / Nucleo-L552ZE | Renode + Arm GNU 14.2 | [stm32l552](../hil/emulator/evidence/stm32l552/) |
| STM32 | STM32WBA52CG / Nucleo-WBA52CG | Renode + Arm GNU 14.2 | [stm32wba52](../hil/emulator/evidence/stm32wba52/) |
| i.MX RT | MIMXRT595 / NXP MIMXRT595-EVK | Renode + Arm GNU 14.2 | [imxrt500](../hil/emulator/evidence/imxrt500/) |
| i.MX RT | MIMXRT798S / NXP MIMXRT700-EVK | Renode + Arm GNU 14.2 | [imxrt700](../hil/emulator/evidence/imxrt700/) |
| RA6M5 | R7FA6M5BH / Renesas EK-RA6M5 | Renode + Arm GNU 14.2 | [ra6m5](../hil/emulator/evidence/ra6m5/) |
| OpenTitan | Ibex / OpenTitan Earl Grey | Renode + RISC-V GNU 14.2 | [opentitan](../hil/emulator/evidence/opentitan/) |
| VEGA | RI5CY / VEGAboard | Renode + RISC-V GNU 14.2 | [vegaboard](../hil/emulator/evidence/vegaboard/) |
| Kendryte | K210 / Sipeed MAIX Bit | Renode + RISC-V GNU 14.2 | [k210](../hil/emulator/evidence/k210/) |

The authoritative, machine-checked record is
[`hil/qualification-matrix.json`](../hil/qualification-matrix.json) (`python tools/check_hil_matrix.py`).
