# Emulator fault-injection tests

These tests execute real exception/trap instructions in CPU emulators. They
are stronger than a host-only context model, but they are still not physical
hardware qualification.

- QEMU `mps2-an385` executes an escalated Cortex-M3 HardFault, a watchdog-reset
  control-flow model using NMI, and a corrupted stack-canary detection path.
- Renode's SiFive FE310 model executes an illegal RV32 instruction and verifies
  that machine trap dispatch reaches the installed handler.

The watchdog case deliberately does not claim that QEMU models a selected
vendor watchdog peripheral. The stack case validates the canary-to-fault path;
MPU guard regions and real stack exhaustion remain physical HIL scenarios.
Run them through `.github/workflows/emulator-stress.yml` or reproduce the
compiler/emulator commands from that workflow.
