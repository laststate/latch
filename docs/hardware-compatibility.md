# Hardware compatibility matrix

This dashboard separates physical qualification from compilation, host tests,
and emulator evidence. A green entry applies only to the exact board,
toolchain, firmware boundary, and scenarios linked in its evidence. It is not a
family-wide safety certification.

<!-- qualification-matrix:start -->
| Platform | MCU / board | Toolchain | Fault handler | Flash backend | Status | Last physical HIL |
| --- | --- | --- | --- | --- | --- | --- |
| ESP32 | ESP32-D0WD-V3 rev 3.1 / ESP32 DevKit (4 MB flash) | ESP-IDF 5.5.0 | ⚠️ Integration boundary | ✅ Qualified | Qualified (limited scope) | [2026-07-29](../hil/esp32_relay/EVIDENCE.md) |
| STM32 | STM32F4xx / Product-selected board | Arm GNU Toolchain (CI cross-build) | 🧪 Emulator-tested | ⚠️ Product integration | In validation | — |
| RP2040 | RP2040 / Raspberry Pi Pico family | Pico SDK (consumer-owned) | 🧪 Host-tested | ❌ Not supplied | Not qualified | — |
| Nordic | nRF52840 / nRF52840 DK | Zephyr SDK (CI build) | 🧪 Host-tested | ⚠️ Product integration | Compile-tested | — |
| RISC-V | RV32IMAC / SiFive FE310 model | riscv64-unknown-elf GCC + Renode 1.16.1 | 🧪 Emulator-tested | ❌ Not tested | Emulator-tested | — |
| Linux | x86_64 / GitHub-hosted runner | GCC and Clang | 🧪 Host-tested | 🧪 Host-tested | Host-tested | — |
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
