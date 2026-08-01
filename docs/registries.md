# Registry publication

Registry discovery is prepared but publication is a release operation. Keep
`CMakeLists.txt`, `include/laststate/version.h`, `library.json`,
`library.properties`, `idf_component.yml`, `rust/latch/Cargo.toml`,
`CHANGELOG.md` and the Git tag on exactly the same version before publishing.
`python tools/check_release_metadata.py --version X.Y.Z` verifies that parity
without publishing anything.

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

## Arduino Library Manager

`library.properties` and the `src/laststate` public-header shims make the
repository installable as an Arduino library. The package lists `esp32` and
`avr`: both are cooperative normal-runtime integrations, not generic 8/16-bit
automatic-crash ports. AVR automatically selects `LS_CONSTRAINED_PROFILE`.

Before submitting a release to the Arduino Library Manager, run the repository
metadata test and compile both the
[`arduino-esp32-cooperative`](../examples/arduino-esp32-cooperative/README.md)
and [`arduino-avr-cooperative`](../examples/arduino-avr-cooperative/README.md)
sketches against current board packages. The submission
itself is reviewed through the Arduino Library Manager registry process; use
the [Arduino library specification](https://docs.arduino.cc/arduino-cli/library-specification)
as the source of truth for metadata and layout changes.

Do not represent the Arduino package as crash-persistent. The examples use
volatile RAM and prove a bounded explicit capture path only.

## ESP-IDF Component Registry

The root `idf_component.yml` makes the repository consumable as an ESP-IDF
component. Its component CMake path includes the portable runtime and
`ports/esp-idf/esp_idf.c`; it deliberately excludes the version-sensitive
private panic-wrapper and Xtensa adapter. The local consumer is under
[`examples/esp-idf-component`](../examples/esp-idf-component/README.md).

Validate it with an ESP-IDF 5.x installation before publishing:

```sh
cd examples/esp-idf-component
idf.py set-target esp32
idf.py build
```

Publish only after the release tag is public and the component archive has been
checked against the [ESP-IDF Component Manager packaging guide](https://docs.espressif.com/projects/idf-component-manager/en/latest/guides/packaging_components.html).
The manifest format and its version constraints are documented in the
[Component Manager manifest reference](https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/manifest_file.html).

## Zephyr module

The root `zephyr/module.yml`, `zephyr/CMakeLists.txt` and `zephyr/Kconfig` expose
Latch to Zephyr's module discovery. The native-simulator first-capture sample
is built and run in CI; add a checkout with `ZEPHYR_EXTRA_MODULES` as shown in
[`examples/zephyr-first-capture`](../examples/zephyr-first-capture/README.md).
