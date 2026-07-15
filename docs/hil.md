# Hardware-in-the-loop validation

The `Hardware in the loop` workflow is manual and targets a runner labelled `self-hosted` and `latch-hil`. Destructive tests are never scheduled on shared runners.

The board firmware must accept `HIL:RUN:<scenario>` over its control UART, emit `HIL:ARMED:<SCENARIO>` immediately before the fault, reboot, inspect the retained Latch event and hardware reset registers, then emit `HIL:PASS:<SCENARIO>`. Supported scenarios are brownout, watchdog, MPU, TrustZone, lazy FPU stacking, real Flash and reset registers.

`hil/cortex_m_hil.c` supplies fault triggers and a destructive Flash erase/program/readback check. Brownout is performed externally by a SCPI power supply. The runner configuration follows `hil/boards/stm32u5.json.example`; keep real COM ports and lab addresses outside the repository.

Required lab controls:

- a debug probe able to flash and recover the board;
- a dedicated UART;
- a current-limited programmable supply for brownout;
- a board with an independent hardware watchdog;
- an M33-class target for TrustZone and an FPU target for lazy stacking;
- a disposable Flash test sector outside the application, bootloader and Latch production spool.

Each port must map reset flags using vendor CMSIS definitions. STM32F4 has a concrete RCC CSR profile; H7/U5-style RCC RSR ports use `LS_STM32_RESET_PORT_FROM_RCC_RSR` with that family's CMSIS `RCC_RSR_*` masks in a translation unit that includes the vendor device header.
