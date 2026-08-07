# Validation report — 2026-08-07 — v0.3.0

This report records validation performed on the supplied source snapshot after the
AUV-observability and product-readiness hardening pass. The input snapshot did not
contain Git metadata, so these results are tied to the packaged source bytes and the
`SOURCE_MANIFEST.sha256` produced for the final archive rather than to a commit SHA.

## Local software evidence

| Check | Result |
| --- | --- |
| Full CTest, host build | **58/58 passed** |
| Clang ASan + UBSan CTest | **58/58 passed** |
| GCC Release `-Werror` CTest | **58/58 passed** |
| Clang Release `-Werror` CTest | **58/58 passed** |
| Production line coverage (`src/`, `arch/`, `ports/`) | **5427/5788 = 93.76%** |
| Production branch coverage (`src/`, `arch/`, `ports/`) | **3292/4105 = 80.19%** |
| Mutation smoke | **3/3 critical mutants killed** |
| Safety-C source policy | passed across `src/`, `arch/`, `ports/` |
| Documentation checks | passed |
| Release metadata | passed for **v0.3.0** |
| HIL evidence-matrix validation | passed; 6 platform entries |
| LEP public/invalid vectors | passed; **7 invalid vectors rejected** |
| Compact-source tests | **7/7 passed** |
| Workflow YAML parse | passed; **29 workflow files** |
| CPack external C/C++/port/Linux consumer smoke | passed |
| Python tool/test byte-compilation | passed |
| Runtime TODO/FIXME/stub scan | no unresolved markers found in `src/`, `arch/`, `ports/`, `include/` |
| Heap/unsafe-string source audit | no forbidden dynamic-allocation/unsafe-string calls found by the Safety-C gate |

`clang-tidy` and `cppcheck` were not installed in this local execution environment;
the repository retains CI jobs/scopes for static analysis. Their absence here is not
reported as a local pass.

## Coverage policy

`tools/check_gcov_coverage.py` aggregates gcov data by production source location
across all instrumented test executables. Test source is not counted and no production
files were excluded to inflate the result. CI fails below **93% line** or **80% branch**
coverage across `src/`, `arch/` and `ports/`.

The v0.3.0 work added substantial new production code, initially reducing aggregate
coverage. Edge, recovery, concurrency, replay, storage, OTA, provisioning, black-box,
mission, environment and supervisor tests were added until the expanded runtime again
passed the existing gate.

## AUV/runtime assurance additions

- CRC-protected retained black-box recorder with bounded recent-record export,
  freeze/thaw behavior and quiet/normal/anomaly profiles.
- Mission/dive/node context, 128-bit cross-node incident IDs, synchronized UTC anchors
  and build-aware crash/event fingerprints.
- AUV environment evidence for pressure/depth, temperature, humidity, vibration,
  leak/water-ingress and pressure-sensor state.
- Health supervisor for watchdog/deadline staleness, brownout/power, battery,
  temperature, heap, spool occupancy, boot loop, leak, vibration and sensor faults.
- RTOS/peripheral tracing for task, IRQ, mutex, DMA, state and link transitions.
- Separate normal/Critical/Emergency spool capacity with priority-first draining,
  operational drop/corruption/retry/transport counters and interrupted-write recovery.
- Provisioning lifecycle, secure-element attestation, rotation/revocation and fail-closed
  secure decommissioning.
- Generic authenticated A/B OTA orchestration with persistent pending/confirm/rollback
  state and monotonic anti-rollback floor.
- Named fault-injection points in storage programming, spool commit/send/ACK and
  transport paths; replay property testing and threaded black-box producer torture test.
- Fleet collector namespacing, persistent replay state, crash clustering, build
  comparison, canary promotion guardrails, symbol lookup and deterministic support
  bundles that exclude likely secret/key material by default.
- CPack install archives verified by building clean external C, C++, port and Linux
  consumers.

## Evidence that cannot be produced from this environment

This validation **does not certify an AUV or any physical target**. Before commercial
vehicle deployment, the exact board/MCU/flash/bootloader/toolchain/configuration still
requires physical HIL, pressure/thermal/vibration/shock/EMC testing, real brownout and
power-cut campaigns, mission-duration soak tests, measured WCET/stack/interrupt budgets,
flash-endurance qualification, product key provisioning, signed bootloader integration
and independent security/cryptographic review.

Latch must remain diagnostic evidence infrastructure. It must not be the sole controller
for propulsion shutdown, navigation, leak response, emergency ascent, watchdog reset or
vehicle recovery.

See `docs/auv-observability-runtime.md`, `docs/auv-commercial-readiness.md`,
`docs/production-readiness.md` and `docs/safety/` for the product-level boundary.
