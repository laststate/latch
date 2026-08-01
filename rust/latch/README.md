# laststate-latch

`#![no_std]` Rust helpers and C ABI declarations for
[LastState Latch](https://github.com/laststate/latch), a heap-free embedded
crash-evidence runtime.

The safe Rust surface validates and iterates LEP v1 envelopes without
allocation. The `extern "C"` capture calls link to the Latch C runtime, which
must be included in the final firmware separately (for example through CMake,
PlatformIO, or a Zephyr module).

```rust
use laststate_latch::Envelope;

let envelope = Envelope::parse(bytes_from_device)?;
for field in envelope.tlvs() {
    let field = field?;
    // Unknown TLV types remain safely skippable.
}
# Ok::<(), laststate_latch::DecodeError>(())
```

This crate performs no allocation and enables no default features. It is
versioned below 1.0 while the higher-level Rust capture API is still evolving;
the LEP v1 wire format is explicitly versioned and compatibility-tested.

License: Apache-2.0.
