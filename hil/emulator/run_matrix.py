#!/usr/bin/env python3
"""Run the Renode emulator chip matrix and collect per-chip evidence.

Reads chips.json, builds the generic fault-path firmware for each chip with the
appropriate GCC toolchain, generates a .ld/.resc/.robot fixture, runs
renode-test.bat, and records evidence JSON + Markdown in
hil/emulator/evidence/<chip>/.
"""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EMU = REPO / "hil" / "emulator"
BUILD = REPO / "build" / "emulator"
EVIDENCE = EMU / "evidence"

CONFIG = json.loads((EMU / "chips.json").read_text(encoding="utf-8"))
RENODE_TEST = Path(CONFIG["renode_path"]) / "renode-test.bat"
ARM_GCC = Path(CONFIG["arm_gcc"])
RISCV_GCC = Path(CONFIG["riscv_gcc"])

SCENARIO = "USAGEFAULT"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_elf(chip: dict, ld: Path, elf: Path, marker: int, scenario: str) -> None:
    if chip["arch"] == "cortex-m":
        src = EMU / "renode_cortex_m.c"
        asm = REPO / "arch" / "cortex-m" / "cortex_m_fault.S"
        cmd = [
            str(ARM_GCC),
            str(src),
            str(asm),
            f"-DLS_EMULATOR_{scenario}=1",
            f"-DLS_RENODE_MARKER_ADDR=0x{marker:X}",
            f"-mcpu={chip['mcpu']}",
            "-mthumb",
            "-Os",
            "-ffreestanding",
            "-fno-builtin",
            "-nostdlib",
            f"-Wl,--gc-sections,-T,{ld}",
            "-o",
            str(elf),
        ]
    else:
        src = EMU / "renode_riscv.c"
        cmd = [
            str(RISCV_GCC),
            str(src),
            f"-march={chip['march']}",
            f"-mabi={chip['mabi']}",
            f"-DLS_RENODE_MARKER_ADDR=0x{marker:X}",
            "-mcmodel=medany",
            "-Os",
            "-ffreestanding",
            "-fno-builtin",
            "-nostdlib",
            f"-Wl,--gc-sections,-T,{ld}",
            "-o",
            str(elf),
        ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        raise RuntimeError(f"build failed for {chip['id']}:\n{res.stderr}")


def write_ld(chip: dict) -> Path:
    ld = EVIDENCE / chip["id"] / f"{chip['id']}.ld"
    ld.parent.mkdir(parents=True, exist_ok=True)
    if chip["arch"] == "cortex-m":
        ld.write_text(
            f"""ENTRY(Reset_Handler)

MEMORY
{{
    FLASH (rx)  : ORIGIN = 0x{chip['flash_base']:08X}, LENGTH = {chip['flash_size']}
    RAM   (rwx) : ORIGIN = 0x{chip['ram_base']:08X}, LENGTH = {chip['ram_size']}
}}

SECTIONS
{{
    .isr_vector : {{ KEEP(*(.isr_vector)) }} > FLASH
    .text : {{ *(.text*) *(.rodata*) }} > FLASH
    .data : {{ *(.data*) }} > RAM AT > FLASH
    .bss (NOLOAD) : {{ *(.bss*) *(COMMON) }} > RAM
    . = ALIGN(8);
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}}
""",
            encoding="utf-8",
        )
    else:
        ld.write_text(
            f"""ENTRY(_start)

MEMORY
{{
    FLASH (rx)  : ORIGIN = 0x{chip['flash_base']:08X}, LENGTH = {chip['flash_size']}
    RAM   (rwx) : ORIGIN = 0x{chip['ram_base']:08X}, LENGTH = {chip['ram_size']}
}}

SECTIONS
{{
    .start : {{ KEEP(*(.start)) }} > FLASH
    .text : {{ *(.text*) *(.rodata*) }} > FLASH
    .data : {{ *(.data*) }} > RAM AT > FLASH
    .bss (NOLOAD) : {{ *(.bss*) *(COMMON) }} > RAM
    . = ALIGN(16);
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}}
""",
            encoding="utf-8",
        )
    return ld


def write_resc(chip: dict) -> Path:
    resc = EVIDENCE / chip["id"] / f"{chip['id']}.resc"
    resc.parent.mkdir(parents=True, exist_ok=True)
    repl = (Path(CONFIG["renode_path"]) / chip["repl"]).as_posix()
    lines = [
        "using sysbus",
        f'mach create "latch-{chip["id"]}"',
        f'machine LoadPlatformDescription "{repl}"',
    ]
    vto = chip.get("vector_table_offset")
    if chip["arch"] == "cortex-m" and vto:
        lines.append(f"{chip['cpu_node']} VectorTableOffset 0x{vto:X}")
    lines.append(f"{chip['cpu_node']} PerformanceInMips 64")
    resc.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return resc


def write_robot(chip: dict, elf: Path, resc: Path) -> Path:
    robot = EVIDENCE / chip["id"] / f"{chip['id']}.robot"
    robot.parent.mkdir(parents=True, exist_ok=True)
    marker = chip["marker"]
    script = str(resc).replace("\\", "/")
    elfpath = str(elf).replace("\\", "/")
    lines = [
        "*** Settings ***",
        "Library           RenodeLibrary",
        "Library           String",
        "",
        "*** Variables ***",
        f"${{SCRIPT}}         {script}",
        f"${{ELF}}            {elfpath}",
        f"${{MARKER}}         0x{marker:X}",
        "",
        "*** Keywords ***",
        "Read Word",
        "    [Arguments]    ${addr}",
        "    ${raw}=  Execute Command  sysbus ReadDoubleWord ${addr}",
        "    ${val}=  Strip String  ${raw}",
        "    [Return]    ${val}",
        "",
        "Wait For Marker",
        "    ${magic}=  Read Word  ${MARKER}",
        "    Should Be Equal As Strings  ${magic}  0x4C415443",
        f"    ${{status_raw}}=  Read Word  0x{marker + 4:X}",
        "    ${status}=  Convert To Integer  ${status_raw}",
        "    Should Be Equal As Integers  ${status}  0",
        "",
        "*** Test Cases ***",
    ]
    if chip["arch"] == "cortex-m":
        lines += [
            f"{chip['id']} {SCENARIO} Reaches Production Handler",
            "    Execute Script         ${SCRIPT}",
            "    Execute Command        sysbus LoadELF @${ELF}",
            f"    Execute Command        {chip['cpu_node']} Reset",
            "    Start Emulation",
            "    Wait Until Keyword Succeeds    10    0.5    Wait For Marker",
        ]
    else:
        lines += [
            f"{chip['id']} Illegal Instruction Reaches Trap Handler",
            "    Execute Script         ${SCRIPT}",
            "    Execute Command        sysbus LoadELF @${ELF}",
            f"    Execute Command        {chip['cpu_node']} PC `sysbus GetSymbolAddress \"_start\"`",
            "    Start Emulation",
            "    Wait Until Keyword Succeeds    10    0.5    Wait For Marker",
        ]
    robot.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return robot


def run_renode(robot: Path) -> tuple[bool, Path]:
    log = Path(os.environ.get("TEMP", "/tmp")) / f"latch-{robot.stem}-renode.log"
    res = subprocess.run(
        [str(RENODE_TEST), str(robot)],
        cwd=REPO,
        capture_output=True,
        text=True,
        timeout=600,
    )
    log.write_text(res.stdout + res.stderr, encoding="utf-8", errors="replace")
    return res.returncode == 0, log


def collect_evidence(chip: dict, elf: Path, log: Path, ok: bool, dt: float) -> Path:
    chip_dir = EVIDENCE / chip["id"]
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    ev = {
        "schema_version": 1,
        "result": "pass" if ok else "fail",
        "execution": "Renode",
        "chip": chip["id"],
        "family": chip["family"],
        "mcu": chip["note"],
        "arch": chip["arch"],
        "scenario": SCENARIO,
        "fault_source": "firmware UDF instruction" if chip["arch"] == "cortex-m" else "illegal instruction trap",
        "firmware": str(elf),
        "firmware_sha256": sha256(elf) if elf.exists() else None,
        "linker": str(chip_dir / f"{chip['id']}.ld"),
        "linker_sha256": sha256(chip_dir / f"{chip['id']}.ld"),
        "marker_address": f"0x{chip['marker']:X}",
        "observed_at": datetime.now(timezone.utc).isoformat(),
        "duration_s": round(dt, 2),
        "renode_log": str(log),
        "renode_log_sha256": sha256(log),
        "robot_output": str(REPO / "robot_output.xml"),
        "robot_output_sha256": (
            sha256(REPO / "robot_output.xml") if (REPO / "robot_output.xml").exists() else None
        ),
        "caveat": (
            "This validates Renode's board model, vector table, toolchain path and fault "
            "entry for this architecture; it is not physical-board qualification and does "
            "not validate silicon errata, reset registers, flash timing, power loss, or a "
            "vendor bootloader."
        ),
    }
    out = chip_dir / f"{stamp}.json"
    out.write_text(json.dumps(ev, indent=2), encoding="utf-8")
    md = out.with_suffix(".md")
    md.write_text(
        f"""# Renode evidence: {chip['id']}

- Result: **{'PASS' if ok else 'FAIL'}**
- Family: `{chip['family']}` ({chip['note']})
- Architecture: `{chip['arch']}`
- Scenario: `{SCENARIO}`
- Observed at (UTC): {ev['observed_at']}
- Duration: {ev['duration_s']} s

## Assertions

1. The Renode board model for `{chip['id']}` loads and the CPU is the expected architecture.
2. The firmware vector table / `_start` is accepted and booted.
3. The fault scenario reached the production fault entry and reported `status == 0`.

## Raw evidence

- Robot XML SHA-256: {ev['robot_output_sha256']}
- Renode log: {ev['renode_log']}
- Renode log SHA-256: {ev['renode_log_sha256']}
- Firmware ELF SHA-256: {ev['firmware_sha256']}

## Scope

{ev['caveat']}
""",
        encoding="utf-8",
    )
    return out


def main() -> int:
    only = sys.argv[1:] or None
    total = 0
    passed = 0
    failed = []
    for chip in CONFIG["chips"]:
        if only and chip["id"] not in only:
            continue
        total += 1
        chip_dir = EVIDENCE / chip["id"]
        chip_dir.mkdir(parents=True, exist_ok=True)
        t0 = time.time()
        print(f"=== {chip['id']} ({chip['family']}) ===", flush=True)
        ok = False
        log = None
        elf = BUILD / f"latch-renode-{chip['id']}.elf"
        try:
            marker = chip["ram_base"] + 0x1000
            chip["marker"] = marker
            ld = write_ld(chip)
            build_elf(chip, ld, elf, marker, SCENARIO)
            resc = write_resc(chip)
            robot = write_robot(chip, elf, resc)
            ok, log = run_renode(robot)
            dt = time.time() - t0
            collect_evidence(chip, elf, log, ok, dt)
            if ok:
                passed += 1
                print(f"  PASS ({dt:.1f}s)", flush=True)
            else:
                failed.append(chip["id"])
                print(f"  FAIL ({dt:.1f}s)  log: {log}", flush=True)
        except Exception as exc:  # noqa: BLE001
            failed.append(chip["id"])
            print(f"  ERROR: {exc}", flush=True)
            if log:
                print(f"  log: {log}", flush=True)
    print(f"\n{passed}/{total} passed; failed: {failed or 'none'}")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())