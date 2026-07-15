# Implementation status

The portable runtime, modular build, LEP encoding/validation, persistent spool, boot state, filters, diagnostics, dumps/redaction, C++ wrapper and Rust no-std bindings are implemented and host-tested. Authentication, replay protection, compression codecs, public vectors, an atomic two-bank Flash adapter, fault injection, property tests, bounded decoder fuzzing and transport adapters are included.

Architecture sources are implementation-complete at their generic boundary, but real-hardware qualification remains required. In particular, Cortex-M vector/linker integration, FPU lazy stacking under the selected compiler, RISC-V `mscratch` ownership, Xtensa vendor-frame translation, TrustZone secure/non-secure placement and manufacturer reset masks cannot be certified by host tests.

Production qualification still needs the selected product hardware, linker script, bootloader, Flash geometry and vendor networking stack. TLS, BLE GATT, LoRaWAN and CAN bus drivers intentionally remain callbacks owned by those platform stacks; Latch provides framing, fragmentation, prioritization, incident beacons and HTTPS/MQTT adapters around them. Long-running fuzz campaigns and hardware-in-the-loop matrices are configured as automation entry points but require GitHub runners or physical boards to execute.
