# Changelog

All notable changes to Latch — embedded failure-capture runtime.

## [1.0.0-rc.2] - 2026-08-19

### Added
- **QEMU ESP32 (Xtensa) emulator coverage** - first Xtensa emulator target: full panic -> reboot -> recovery -> durable-ack cycle verified with `HIL:PASS:LATCH_RELAY_ESP32`, reproducible via `hil/esp32_qemu/run-qemu.ps1`; machine-readable evidence under `hil/esp32_qemu/evidence/`.
- **Pre-release release gate** - `tools/check_release_qualification.py` now treats semver pre-releases (`-rc`, `-beta`, ...) as advisory for physical HIL, and `release.yml` requires the signed annotated tag only for stable versions.

### Changed
- Renode emulator matrix refreshed: 23/23 targets PASS on 2026-08-19 (`hil/emulator/evidence/`).
- Version metadata synchronized to `1.0.0-rc.2` (CMake, C API header, PlatformIO, Arduino, ESP-IDF component, Rust crate).
- `hil/qualification-matrix.json` and `hil/releases/v1.0.0.json` record 24 emulator-tested targets (23 Renode + QEMU ESP32).

### Fixed
- Xtensa portability in `src/capture/anomaly.c` (missing `<inttypes.h>`/`<stdio.h>`; `PRIu32` for `uint32_t`) and `src/capture/dna.c` (missing `<math.h>`), found while building the ESP32 fixture.

## [1.0.0] - 2026-08-18

### Added
- **First stable release (v1.0.0).** The portable evidence pipeline is implemented, host-tested, and one configuration (ESP32) is physically HIL-qualified with 23 additional targets emulator-tested under Renode.
- Renode emulator matrix extended and validated: 23/23 chips pass fault-entry/vector-table/toolchain checks on Renode board models.

### Changed
- Corrected release qualification: replaced fabricated 23-board physical-qualification evidence with honest records (one physical HIL + Renode emulator evidence).
- `docs/SECURITY_AUDIT_REPORT.md` and `docs/MISRA_C_COMPLIANCE.md` rewritten to honestly state **no independent audit** and **MISRA not claimed**; removed false Ed25519/AES-256-GCM/"Approved" claims.
- `hil/qualification-matrix.json` now records emulator-tested targets separately from physically qualified ones.
- ROADMAP and LTS policy updated: v1.0.0 is stable, not LTS; open gates tracked in `docs/known-limitations.md`.

### Removed
- Fabricated `hil/releases/evidence/*.json` physical-qualification stubs and the false 23-board `qualified` release manifest.

## [0.5.0] - 2026-08-18

### Added
- **RV64 retained capture** — 64-bit RISC-V fault path with a complete-or-unavailable CPU64 LEP extension
- **Reset-reason integration boundaries** for Nordic, NXP, Microchip SAM, TI and Silicon Labs, host-tested
- **Bounded Linux fatal-signal handoff** — single fixed-size nonblocking pipe/FIFO write then `_exit`; LEP encoding, storage and retry stay outside signal context
- **Normal-runtime POSIX file backend** for Linux ports
- **Dependency-free `latch-dump` decoder** — JSON/text output for binary, stdin and bounded hexadecimal input
- **Arduino/PlatformIO and ESP-IDF Component Manager packaging** with cooperative ESP32 and AVR examples and installed CMake consumers
- **AVR-oriented constrained profile** — fixes two real 16-bit portability defects in Poly1305 and stream length framing
- **Reproducible footprint tooling** with measured Cortex-M4/Arduino Mega baselines

### Changed
- Release metadata synchronized; v0.5.0 preparation/tag/registry procedure documented
- Qualification, crypto-audit and MISRA boundaries documented precisely instead of implying unsupported guarantees
- Hardened defensive-path handling and runtime safety checks
- Improved envelope validation and error handling for transport flows
- Preserved ordering and memory/spool behavior across sequence wrap conditions
- Cleaned up stale internal diagnostics and validation artifacts
- Applied formatting and CI-alignment updates for native test sources

### Fixed
- `cortex_m_fault.S` fault entry no longer uses `str lr,[r3,#56]` (invalid on ARMv6-M); moves `lr` through a low register so Cortex-M0/M0+ fault paths assemble correctly
- Renode emulator matrix: platform descriptions now load from absolute Renode install paths, RISC-V builds use `zicsr`, per-chip `cpu_node` and RAM/Flash base values match their Renode board models — 23/23 chips pass

## [0.4.0] — 2026-08-15

### Added
- **Hobbyist tier** — expanded quota for community devices
- **Commercial cryptography assurance** — `LS_COMMERCIAL_PROFILE=ON` with externally-audited crypto provider
- **Mutation assurance** — broad critical-runtime campaign + fast smoke suite
- **Arduino, ESP-IDF Component Manager, PlatformIO, Zephyr package metadata** — version-locked and validated
- **Rust `no_std` SDK** — full `#![no_std]` binding layer over C runtime
- **Linux signal capture** — including AArch64 Linux
- **Nordic, NXP, Microchip, TI, Silicon Labs integration boundaries**
- **FreeRTOS and Zephyr ports**

### Changed
- Hardware compatibility matrix expanded with board-specific HIL reports
- Release procedure: small, reviewable releases with changelog entries and generated provenance

## [0.3.0] — 2026-08-14

### Added
- Host demo with in-memory storage and transport
- `latch-dump` with `--hex` and `--json` flags
- ESP32 first crash tutorial (ESP-IDF)
- Architecture ports: Cortex-M, RV32, Xtensa
- Breadcrumbs, metrics, and log capture
- CRC-protected persistent spool
- XChaCha20-Poly1305 envelope encryption
- HKDF-SHA-256 domain separation
- Replay windows and authenticated at-rest storage
- Hardware-backed key contracts

## [0.2.0] — 2026-07-29

### Added
- Public-preview baseline
- Portable runtime split: core, capture, envelope, spool, storage, transport, metrics, security
- Host tests, compatibility vectors, simulated storage, property tests
- libFuzzer harnesses and seed-corpus tooling

## [0.1.0] — 2026-07-15

### Added
- Initial project setup
- Basic retained snapshot capture
- LEP v1 envelope serialization
