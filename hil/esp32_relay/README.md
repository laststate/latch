# ESP32 hardware-in-the-loop test

This fixture proves the Latch embedded path on a physical ESP32 and an
acknowledging collector:

1. initialize an atomic mirrored-flash backend;
2. capture telemetry and an error before an intentional panic;
3. reboot and recover the queued LEP envelope from flash;
4. send it over the Latch stream framing on UART;
5. wait for the collector's durable `LSAK` acknowledgement;
6. capture and deliver a second post-recovery event.

The success marker is:

```text
HIL:PASS:LATCH_RELAY_ESP32
```

This specifically validates the portable Latch runtime, ESP-IDF reset-reason
adapter, on-device flash persistence, stream framing, collector ingestion,
deduplication-compatible durable ACK, and post-reboot recovery. It does not yet
install an ESP32/Xtensa panic-frame hook; the panic is intentional and the
pre-panic event is captured from normal runtime immediately before it.

## Collector used for the qualification

This fixture was verified on 2026-07-29 against the production LastState Relay.
Relay accepts framed LEP envelopes, validates and persists them before replying
with `LSAK_STORED` or `LSAK_DUPLICATE`, and exposes spool health checks.

Relay is a private LastState component and, as of 2026-07-29, has not been
released publicly. It is not a dependency of the Latch library or build. A
different collector can reproduce the test by implementing the public
`latch-stream` and `LSAK v1` formats documented below.

## Start from zero

The commands below are for PowerShell on Windows. Install Python 3.11 or newer,
then create an isolated tool environment:

```powershell
py -3.11 -m venv "$env:LOCALAPPDATA\LastState\esp32-tools"
& "$env:LOCALAPPDATA\LastState\esp32-tools\Scripts\python.exe" `
  -m ensurepip --upgrade
& "$env:LOCALAPPDATA\LastState\esp32-tools\Scripts\python.exe" `
  -m pip install platformio esptool pyserial
```

Identify the board and verify its real flash geometry before using the
partition table:

```powershell
$python = "$env:LOCALAPPDATA\LastState\esp32-tools\Scripts\python.exe"
& $python -m esptool --port COM3 chip-id
& $python -m esptool --port COM3 flash-id
```

The qualified board reported ESP32-D0WD-V3 revision 3.1 and 4 MB of flash.
Change `monitor_port`, `upload_port`, and the flash/partition configuration
when using different hardware.

From this directory, build, erase the selected board, and upload:

> `erase` destroys the complete contents of the selected device. Confirm the
> port and keep production credentials or calibration data off the test board.

```powershell
& $python -m platformio run
& $python -m platformio run --target erase
& $python -m platformio run --target upload
```

Immediately start an acknowledging collector. The internal qualification used:

```powershell
<laststate-relay> collect --serial COM3 --baud 115200 `
  --framing latch-stream --ack lsak-v1 --data-dir .\.lab\esp32-relay
```

The firmware waits before delivery and retries, so Relay can acquire the port
after the uploader releases it. Some FTDI auto-reset circuits reset the ESP32
when the collector opens the port; the fixture handles this and may produce an
additional valid pre-panic envelope.

## Public collector contract

The device writes:

```text
"LS" | version=1 | flags=0 | envelope_length:u32le | LEP | crc32:u32le
```

After validating and durably storing the LEP envelope, the collector replies:

```text
"LSAK" | version=1 | status | reserved:u16le | event_id:u32le
```

`LSAK_STORED` and `LSAK_DUPLICATE` are successful durable outcomes. The
definitions and parser are in
[`stream_transport.h`](../../include/laststate/stream_transport.h).

## Expected evidence

- the console shows phase 0 capture and an intentional `abort()`;
- the next Latch boot reports reset reason `LS_RESET_LOCKUP`;
- the collector stores the recovered pre-panic event;
- the device captures a post-recovery confirmation only after the first
  durable ACK;
- the collector ends with no pending, corrupt, quarantined, or dead-letter
  objects;
- the device emits `HIL:PASS:LATCH_RELAY_ESP32`.

The first physical result and decoded event IDs are retained in
[`EVIDENCE.md`](EVIDENCE.md).

## Problems found by the physical run

| Problem | Effect | Resolution |
| --- | --- | --- |
| Generated ESP-IDF config selected 2 MB while the chip reported 4 MB | Image metadata and the partition plan disagreed with hardware | Pinned the fixture to 4 MB after verifying it with `esptool flash-id` |
| BOYA flash support was not linked | ESP-IDF fell back to its generic flash driver | Enabled `CONFIG_SPI_FLASH_SUPPORT_BOYA_CHIP` |
| `CONFIGURE_DEPENDS` was used while ESP-IDF evaluated component requirements in script mode | CMake configuration failed | Removed `CONFIGURE_DEPENDS` from the fixture source discovery |
| The first fixture relied on `RTC_DATA_ATTR` to select the recovery phase | The board repeatedly captured and panicked instead of reaching delivery | Select recovery from `ESP_RST_PANIC`/`ESP_RST_SW`, which is the reset evidence exposed by ESP-IDF |
| Opening the FTDI port caused an external reset | A second pre-panic envelope was queued | Made the flow tolerate the reset and verified recovery of multiple queued records |
| The AEAD size guard was constant-false on 32-bit `size_t` | Xtensa builds emitted a Latch warning | Compile the large-input guard only where `size_t` can represent such a length |
| Package mirrors returned partial tool archives during first-time setup | Toolchain installation stalled or lacked files | Re-ran the PlatformIO package installation with mirror fallback and verified package manifests before building |
| The isolated Python environment initially lacked `pip` | PlatformIO could not install esptool's Python dependencies | Bootstrapped it with `python -m ensurepip --upgrade` |

The remaining product limitation is automatic Xtensa panic-frame capture.
Reset classification and post-reset recovery are verified; an ESP-IDF panic
hook for automatic register/stack capture is still future work.
