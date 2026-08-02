# Production reference designs

These are complete, host-executed integration designs for three common product
shapes. They exercise real Latch APIs and are built and run by the normal test
suite. Hardware-specific comments identify the callbacks that must be replaced
before deployment.

| Design | Demonstrates | Product-owned boundary |
| --- | --- | --- |
| [`iot-ota`](iot-ota/main.c) | Pending release, expected reset, rollback evidence and durable ACK | Bootloader slot state, signature verification, atomic flash and network collector |
| [`critical-redaction`](critical-redaction/main.c) | Selective dump, exclusion redaction, XChaCha20-Poly1305 policy and secure durable transport | Hardware CSPRNG, per-device key provisioning, audited policy and safety case |
| [`low-power-deep-sleep`](low-power-deep-sleep/main.c) | Offline capture, persistent spool, reboot/deep-sleep boundary and later low-power delivery | Actual retention/flash, sleep synchronization, brownout behavior and radio driver |

Build and execute all three from the repository root:

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug -R latch-reference
```

The storage and entropy implementations in these hosted fixtures are
deliberately deterministic so CI can reproduce them. They are placeholders,
not production backends. Before adopting a design, replace them and run the
exact board/toolchain scenarios in the
[hardware compatibility matrix](../../docs/hardware-compatibility.md).
