# Latch

Latch is Last State's heap-free C11 runtime for capturing, preserving and delivering the last known state of an embedded device.

The current implementation includes:

- deterministic LEP v1 envelopes, length-checked TLVs, public validation/visitor APIs and CRC32;
- persistent boot counters, previous uptime, pending-crash association, expected reset tracking, boot-loop detection and OTA state;
- fixed breadcrumb, metric-window, power-sample, health, watchdog and performance buffers;
- binary logs, runtime/compile-time filters, structured breadcrumb values and optional production builds without stored strings;
- assertions with continue/reset/halt/breakpoint/callback policies;
- authorized selective dumps, bounded stack snapshots, full authorized coredumps and zero/hash/exclude redaction;
- atomic spool records, priority-aware retention, recovery, retry counters, ACK-aware delivery and transport fragmentation;
- Cortex-M fault preservation with dedicated handlers, emergency stack, M0/M0+/M3/M4/M7/M23/M33-safe assembly paths, FPU frame decoding and TrustZone fault status;
- RV32 trap entry, Xtensa frame adapter, Linux signal capture and STM32/RP/ESP-IDF reset ports;
- generic acknowledged stream framing for UART, USB CDC and RS-485;
- XChaCha20-Poly1305 authenticated envelope encryption, HKDF-SHA-256 domain separation, CSPRNG enforcement, replay windows and legacy HMAC verification;
- native mbedTLS and Zephyr verified-TLS socket transports, encrypted BLE GATT, fragmented CAN/CAN-FD and LoRaWAN transports, including cellular modem socket offload;
- dual-bank mirroring, multi-slot Flash wear leveling, authenticated at-rest storage and interrupted-write simulation;
- secure-element contracts plus a CryptoAuthLib adapter for hardware RNG, P-256 signing, key derivation and X.509 certificate reconstruction;
- C++ RAII wrapper and a Rust `#![no_std]` SDK;
- separate CMake libraries for core, capture, envelope, spool, storage, transport and metrics.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
cargo check --manifest-path rust/latch/Cargo.toml
```

On Windows with Visual Studio 2022, use the checked-in `host-msvc` preset:

```sh
cmake --preset host-msvc
cmake --build --preset host-msvc
ctest --preset host-msvc
```

CMake generates a 128-bit printable Build ID from the project version, Git revision and source hashes. Applications may override it with `LS_BUILD_ID` or `identity.firmware_build_id`; invalid IDs are rejected by `ls_init`.

## Compact production source

Keep the checkout formatted for maintenance. To make a separate compact distribution, run:

```sh
python tools/minify_sources.py --output ../latch-production --verify
```

The script copies the project, removes comments and unnecessary lexical whitespace from C, C++ and Rust source files, preserves the original checkout, and writes `minify-manifest.json` with the result. It does not replace compiler optimization, link-time optimization, or binary stripping when producing firmware.

## Libraries

Use `laststate::latch` for the complete portable runtime, or link the modular archives explicitly:

- `latch-core`
- `latch-capture`
- `latch-envelope`
- `latch-spool`
- `latch-storage-memory`
- `latch-transport`
- `latch-metrics`
- `latch-security`

Architecture and platform libraries include `latch-cortex-m`, `latch-riscv`, `latch-xtensa`, `latch-linux`, `latch-port-stm32`, `latch-port-rp`, `latch-port-esp-idf`, `latch-port-freertos` and `latch-port-zephyr`.

Optional integrations are enabled with `LS_BUILD_MBEDTLS_PORT=ON` and `LS_BUILD_CRYPTOAUTHLIB_PORT=ON`.

## Storage

Call `ls_storage_required_size()` before allocating retained storage. `ls_memory_storage_*` adapts a byte buffer; place it in `.noinit` with `LS_NOINIT` for reset persistence. `ls_flash_wear_init()` distributes complete transactional images over 2–32 erase slots. `ls_secure_storage_init()` can wrap that logical backend with generation-keyed XChaCha20-Poly1305 encryption. The provided workspace must be retained only while the backend is active and is wiped after each operation.

## Security setup

Register a hardware CSPRNG with `ls_security_set_random_provider()`, then provision exactly 32 bytes with `ls_security_set_key()`. A configured key makes new envelopes encrypted and authenticated by default. See [SECURITY.md](SECURITY.md) for nonce, rotation and provisioning requirements and [docs/security-and-storage.md](docs/security-and-storage.md) for integration examples.

## Fault integration

For Cortex-M, call `ls_cortex_m_init()` during early boot and install the dedicated handler symbols in the vector table. For RV32, initialize `mscratch` with `ls_riscv_init()` and point `mtvec` at `ls_riscv_trap_handler`. Xtensa/ESP-IDF panic integration passes the vendor exception frame through `ls_xtensa_capture_frame()`.

Ports that touch CPU exception state still require validation on each MCU/toolchain combination before production deployment; host tests cannot prove interrupt-vector, MPU or vendor reset-register behavior.

Physical brownout, watchdog, MPU, TrustZone, lazy-FPU, Flash and reset-register scenarios are driven by the self-hosted HIL workflow described in [docs/hil.md](docs/hil.md). Native transport and secure-element integration is described in [docs/native-integrations.md](docs/native-integrations.md).

Licensed under Apache-2.0.

Repository validation, maintenance bots and the GitHub settings required for automated fix commits are documented in [docs/automation.md](docs/automation.md).
