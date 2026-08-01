# CMake FetchContent consumer

This example downloads the immutable `v0.2.0` release and exposes the same
`laststate::latch` target as an installed or vendored build:

```sh
cmake -S . -B build
cmake --build build
./build/firmware
```

Pin a release tag or commit in production; do not track a moving branch.
Maintainers can test the current checkout without network access using
`-DFETCHCONTENT_SOURCE_DIR_LATCH=/absolute/path/to/latch`.
