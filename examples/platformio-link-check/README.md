# PlatformIO registry link check

This small Arduino/ESP32 build verifies that PlatformIO's Library Dependency
Finder reads the root `library.json`, compiles the portable C sources selected
by `build.srcFilter`, exposes the public C++ wrapper (which includes the C
runtime under `extern "C"`), and links the runtime.

```sh
pio run -d examples/platformio-link-check
```

It is a packaging check, not the crash/reboot tutorial. Use
[`esp32-first-crash`](../esp32-first-crash/README.md) for the complete ESP-IDF
flow.
