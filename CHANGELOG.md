# Changelog

All notable changes to Latch are recorded here. The project follows [Semantic Versioning](https://semver.org/) while it is pre-1.0, and the format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

The first public tag will establish the `v0.2.0` baseline. Until that tag exists, the public-preview work remains under `Unreleased`.

## [Unreleased]

### Added

- Heap-free portable C11 runtime with bounded capture, breadcrumbs, metrics, logs, health, performance, dumps, and policy controls.
- Deterministic LEP v1 envelopes with public C and Rust validation, golden vectors, CRC, authentication, encryption, compression, and replay protection.
- Retained crash recovery, transactional spool records, memory/Flash storage adapters, interrupted-write simulation, retries, and acknowledged delivery.
- Cortex-M, RV32, Xtensa, Linux, STM32, RP, ESP-IDF, FreeRTOS, and Zephyr integration boundaries.
- C++ wrapper, Rust `#![no_std]` SDK, host decoder, fuzz targets, hardware-in-the-loop entry points, packages, SBOM, and provenance automation.
- Runnable host demo that captures an envelope and validates it with `latch-dump`.
- Public roadmap, contribution guide, issue forms, support policy, and community code of conduct.

### Changed

- Reworked the README around the post-reset evidence problem, a three-call example, and a 60-second demo.
- Kept reviewable source on the default branch and moved source compaction to a verified distribution artifact.
- Aligned default-branch workflows with `prod` and enabled security-result publication.

### Fixed

- Instrumented the portable runtime itself during libFuzzer builds instead of instrumenting only the thin fuzz harnesses, and seeded all three fuzz targets deterministically.
- Made the previously unbuilt host example use a valid build ID and enough retained storage for the configured spool.
- Removed signed-overflow undefined behavior from delta decoding when valid values cross the signed 32-bit boundary.

### Security

- Documented provisioning, redaction, authenticated storage, transport, and independent-review requirements.
- Added repository settings guidance for private vulnerability reporting, secret scanning, and push protection.
