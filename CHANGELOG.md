# Changelog

All notable changes to Latch are recorded here. The project follows [Semantic Versioning](https://semver.org/) while it is pre-1.0, and the format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Copyable ESP32/ESP-IDF 5.5 crash tutorial with flash-backed capture,
  intentional panic, reboot recovery, retained Xtensa registers and an
  NVS-committed durable ACK.
- Public durable Python reference collector, bounded C stream-frame parser,
  stream/LSAK fuzz target and JSON output for `latch-dump`.
- Standalone `add_subdirectory` and `FetchContent` consumers, extracted CPack
  smoke test, Zephyr module/native sample and PlatformIO link check.
- PlatformIO and crates.io manifests, registry-package CI, Rust crate metadata,
  threat model and registry publication runbook.
- Version 3 retained snapshots with Xtensa A0-A15/special registers and a
  host-tested fault-safe normalized-frame adapter.
- RV64 retained context with an additive, explicitly complete-or-unavailable
  CPU64 LEP extension; vendor reset-reason adapters for Nordic, NXP,
  Microchip, TI and Silicon Labs.
- Native Arduino and ESP-IDF Component Manager layouts, a cooperative Arduino
  example, deterministic embedded-package checks, static footprint reports,
  and a release-metadata consistency gate.
- A constrained 8/16-bit cooperative profile with an Arduino Mega 2560
  compile/link example; it is explicitly not an automatic fault or persistent
  crash-recovery port.
- A bounded `latch-dump` host decoder that accepts files or stdin, names LEP
  fields and keeps encrypted payloads metadata-only without a decryption key.
- Linux alternate-stack fatal-signal handoff using a single nonblocking write
  of a CRC-protected raw record, with full-width x86_64/AArch64 state.
- Transactional secure-storage key rotation with failure rollback of in-memory
  key state.
- A validated public hardware compatibility matrix with dated HIL evidence,
  QEMU Cortex-M and Renode RV32 fault-injection jobs, and expanded physical HIL
  entry points for HardFault and stack-canary scenarios.
- Deterministic spool corruption/interrupted-write stress coverage and hosted,
  CI-executed OTA, critical-redaction and low-power reference designs.
- Explicit 1.0/LTS qualification, observation-window and backport policy plus
  an independent security-review invitation and public report template.

### Changed

- Coverage collection now includes branches and uses explicit project/patch
  targets; LSAK parsing rejects non-canonical sizes, status and reserved bytes.
- Curated contributor issues no longer retain the contradictory `triage` label,
  and first issues include effort, size, hardware and maintainer-help metadata.

### Fixed

- Initialized the spool append slot before the checked lookup, avoiding a GCC
  release-build false-positive that became fatal under `-Werror`.
- Applied the repository formatter drift reported by maintenance automation.
- Kept RV32 trap capture on the retained fault-safe path instead of entering
  normal runtime/storage logic, and removed normal Latch runtime work from the
  Linux fatal-signal handler.

## [0.2.0] - 2026-07-29

### Added

- Heap-free portable C11 runtime with bounded capture, breadcrumbs, metrics, logs, health, performance, dumps, and policy controls.
- Deterministic LEP v1 envelopes with public C and Rust validation, golden vectors, CRC, authentication, encryption, compression, and replay protection.
- Retained crash recovery, transactional spool records, memory/Flash storage adapters, interrupted-write simulation, retries, and acknowledged delivery.
- Cortex-M, RV32, Xtensa, Linux, STM32, RP, ESP-IDF, FreeRTOS, and Zephyr integration boundaries.
- C++ wrapper, Rust `#![no_std]` SDK, host decoder, fuzz targets, hardware-in-the-loop entry points, packages, SBOM, and provenance automation.
- Runnable host demo that captures an envelope and validates it with `latch-dump`.
- Public roadmap, contribution guide, issue forms, support policy, and community code of conduct.
- Reproducible physical ESP32 HIL fixture with a dedicated flash partition,
  intentional panic/reboot recovery, public stream/ACK contract and retained
  evidence from validation against the private production LastState Relay.

### Changed

- Reworked the README around the post-reset evidence problem, a three-call example, and a 60-second demo.
- Kept reviewable source on the default branch and moved source compaction to a verified distribution artifact.
- Aligned default-branch workflows with `prod` and enabled security-result publication.

### Fixed

- Instrumented the portable runtime itself during libFuzzer builds instead of instrumenting only the thin fuzz harnesses, and seeded all three fuzz targets deterministically.
- Made the previously unbuilt host example use a valid build ID and enough retained storage for the configured spool.
- Removed signed-overflow undefined behavior from delta decoding when valid values cross the signed 32-bit boundary.
- Corrected the ESP32 fixture's flash-size/vendor configuration, panic-phase
  selection and ESP-IDF component discovery after exercising them on physical
  hardware.
- Avoided a constant-false ChaCha20 length warning on 32-bit targets while
  retaining the overflow guard on wider `size_t` implementations.

### Security

- Documented provisioning, redaction, authenticated storage, transport, and independent-review requirements.
- Added repository settings guidance for private vulnerability reporting, secret scanning, and push protection.
