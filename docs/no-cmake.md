# Latch without CMake (vendoring + direct compile)

Latch is portable C11 with no mandatory build system. CMake is the tested
path (`cmake --preset host-debug`), but you can vendor the sources and
compile with anything: gcc, clang, MSVC, ESP-IDF, PlatformIO.

## 1. Vendor (copy what you need)

Smallest useful set (host example proves the API surface):

```sh
mkdir -p vendor/latch
cp -r include/ vendor/latch/include
cp -r src/envelope/ src/storage/ src/transport/ src/capture/ vendor/latch/src
# KEEP: include/laststate/config.h — edit the LS_* knobs for your target
```

Verify against the repo: `python tools/minify_sources.py --output /tmp/latch-production --verify`.

## 2. Compile (no CMake)

```sh
# gcc / clang — freestanding-friendly C11, warnings as errors
cc -std=c11 -Wall -Wextra -Werror \
   -I vendor/latch/include \
   $(find vendor/latch/src -name '*.c') \
   -c  # then link into your firmware

# MSVC (host tools / latch-dump)
cl /std:clatest /W4 /I vendor\latch\include vendor\latch\src\*.c
```

Link only what you use: storage + transport are swappable — provide your
own `ls_storage_*` / `ls_transport_*` backends (flash, RTC RAM, UART) and
skip the host in-memory ones.

## 3. ESP-IDF / PlatformIO (no CMakeLists edits on your side)

- ESP-IDF: add `vendor/latch/src/*.c` + `vendor/latch/include` to your
  component `CMakeLists` `SRCS`/`INCLUDE_DIRS` (or list files explicitly);
  set `LS_*` options in a copied `config.h`. Full panic→reboot→ACK walkthrough:
  `examples/esp32-first-crash/README.md`, HIL fixture: `hil/esp32_relay/README.md`.
- PlatformIO: same files under `lib/latch/`; add `build_flags = -I lib/latch/include`.

## 4. Validate the vendored copy

```sh
# vectors (no build system needed beyond a C compiler + latch-dump):
latch-dump --hex tests/vectors/lep-v1-basic.hex
latch-dump --hex tests/vectors/lep-v2-basic.hex
```

Keep vectors + `latch-dump` in CI: they catch envelope regressions before
flashing. For crypto posture (built-in vs commercial provider), see
`docs/security/crypto-assurance.md`.
