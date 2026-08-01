# ESP32 first crash — from zero to durable ACK

This is the shortest complete Latch experiment on a real ESP32. You do not
need to know Latch's architecture, run Relay, or write a collector first. The
example owns a dedicated `latch` flash partition and uses a tiny on-device
loopback collector so the entire crash/recovery contract is visible in one
terminal.

## Prerequisites

- an ESP32 connected by USB;
- ESP-IDF 5.5 with its environment activated;
- a clone of this repository.

From this directory, run exactly:

```sh
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

The board prints:

```text
1. Event recorded in the flash-backed Latch spool
2. Intentional panic now (abort)
... ESP-IDF panic and reboot ...
3. Reboot detected: reset_reason=...
4. Recovering the queued event and sending its LS frame
5. ACK stored in NVS: event=... status=stored
PASS: crash -> reboot -> recovery -> durable ACK
```

What each step proves:

1. public breadcrumb, metric and capture calls encoded an event into the
   transactional flash-backed spool;
2. `abort()` caused a real ESP-IDF panic and reboot, not a simulated process
   restart;
3. the reset reason reached Latch after boot; the example's ESP-IDF 5.5 panic
   adapter also promoted the retained Xtensa registers into a crash event;
4. Latch recovered the pre-panic and retained-fault events and emitted
   CRC-checked `LS` frames;
5. the tutorial collector committed the event ID to NVS before returning the
   durable ACK that lets Latch remove the spool record.

The loopback collector is intentionally local so a first-time user needs only
`idf.py`. For a real serial boundary, use
[`tools/latch_collector.py`](../../tools/latch_collector.py) or a production
collector implementing the same documented `LS`/`LSAK` contract. The private
LastState Relay is not required.

The automatic Xtensa register capture is intentionally pinned to ESP-IDF 5.5.
ESP-IDF currently has no stable public application panic-hook ABI, so this
sample wraps `esp_panic_handler` and immediately delegates to the vendor
handler after a bounded retained-memory write. Revalidate the adapter when
upgrading ESP-IDF; the pre-panic flash event remains independent of that
private ABI.

All Latch storage, encoding, collector and flush callbacks in this sample run
from normal FreeRTOS task context. Do not call them from an ISR. The wrapped
panic path calls only the explicitly fault-safe retained snapshot writer; it
does not allocate, enter the scheduler, touch flash/NVS, or send a transport.
The normal boot converts that snapshot into the durable spool.

Run `idf.py erase-flash` before repeating the tutorial. The sample is evidence
for the ESP32/ESP-IDF combination, not qualification for every ESP32 variant,
flash chip, power-loss point, or production security policy.
