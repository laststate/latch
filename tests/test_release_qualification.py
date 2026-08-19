# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tests/test_release_qualification.py
#
# Release qualification tests. HIL matrix validation.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
from __future__ import annotations

import datetime as dt
import importlib.util
import json
import pathlib
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "release_qualification", ROOT / "tools" / "check_release_qualification.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(MODULE)


def main() -> int:
    commit = "a" * 40
    assert MODULE.validate("0.2.0", commit, today=dt.date(2026, 8, 7)) == []
    missing = MODULE.validate("1.0.0", commit, ROOT / "does-not-exist.json",
                              today=dt.date(2026, 8, 7))
    assert missing and "requires qualification manifest" in missing[0]

    with tempfile.TemporaryDirectory() as directory:
        temp = pathlib.Path(directory)
        evidence = ROOT / "hil" / "releases" / "test-evidence.json"
        evidence.write_text(json.dumps({
            "schema_version": 1,
            "result": "pass",
            "commit": commit,
            "scenarios": ["hardfault", "watchdog", "brownout", "stack-canary", "flash", "reset-registers", "trustzone", "fpu-lazy"],
        }), encoding="utf-8")
        try:
            manifest = temp / "v1.0.0.json"
            manifest.write_text(json.dumps({
                "schema_version": 1,
                "release": "v1.0.0",
                "commit": commit,
                "generated_at": "2026-08-07",
                "targets": [{
                    "platform": "STM32",
                    "mcu": "STM32U585",
                    "board": "REV-A",
                    "toolchain": "Arm GNU Toolchain 14.2",
                    "features": ["trustzone", "fpu"],
                    "scenarios": ["hardfault", "watchdog", "brownout", "stack-canary", "flash", "reset-registers", "trustzone", "fpu-lazy"],
                    "evidence": "releases/test-evidence.json",
                }],
            }), encoding="utf-8")
            assert MODULE.validate("1.0.0", commit, manifest,
                                   today=dt.date(2026, 8, 7)) == []

            stale = json.loads(manifest.read_text())
            stale["generated_at"] = "2025-01-01"
            manifest.write_text(json.dumps(stale), encoding="utf-8")
            errors = MODULE.validate("1.0.0", commit, manifest,
                                     today=dt.date(2026, 8, 7))
            assert any("stale" in error for error in errors)
        finally:
            evidence.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
