# Contributing

Build and run the portable suite before proposing a change:

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
cargo fmt --manifest-path rust/latch/Cargo.toml -- --check
cargo clippy --manifest-path rust/latch/Cargo.toml --all-targets -- -D warnings
cargo test --manifest-path rust/latch/Cargo.toml
python tools/check_docs.py
python tools/check_test_vector.py
```

Changes to LEP must preserve old test vectors or intentionally increment the protocol version. Fault-path code must remain heap-free and must not introduce a hosted C library dependency. New platform code should include a simulator test when possible and a hardware qualification note when it depends on real registers, exception entry, cache, MPU, TrustZone or Flash timing.

Do not include device secrets, production keys or captured customer memory in fixtures, fuzz corpora or issue reports.
