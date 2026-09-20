# Power-fail seal (Last-Microjoule commit)

Latch survives brownout resets that erase ordinary evidence. A PVD/NMI
handler seals a fixed 64-byte record into retained memory with bounded,
heap-free stores only. Normal boot promotes a valid seal into the spool
before clearing it. Not hardware-tested.

## Context separation

- NMI/PVD path (`ls_powerfail_seal`): retained memory plus the installed
  backup window only. No allocation, storage, transport, crypto, logging,
  locks, or reset callbacks. A second NMI during the same brownout seals
  once and stops so a dying rail cannot torn-write the record.
- Normal runtime (`ls_powerfail_recover`, called by `ls_boot`): validates
  magic, version, reason range, and CRC; emits an `EMERGENCY` reset event
  with TLV 23; appends to the spool; clears the seal only after the append
  succeeds. Interrupted records stay ignored on recovery.

## Tiers

- Tier RAM: internal `.noinit` record. Survives brownout reset while SRAM
  stays powered.
- Tier backup RAM: integrator registers a VBAT-retained window with
  `ls_powerfail_install_backup()`. The NMI path mirrors the seal there
  with direct bounded stores. Recovery prefers `.noinit` and falls back
  to the mirror. True power-loss with dead VBAT is out of scope for the
  portable runtime and must be qualified per board.

## Wire format

Additive LEP v1 TLV 23 (`POWERFAIL_SEAL`): encoding `1`, reason, tier,
`vcap_mv` little-endian `u16`, `boot_id` `u32`, fault `u32` (13 bytes).
Legacy decoders skip it by length. `latch-dump` prints it as
`powerfail_seal`.

## Integration

```c
static uint8_t backup_ram[128]; /* VBAT-retained section via linker */

ls_powerfail_install_backup(backup_ram, sizeof(backup_ram));

/* In the PVD/NMI ISR, after switching to a known-good stack: */
ls_powerfail_seal(LS_POWERFAIL_BROWNOUT, vcap_mv);
```

Call `ls_powerfail_install_backup()` once after `ls_init()`, before
`ls_boot()`. `ls_boot()` promotes automatically. `ls_boot_mark_successful()`
clears the in-stream seal flag. SPI FRAM and single-shot Flash slots are
not claimed; they need board-level HIL with a programmable supply before
any production claim. See [HIL](hil.md) and
[hardware compatibility](hardware-compatibility.md).
