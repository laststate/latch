# Arduino AVR cooperative capture

This is a deliberately small, normal-runtime example for an Arduino Mega 2560
(ATmega2560). It selects `LS_CONSTRAINED_PROFILE=1`, stores a single bounded
LEP event in RAM, validates it, and sends it through `Serial`.

It proves a packaging and compile/link path only. It does **not** install an
AVR fault handler, survive a reset, reserve EEPROM/Flash, or turn an 8-bit MCU
into a qualified crash recorder.

## PlatformIO

```sh
pio run -d examples/arduino-avr-cooperative
pio run -d examples/arduino-avr-cooperative -t upload
pio device monitor -b 115200
```

Expected output ends with `Latch AVR cooperative capture complete`.

## Arduino IDE / CLI

Install this checkout as a library, select **Arduino Mega or Mega 2560**, and
compile the equivalent sketch with `LS_CONSTRAINED_PROFILE=1` applied to every
Latch C source. The PlatformIO example is the reproducible way to do that. The
automatic AVR profile also selects this baseline when `__AVR__` is defined.

The 512-byte envelope and 1 KiB RAM storage region are intentional trade-offs,
not a promise that the profile fits every AVR. Measure the fully linked
firmware and its stack before use; an Uno-class 2 KiB-RAM board is not claimed
as supported.
