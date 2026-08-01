# Port integration guidance

Apply the root [AGENT.md](../AGENT.md) and read
[docs/ports.md](../docs/ports.md) before modifying an adapter.

- Ports adapt external SDKs and hardware; do not leak SDK assumptions into the
  portable runtime.
- Name every external SDK version, linker, reset, TLS, storage-geometry, ISR,
  and concurrency assumption in code or documentation.
- Do not silently downgrade TLS verification, entropy quality, secure-element
  guarantees, flash durability, or reset behavior for compatibility.
- Prefer a host simulator/unit test plus a narrowly scoped build check. Mark
  physical evidence precisely and never infer it from compilation.
- ESP-IDF panic integration depends on private ABI boundaries. Read the ESP32
  example and HIL fixture before changing it, and do not call it qualified
  without revalidation on the target SDK.
