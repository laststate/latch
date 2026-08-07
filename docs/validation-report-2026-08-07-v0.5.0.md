# Validation report — 2026-08-07 — v0.5.0

## Software validation completed in this environment

| Check | Result |
|---|---|
| Release build, development profile | PASS |
| Host test suite | 59/59 PASS |
| Commercial profile build | PASS |
| Commercial crypto fail-closed test | PASS |
| Critical mutation campaign | 40/40 killed; 100%; 0 survivors; 0 invalid |
| Project Readiness | PASS |
| Public API Compatibility | PASS (244 exported `ls_*` API names inventoried) |
| Documentation checks | PASS |
| Release metadata 0.5.0 | PASS |

Production runtime source was not changed in v0.5.0 relative to v0.4.0; the previously validated production-source coverage gate remains 93% lines / 80% branches, with the v0.4.0 measured result 93.80% lines / 80.07% branches. CI continues to recompute and enforce the gate.

## Deliberately pending product evidence

The following are not software defects and cannot be truthfully closed without the selected vehicle hardware: physical HIL for the final board/toolchain/bootloader/flash combination; real brownout and power-cut campaign; pressure/temperature/vibration/EMC/EMI qualification; measured WCET and stack margins; secure-element manufacturing provisioning; and independent review of the final product integration.
