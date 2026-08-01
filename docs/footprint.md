# Footprint and small-device profiles

Latch has no universal RAM or Flash minimum. The linked footprint depends on the CPU ABI, compiler, linker script, linker garbage collection, selected features, storage backend, transport, crypto choice and the application's own buffers. A static library total is useful for comparing source profiles, but it is not a final firmware measurement.

## Published source-profile baselines

The following reproducible source-archive aggregates were measured on
2026-08-01 with `arm-none-eabi-gcc 13.2.1`, Cortex-M4 (`-mcpu=cortex-m4
-mthumb`), `MinSizeRel`, and the portable `liblatch.a` archive. They are
published to make the default and constrained profiles concrete; they are
**not** a minimum-device claim or a linked firmware size.

| Portable profile | `.text` | `.data` | `.bss` | Flash estimate | Static RAM estimate |
| --- | ---: | ---: | ---: | ---: | ---: |
| Default configuration | 27,457 B | 0 B | 7,524 B | 27,457 B | 7,524 B |
| Constrained starting profile | 24,386 B | 0 B | 792 B | 24,386 B | 792 B |

An additional install-path check was run on 2026-08-01 by packing the
PlatformIO library and linking `examples/arduino-avr-cooperative` for an
Arduino Mega 2560 with PlatformIO Atmel AVR 5.1.0 / AVR GCC 7.3.0. The linked
image used 34,554 B of Flash (13.6% of 253,952 B) and 2,416 B of static RAM
(29.5% of 8,192 B). This is a compile/link and footprint measurement, not a
hardware qualification or a minimum-size promise.

The constrained measurement used the feature/capacity values shown below,
including one power/health/span/transport/dump/redaction/spool/policy/dedup
slot each.
It has no stored strings, breadcrumbs, metrics, logs, power/performance
samples, dumps, assertions or stack snapshot. It still includes the portable
cryptographic implementation and the 768-byte emergency-stack baseline; a
real application adds its retained storage/spool region, transport buffers,
linker/startup code and selected vendor libraries.

## What to measure

Measure the linked firmware for every board and build mode that will ship. At minimum record:

- `.text + .data` as a Flash estimate;
- `.data + .bss` as static RAM;
- retained-memory and spool allocations supplied by the application;
- maximum stack frame records from `-fstack-usage`, followed by a real call-depth/interrupt-nesting analysis;
- the selected crypto, storage and transport dependencies.

The repository includes a reproducible, target-agnostic reporter:

```sh
python tools/measure_footprint.py path/to/firmware.elf \
  --tool arm-none-eabi-size \
  --stack-usage-dir build/firmware \
  --label my-board-release \
  --format markdown
```

Use `--format json` for CI artifacts. It detects whether its input is a linked ELF image or a static archive and labels the result accordingly. It will not pretend that an archive aggregate or individual compiler frame proves final Flash, RAM or total stack use.

For the existing portable-library budget gate:

```sh
python tools/check_size.py build/arm-cortex-m4/liblatch.a \
  --tool arm-none-eabi-size --flash-budget 131072 --ram-budget 65536
```

## Start with a profile, not defaults

The defaults prioritize useful post-reset evidence on typical 32-bit MCUs. They are not a recommendation for every Arduino-class board. For constrained devices, begin with `LS_CONSTRAINED_PROFILE=1` and then opt in only to what the firmware can afford. On AVR the profile is automatic; on other toolchains the definition must be passed to every Latch C source. Its baseline disables optional observability/string storage, chooses one spool and transport slot, and caps envelopes at 512 bytes. Equivalent explicit settings are:

```cmake
target_compile_definitions(firmware PRIVATE
  LS_STORE_STRINGS=0
  LS_ENABLE_BREADCRUMBS=0
  LS_ENABLE_METRICS=0
  LS_ENABLE_LOGS=0
  LS_ENABLE_POWER_SAMPLES=0
  LS_ENABLE_PERFORMANCE=0
  LS_ENABLE_DUMPS=0
  LS_ENABLE_ASSERTS=0
  LS_ENABLE_STACK_SNAPSHOT=0
  LS_MAX_EVENT_SIZE=512
  LS_BREADCRUMB_CAPACITY=1
  LS_METRIC_CAPACITY=1
  LS_POWER_SAMPLE_CAPACITY=1
  LS_HEALTH_CAPACITY=1
  LS_SPAN_CAPACITY=1
  LS_MAX_TRANSPORTS=1
  LS_MAX_DUMP_REGIONS=1
  LS_MAX_REDACTIONS=1
  LS_SPOOL_MAX_RECORDS=1
  LS_POLICY_CAPACITY=1
  LS_DEDUP_CAPACITY=1)
```

This is a starting point, not a memory guarantee. The application still needs storage for the retained snapshot, the persistent spool, transport framing and any enabled authenticated-encryption buffers. On 8/16-bit MCUs, first use the portable/cooperative API with a measured build; do not assume automatic fault capture or the default cryptographic profile will fit.

RV64 full register recovery is an explicit additional retained-RAM choice:
set `LS_ENABLE_WIDE_CONTEXT=1` only after reserving and measuring its
CRC-protected sidecar. Leaving it disabled retains the bounded legacy summary
and marks the CPU64 extension unavailable rather than silently truncating
64-bit state.

## Reproducible evidence

Attach the generated JSON/Markdown report to a board qualification or release record together with the commit, compiler version, link map, CMake/cache configuration and linker script. A changed compiler, optimization mode or enabled feature requires a new report. This makes footprint claims reviewable instead of turning a single benchmark into a compatibility promise.
