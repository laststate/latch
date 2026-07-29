# Production readiness

## Verified locally

- Fault snapshots use dedicated emergency stacks, validate Cortex-M MSP/PSP bounds, decode FPU frames, detect recursion and recover retained snapshots into the spool on the next boot.
- The spool uses committed records, CRC validation, retry accounting and durable-ACK transport semantics. Flash mirror and wear-level adapters use transactional commits and honor write geometry.
- LEP v1 has canonical encoding, version checks, golden C/Rust vectors, truncation flags, bounded parsing and replay-window support.
- XChaCha20-Poly1305, HKDF key derivation, key IDs, replay protection, redaction, SBOM/provenance automation, sanitizers, fuzzing, property tests and host compatibility builds are present.
- Transport selection, retries, fragmentation, reassembly and backpressure return explicit result codes. Thread, ISR and fault-context contracts are documented in [concurrency.md](concurrency.md).

## Required before a product release

- Run the HIL scenarios on every supported board, linker script and toolchain: HardFault, MSP, PSP, corrupt PSP, stack overflow, nested faults, watchdog, brownout, Flash and reset registers.
- Qualify vendor integrations for TLS, BLE, LoRaWAN, CAN, secure elements, TrustZone placement and RISC-V trap ownership.
- Obtain an independent cryptographic, side-channel and provisioning review. Repository tests are not an audit or certification.
- Protect `prod`, require the stable validation check, build and test the separate compact source artifact, publish the release artifacts, and retain the HIL evidence with the release record.

No release gate is considered complete merely because host tests pass.
