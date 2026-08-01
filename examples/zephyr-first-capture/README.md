# Zephyr first capture

This sample starts from a normal Zephyr application and needs no hardware. Its
west manifest pins Zephyr v4.4.0; CI uses the host compiler and
`native_sim/native/64`. Set up that workspace, then from the Latch repository
run:

```sh
west build -p always -b native_sim/native/64 examples/zephyr-first-capture -- \
  -DZEPHYR_EXTRA_MODULES=$PWD
west build -t run
```

It registers bounded in-memory storage, captures a breadcrumb, metric and
message, validates the resulting LEP envelope in its transport, and prints
`Latch: PASS first capture`. Real products should replace both example
backends and qualify retained fault capture on their board.

No devicetree overlay or custom linker script is used. The sample's storage is
ordinary process memory, so it does not qualify retained RAM, flash, reboot, or
fault entry. Zephyr callbacks and Latch capture/flush run in normal thread
context; ISR/fault code must defer everything except a selected port's
explicitly fault-safe retained writer.
