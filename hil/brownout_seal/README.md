# Brownout seal HIL — procedure (EVIDENCE: NOT RUN)

Status: **procedure staged, no physical run yet.** Do not cite this fixture
as evidence until the table in `EVIDENCE.md` records a PASS with board,
revision, SDK/toolchain, firmware commit, scenario, and decoded result.
A host or simulator test is not HIL evidence.

## Goal

Prove the [power-fail seal](../../docs/powerfail-seal.md) end to end on a
physical board: PVD/NMI fires on a dying rail, `ls_powerfail_seal()` commits
a fixed record, reboot promotes it as an `EMERGENCY` reset event with LEP
TLV 23, and `latch-dump` decodes `powerfail_seal` with `sealed_crc_ok`
semantics intact.

## Destructive-test warning

This procedure cuts board power mid-execution, forces watchdog and nested
faults, and programs a disposable Flash sector. Run only on a bench board
with a debug probe attached for recovery. Never run on a shared runner;
never run against production hardware, production keys, or a production
spool.

## Lab requirements

- Debug probe able to flash and recover the exact target.
- Dedicated control UART (commands) plus Latch transport UART.
- Current-limited programmable (SCPI) supply for ramp and cut scenarios.
- Board with PVD/BOD interrupt routed to `ls_powerfail_seal()` and a
  VBAT-retained RAM window registered via `ls_powerfail_install_backup()`.
- Disposable Flash test sector outside application, bootloader, and spool
  (only needed for the Tier-2 single-shot scenario, currently out of scope
  for a PASS claim).

## Firmware under test

- Latch commit: `<fill at run time>`.
- Board / revision / SDK / toolchain / linker script: `<fill at run time>`.
- PVD threshold (`mV`), backup-RAM section, measured `vcap_mv` at NMI:
  `<fill at run time>`.
- The HIL firmware accepts `HIL:RUN:BROWNOUT_SEAL` over control UART, emits
  `HIL:ARMED:BROWNOUT_SEAL` immediately before arming the PVD, then after
  reboot inspects the retained seal, the spool, and the reset registers and
  emits `HIL:PASS:BROWNOUT_SEAL` or `HIL:FAIL:<reason>`.

## Scenarios

1. **Ramp 3.3V -> 2.0V in 10ms**: PVD must fire, seal must commit, reboot
   must promote exactly one `EMERGENCY` event with TLV 23, `reason=brownout`.
2. **Hard cut 3.3V -> 0V**: if `.noinit` dies, the backup-RAM mirror must
   still recover; otherwise the run records which tier survived.
3. **Brownout during `spool_flush`**: no torn record may replace the last
   committed envelope; corrupt/torn candidates stay ignored with counters.
4. **NMI nested inside HardFault**: exactly one seal (no torn rewrite);
   both the fault snapshot and the seal must promote in priority order.

## Pass criteria (all required, 100/100 cuts per scenario 1–2)

- `latch-dump --json` validates each promoted envelope; TLV 23 present with
  `encoding=1`, plausible `vcap_mv`, and matching `boot_id`.
- `sent == 1` per seal; no duplicate promotion after `ls_boot_mark_successful()`.
- No `HIL:FAIL` on any run; raw UART logs retained with the evidence record.

## Evidence

See `EVIDENCE.md` in this directory. Current content: NOT RUN table only.
