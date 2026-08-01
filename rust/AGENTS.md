# Rust binding guidance

Apply the root [AGENT.md](../AGENT.md) first.

- `rust/latch` is `#![no_std]` and binds to the C runtime. Avoid allocation and
  platform assumptions unless a feature explicitly supports them.
- Preserve C/Rust FFI type layout, integer widths, ownership, and lifetime
  contracts. Change C and Rust bindings together where necessary.
- Run format, Clippy with warnings denied, and tests for Rust changes. Keep the
  crate's public documentation aligned with the C API.
- Do not expose cryptographic keys, raw captured memory, or unsafe behavior as
  a convenience wrapper without an explicit design and security review.
