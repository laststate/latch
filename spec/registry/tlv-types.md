# Latch TLV Registry

## Overview

Unencrypted LEP v1 payload = contiguous TLVs, no padding:

```
type u16 LE | length u16 LE | value[length]
```

- `type == 0` is illegal.
- Unknown types: skip by length.
- Nested identity strings: `field_id u8 | str_len u8 | utf8[str_len]`.

## IDs

| Type | Name | Repeatable |
|-----:|------|:----------:|
| 0 | RESERVED | — |
| 1 | IDENTITY | no |
| 2 | RESET | no |
| 3 | EVENT | no |
| 4 | CPU | no |
| 5 | FAULT | no |
| 6 | BREADCRUMB | yes |
| 7 | METRIC | yes |
| 8 | POWER | yes |
| 9 | HEALTH | no |
| 10 | ASSERT | no |
| 11 | PERIPHERAL | no |
| 12 | LOG | no |
| 13 | MEMORY | yes |
| 14 | STACK | no |
| 15 | HEAP | no |
| 16 | CPU64 | no |
| 17 | BLACKBOX | yes |
| 18 | MISSION | no |
| 19 | TIME_SYNC | no |
| 20 | PROVISIONING | no |
| 21 | SUPERVISOR | no |
| 22 | ENVIRONMENT | no |
| 0x0020–0x0021 | attachment meta/chunk | yes (chunk) |
| 0x0030 | probe waveform (reserved) | — |
| 0x8000–0x8FFF | vendor | — |
| 0xF000–0xFFFE | experimental | — |

Header `event_type` (u8): 1 CRASH, 2 ERROR, 3 MESSAGE, 4 HEALTH, 5 RESET, 6 LOG, 7 PERIPHERAL, 8 COREDUMP.

Header flags: bit0 AUTH, 1 ENC, 2 AEAD, 3 TRUNCATED, 4 COMPRESSED. Known mask `0x1F`.

## Key Types

### 1 IDENTITY

Nested strings, field_id 1..15 in order: project_id, device_id, product, hardware_revision, bom_revision, manufacturing_batch, firmware_version, firmware_build_id, bootloader_version, git_commit, variant, architecture, rtos, region, device_group.

### 2 RESET (20 bytes)

`reason u8 | raw_reason u32 | boot_count u32 | previous_uptime_ms u32 | timestamp_ms u32 | expected u8 | previous_crashed u8 | boot_loop u8`

reason: 0 UNKNOWN … 4 WATCHDOG, 5 IWDG, 6 WWDG, 7 BROWNOUT, … 13 CLOCK_FAILURE (see Latch `ls_reset_reason`).

### 3 EVENT (28 bytes)

`priority u8 | severity u8 | capture_level u8 | domain_hash u32 | code u32 | message_hash u32 | fingerprint u32 | repeat_count u32 | first_seen_ms u32 | last_seen_ms u32`

severity 0..4 = DEBUG..FATAL. capture_level 0..4 = METADATA..FULL.

### 4 CPU / 5 FAULT

See the CPU/FAULT layouts in the LEP v1 architecture spec (protocol repository).

### 6 BREADCRUMB

`at_ms u32 | message_id u16 | severity u8 | category_hash u32 | message_hash u32` [+ optional nested strings 1=category, 2=message] `| value_count u8 | (key_id u16, type u8, value u32)xN`

value type: 0=i32, 1=u32, 2=bool.

### 7 METRIC

`name_hash u32` [+ optional name string field 1] `| type u8 | value | previous | min | max | sum u64 | count u32 | window_count u8 | window[] u32`

type: 1=i32, 2=u32, 3=bool, 4=counter.

### 8 POWER (16 bytes)

`timestamp_ms | vdd_mv | battery_mv | current_ma | temperature_c | charger_status | power_flags` (u32 + 6×u16).

### 9 HEALTH (10 bytes)

`watchdog_last_feed_ms u32 | checkpoint u16 | active_task_hash u32`

### 10 ASSERT

`line u32 | expression_hash u32 | file_hash u32` [+ optional nested strings expression/file/message].

### 11 PERIPHERAL (37 bytes)

`domain u8 | fault u16 | instance u16 | status u32 | address u32 | reg u32 | timeout_ms u32 | auxiliary[4] u32`

domain: 0 I2C … 6 NETWORK.

### 12 LOG

`message_id u16 | format_id u16 | argument_count u8 | arguments[N] u32`

### 13 MEMORY

`name_hash | address | region_length | flags | data or hash or zeros`

flags: SAFE, HASH, VOLATILE, SENSITIVE.

### 14 STACK

`stack_pointer u32 | length u32 | bytes[length]`

### 15 HEAP (20 bytes)

`free_bytes | minimum_free | largest_block | allocation_failures | pool_exhaustions`

### 16 CPU64

`encoding u8 | flags u8 | architecture u8 | word_size u8`

Encoding is `1`, architecture is `5` (`RISCV64`) and word size is `8`. Exactly one flag is set: `COMPLETE` (`0x01`) or `UNAVAILABLE` (`0x02`). A complete value is 292 bytes and appends `x0..x31`, `mstatus`, `mcause`, `mtval`, and `mepc` as 36 little-endian `u64` values. An unavailable value is exactly four bytes; consumers MUST NOT infer upper words from TLV 4 (`CPU`).

### 17 BLACKBOX (27 bytes, repeatable)

`encoding u8 | timestamp_ms u32 | kind u16 | source_id u16 | flags u16 | value[4] i32`

Encoding is `1`. A producer emits records oldest to newest within the bounded export window and omits records marked sensitive.

### 18 MISSION (46 bytes plus optional strings)

`encoding u8 | mission_hash u32 | dive_hash u32 | node_hash u32 | vehicle_mode_hash u32 | phase u32 | depth_cm i32 | elapsed_ms u32 | incident_hi u64 | incident_lo u64 | incident_active u8`

Encoding is `1`. Optional suffix fields as `field_id u8 | length u8 | UTF-8 bytes`: 1 `mission_id`, 2 `dive_id`, 3 `node_id`, 4 `vehicle_mode`.

### 19 TIME_SYNC (22 bytes)

`encoding u8 | source u8 | utc_ms_at_sync u64 | monotonic_ms_at_sync u32 | uncertainty_ms u32 | generation u32`

Encoding is `1`. Source values are 1 RTC, 2 GNSS, 3 NTP, 4 PTP, and 5 HOST.

### 20 PROVISIONING (18 bytes)

`encoding u8 | state u8 | key_id u32 | pending_key_id u32 | generation u32 | monotonic_counter u32`

Encoding is `1`. State values are 0 unprovisioned, 1 active, 2 rotating, 3 revoked, 4 decommissioned, and 5 decommissioning.

### 21 SUPERVISOR (17 bytes)

`encoding u8 | active_alarms u32 | previous_alarms u32 | transitions u32 | last_change_ms u32`

Encoding is `1`. Alarm bit definitions are product policy; consumers preserve unknown bits.

### 22 ENVIRONMENT (29 bytes)

`encoding u8 | timestamp_ms u32 | pressure_pa u32 | depth_cm i32 | internal_temperature_c i16 | humidity_permyriad u16 | vibration_mg_rms u16 | flags u16 | sample_count u32 | leak_events u32`

Encoding is `1`. Environment flags are bit 0 leak detected, bit 1 water ingress, bit 2 pressure-sensor fault, and bit 3 vibration limit.

## References

- [Latch Transport Spec](../transports.md)
- [Latch Encryption Spec](../encryption.md)
