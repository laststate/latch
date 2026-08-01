# Registry publication

Registry discovery is prepared but publication is a release operation. Keep
`CMakeLists.txt`, `library.json`, `rust/latch/Cargo.toml`, `CHANGELOG.md` and the
Git tag on exactly the same version before publishing.

## PlatformIO Registry

Validate the exported file set locally:

```sh
pio pkg pack . --output build/laststate-latch-platformio.tar.gz
pio run -d examples/platformio-link-check
```

After `pio account login`, publish only from the clean release tag:

```sh
git status --short
git describe --exact-match --tags
pio pkg publish .
```

The manifest indexes the name, embedded crash-reporting description, keywords,
frameworks, platforms and public header. PlatformIO does not allow a published
name/version pair to be reused even after unpublishing it, so CI packages the
candidate but never publishes automatically.

## crates.io

The `laststate-latch` crate is `#![no_std]`, allocator-free and has no default
features. Its safe LEP decoder works directly in Rust; capture FFI symbols need
the C runtime linked into the final firmware.

```sh
cargo fmt --manifest-path rust/latch/Cargo.toml -- --check
cargo clippy --manifest-path rust/latch/Cargo.toml --all-targets -- -D warnings
cargo test --manifest-path rust/latch/Cargo.toml
cargo publish --manifest-path rust/latch/Cargo.toml --dry-run
cargo login
cargo publish --manifest-path rust/latch/Cargo.toml
```

Crate versions are also immutable. A failed publication should be corrected in
a new patch version rather than attempting to replace uploaded source.

## Zephyr module

The root `zephyr/module.yml`, `zephyr/CMakeLists.txt` and `zephyr/Kconfig` expose
Latch to Zephyr's module discovery. The native-simulator first-capture sample
is built and run in CI; add a checkout with `ZEPHYR_EXTRA_MODULES` as shown in
[`examples/zephyr-first-capture`](../examples/zephyr-first-capture/README.md).
