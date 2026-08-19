# Nordic nRF52840 DK Renode HIL

This fixture validates the nRF52840 Cortex-M4 UsageFault path in Renode. It
builds a small bare-metal image, loads it on Renode's
`nrf52840dk_nrf52840.repl`, observes the UART, and requires both markers:

```text
HIL:ARMED:USAGEFAULT
HIL:PASS:USAGEFAULT
```

This is emulator evidence, not physical-board qualification. Renode proves
the selected CPU/board model, vector table, UART/EasyDMA path, and UsageFault
control flow. The firmware checks `CFSR.UNDEFINSTR` before emitting PASS. The
installed Renode 1.16.1 does not expose synchronous HardFault injection to the
test interface, so this fixture does not claim HardFault generation. It also
does not prove Nordic silicon reset behavior, flash retention, power failure
behavior, or the final board/toolchain integration.

On Linux/CI, the equivalent commands are:

```sh
mkdir -p build/emulator
arm-none-eabi-gcc hil/nordic_nrf52840dk/nrf52840_hil.c \
  -mcpu=cortex-m4 -mthumb -Os -ffreestanding -fno-builtin -nostdlib \
  -Wl,--gc-sections,-T,hil/nordic_nrf52840dk/nrf52840.ld \
  -o build/emulator/latch-renode-nrf52840.elf
build/renode/renode-test hil/nordic_nrf52840dk/nrf52840dk.robot
```

With the desktop Renode installation on Windows, run
`run-renode.ps1 -Firmware <path-to-elf>`. A successful run writes a timestamped
JSON transcript under `evidence/`; do not hand-edit that file.
