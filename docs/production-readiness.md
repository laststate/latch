# Production readiness

## Verified locally

- Cortex-M fault snapshots use dedicated emergency stacks, validate MSP/PSP bounds, decode FPU frames, detect recursion and recover retained snapshots into the spool on the next boot.
- The spool uses committed records, CRC validation, retry accounting and durable-ACK transport semantics. Flash mirror and wear-level adapters use transactional commits and honor write geometry.
- LEP v1 has canonical encoding, version checks, golden C/Rust vectors, truncation flags, bounded parsing and replay-window support.
- XChaCha20-Poly1305, HKDF key derivation, key IDs, replay protection, redaction, SBOM/provenance automation, sanitizers, fuzzing, property tests and host compatibility builds are present.
- Transport selection, retries, fragmentation, reassembly and backpressure return explicit result codes. Thread, ISR and fault-context contracts are documented in [concurrency.md](concurrency.md).
- A physical ESP32-D0WD-V3/ESP-IDF 5.5.0 run verified mirrored flash, intentional panic/reset classification, reboot recovery, UART stream framing and durable acknowledgement against the production LastState Relay. The private Relay collector is not a build dependency; retained evidence and the public collector contract are in the [ESP32 HIL fixture](../hil/esp32_relay/README.md).
- A host-tested v3 retained snapshot preserves Xtensa A0-A15 and special fault registers without moving the v1/v2 prefix. The copyable ESP32 sample includes an ESP-IDF 5.5 linker-wrap panic adapter and is compile-checked separately from the earlier physical evidence.
- RV64 retained context uses a separate CRC-protected wide-context sidecar and an additive LEP CPU64 TLV, while keeping v1/v2/v3 snapshot readers compatible. Its trap boundary is host-tested and cross-compiled, not hardware-qualified.
- Nordic, NXP, Microchip, TI and Silicon Labs reset-reason adapters are host-tested without a vendor SDK. They normalize the supported documented raw masks or status fields; clear semantics vary by controller and are not assumed for every profile. They do not substitute for each board's vector, Flash and reset-register HIL.
- The Linux fatal-signal handoff is exercised on the native x86_64 host with a fork/pipe/SIGABRT test: a configured alternate-stack handler writes a CRC-protected fixed-size native raw record in one nonblocking write. Its AArch64 field extraction remains an integration boundary that needs validation against the selected Linux kernel/libc ABI. It intentionally does not emit LEP, call Latch runtime code or establish on-disk durability from signal context.
- Native Arduino and ESP-IDF Component Manager package layouts, PlatformIO packaging, Zephyr module discovery and Rust crate packaging have deterministic metadata/layout validation. Publication to external registries remains manual and unperformed until a release is approved.
- The footprint reporter distinguishes static archives from linked images and records compiler frame reports without treating either as a complete physical-board or call-stack measurement.
- The public hardware matrix is generated from validated, versioned evidence
  metadata. QEMU executes Cortex-M exception paths and Renode executes an RV32
  illegal-instruction trap; both remain explicitly below physical HIL.
- Deterministic spool stress sweeps every byte of a committed envelope, 160
  interrupted-write/partial-write combinations, and recovery past a corrupt
  record. The hosted OTA, critical-redaction, and low-power reference designs
  are compiled and executed in CI.

## Required before a product release

- Run the HIL scenarios on every supported board, linker script and toolchain: HardFault, MSP, PSP, corrupt PSP, stack overflow, nested faults, watchdog, brownout, Flash and reset registers.
- Re-run physical HIL with the ESP-IDF 5.5 Xtensa panic wrapper and decode the promoted register event before treating automatic panic-frame capture as qualified. Revalidate or replace the wrapper whenever ESP-IDF's private panic ABI changes.
- Qualify vendor integrations for TLS, BLE, LoRaWAN, CAN, secure elements, TrustZone placement and RISC-V trap ownership.
- Obtain an independent cryptographic, side-channel and provisioning review. Repository tests are not an audit or certification.
- If the product requires MISRA C:2012 or another safety standard, define the target configuration/toolchain, run the appropriate analyzer, own approved deviations and retain the results with the release. The repository does not claim compliance.
- Protect `prod`, require the stable validation check, build and test the separate compact source artifact, publish the release artifacts, and retain the HIL evidence with the release record.

No release gate is considered complete merely because host tests pass.

The criteria for a future `1.0` LTS line, including three physically qualified
configurations, a 90-day critical-regression-free window, and an independent
security review, are defined in [the LTS policy](lts-policy.md).
