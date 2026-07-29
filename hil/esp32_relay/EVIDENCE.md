# Physical ESP32 + Relay qualification evidence

Date: 2026-07-29
Result: **PASS**

## Hardware and toolchain

- Device: ESP32-D0WD-V3 revision 3.1 on `COM3`
- Flash: 4 MB BOYA-compatible SPI flash, 3.3 V
- Crystal: 40 MHz
- Framework: ESP-IDF 5.5.0
- Platform: PlatformIO Espressif32 6.12.0
- Compiler: Xtensa GCC 14.2.0
- Collector: production LastState Relay, built with Go 1.26.1 after its full
  test suite passed

The final image used 20,144 bytes of RAM and 223,105 bytes of the 1.5 MB
application partition.

## Scenario

1. The complete 4 MB flash was erased.
2. The fixture booted and initialized Latch on a dedicated 128 KB partition.
3. Latch recorded breadcrumbs, metrics, a power sample, health state, and an
   error envelope in the atomic mirrored-flash backend.
4. The fixture called `abort()`. ESP-IDF ran its panic path and rebooted.
5. A new Latch boot recovered the queued envelope from flash.
6. The production Relay opened the serial port with `latch-stream` framing and `lsak-v1`
   durable acknowledgements.
7. Latch delivered every recovered envelope, then captured and delivered a
   post-recovery confirmation.
8. Relay was stopped and its spool was checked with `status` and `doctor`.

Opening this board's FTDI serial port caused an additional external reset.
That produced a second pre-panic event and exercised recovery of multiple
queued records rather than only one.

## Production Relay result

LastState Relay validates and durably persists LEP envelopes before
acknowledging them and provides spool consistency checks. It is a private
LastState component and was not publicly available on the qualification date.
No Relay source, repository URL, or private implementation detail is required
to build Latch or understand this evidence.

```json
{
  "events": 3,
  "pending": 0,
  "delivering": 0,
  "dead_letter": 0,
  "quarantined": 0,
  "spool_bytes": 1648
}
```

Relay doctor:

```json
{
  "healthy": true,
  "removed_pending_files": 0,
  "missing_objects": null,
  "corrupt_objects": null,
  "orphan_objects": null
}
```

## Decoded LEP evidence

| Sequence | Event ID | Scenario | Latch reset | Raw reset | Boot | Bytes |
| ---: | ---: | --- | ---: | ---: | ---: | ---: |
| 1 | 1246770978 | pre-panic | power-on | 1 | 1 | 641 |
| 2 | 3329628597 | pre-panic after FTDI reset | power-on | 1 | 3 | 641 |
| 3 | 1101461325 | post-recovery confirmation | lockup | 4 | 4 | 366 |

The two pre-panic envelopes contain message hash `464ccdc4`, which matches
FNV-1a(`esp32-before-intentional-panic`). The post-recovery envelope contains
`50a2d9fd`, which matches
FNV-1a(`esp32-post-recovery-confirmation`).

The post-recovery event can only be captured after the recovered queue's
`ls_flush()` returns success. On this transport, success requires Relay's
durable `LSAK_STORED` or `LSAK_DUPLICATE` response. Its presence in Relay
therefore proves the recovery ACK path, not merely serial transmission.

## Scope and remaining gap

This run proves the portable Latch runtime, the ESP-IDF reset-reason adapter,
mirrored flash persistence, reboot recovery, LEP encoding, stream framing,
Relay ingestion, durable acknowledgement, and spool consistency on physical
ESP32 hardware.

It does **not** prove automatic register/stack capture from the Xtensa panic
handler. The current ESP-IDF port maps reset reasons but does not install a
panic-frame hook. The pre-panic envelope in this fixture is deliberately
captured from normal runtime immediately before `abort()`.
