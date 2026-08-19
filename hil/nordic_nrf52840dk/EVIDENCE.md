# Nordic nRF52840 DK — Renode evidence

This file is a procedure, not a pre-existing qualification claim. A real
run must produce a JSON transcript in `evidence/` using `run-renode.ps1` or the
Robot test. The result is valid only for the exact firmware ELF and source
revision recorded by that run.

Renode scope:

- nRF52840 board model loads successfully;
- the firmware vector table is accepted;
- UART0/EasyDMA emits the armed marker;
- an undefined instruction reaches `UsageFault_Handler`;
- the handler confirms `CFSR.UNDEFINSTR` and emits the pass marker.

The generated JSON also records the ELF SHA-256, Robot Framework XML SHA-256,
Renode log SHA-256, and links to the raw Robot/HTML artifacts. The markers are
read from `robot_output.xml`; a Renode process exit by itself is not evidence.

The installed Renode 1.16.1 does not expose synchronous HardFault injection to
the test interface, so this fixture does not claim that an instruction caused
a HardFault. It does not qualify physical Nordic silicon, retained flash,
brownout, watchdog calibration, radio behavior, or final production
linker/toolchain integration. Those still require physical HIL evidence.
