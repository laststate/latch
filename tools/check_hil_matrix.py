# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/check_hil_matrix.py
#
# HIL qualification matrix validator. Ensures claims match the
# hardware-compatibility.md table.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Validate qualification claims and keep the public HIL table reproducible."""

from __future__ import annotations

import json
import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "hil" / "qualification-matrix.json"
DOCUMENT = ROOT / "docs" / "hardware-compatibility.md"
START = "<!-- qualification-matrix:start -->"
END = "<!-- qualification-matrix:end -->"

ALLOWED_COMPONENT = {
    "qualified",
    "integration-boundary",
    "integration-required",
    "emulator-tested",
    "host-tested",
    "not-supplied",
    "not-tested",
}
ALLOWED_STATUS = {
    "qualified-limited",
    "in-validation",
    "not-qualified",
    "compile-tested",
    "emulator-tested",
    "host-tested",
}
ICONS = {
    "qualified": "✅ Qualified",
    "integration-boundary": "⚠️ Integration boundary",
    "integration-required": "⚠️ Product integration",
    "emulator-tested": "🧪 Emulator-tested",
    "host-tested": "🧪 Host-tested",
    "not-supplied": "❌ Not supplied",
    "not-tested": "❌ Not tested",
}
STATUS = {
    "qualified-limited": "Qualified (limited scope)",
    "in-validation": "In validation",
    "not-qualified": "Not qualified",
    "compile-tested": "Compile-tested",
    "emulator-tested": "Emulator-tested",
    "host-tested": "Host-tested",
}


def load() -> dict:
    return json.loads(SOURCE.read_text(encoding="utf-8"))


def validate(data: dict) -> list[str]:
    errors: list[str] = []
    if data.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    rows = data.get("platforms")
    if not isinstance(rows, list) or not rows:
        return errors + ["platforms must be a non-empty list"]
    seen: set[tuple[str, str, str]] = set()
    required = {
        "platform", "mcu", "board", "toolchain", "fault_handler", "flash_backend",
        "status", "last_hil", "evidence", "notes",
    }
    for index, row in enumerate(rows):
        missing = required - set(row)
        if missing:
            errors.append(f"row {index}: missing {sorted(missing)}")
            continue
        key = (row["platform"], row["mcu"], row["board"])
        if key in seen:
            errors.append(f"row {index}: duplicate platform identity {key}")
        seen.add(key)
        for field in ("fault_handler", "flash_backend"):
            if row[field] not in ALLOWED_COMPONENT:
                errors.append(f"row {index}: invalid {field} {row[field]!r}")
        if row["status"] not in ALLOWED_STATUS:
            errors.append(f"row {index}: invalid status {row['status']!r}")
        qualified = row["status"] == "qualified-limited"
        if qualified and (not row["last_hil"] or not row["evidence"]):
            errors.append(f"row {index}: qualification requires dated evidence")
        if row["last_hil"] and not re.fullmatch(r"\d{4}-\d{2}-\d{2}", row["last_hil"]):
            errors.append(f"row {index}: last_hil must be ISO-8601 or null")
        if row["evidence"]:
            evidence = (SOURCE.parent / row["evidence"]).resolve()
            if not evidence.is_file() or SOURCE.parent.resolve() not in evidence.parents:
                errors.append(f"row {index}: evidence does not resolve inside hil/")
    return errors


def render(data: dict) -> str:
    lines = [
        START,
        "| Platform | MCU / board | Toolchain | Fault handler | Flash backend | Status | Last physical HIL |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for row in data["platforms"]:
        mcu = f"{row['mcu']} / {row['board']}"
        last = row["last_hil"] or "—"
        if row["evidence"]:
            relative = pathlib.PurePosixPath("../hil") / pathlib.PurePosixPath(row["evidence"])
            last = f"[{last}]({relative})"
        values = (
            row["platform"], mcu, row["toolchain"], ICONS[row["fault_handler"]],
            ICONS[row["flash_backend"]], STATUS[row["status"]], last,
        )
        lines.append("| " + " | ".join(value.replace("|", "\\|") for value in values) + " |")
    lines.append(END)
    return "\n".join(lines)


def main() -> int:
    data = load()
    errors = validate(data)
    document = DOCUMENT.read_text(encoding="utf-8")
    expected = render(data)
    current_match = re.search(re.escape(START) + r".*?" + re.escape(END), document, re.DOTALL)
    if not current_match:
        errors.append("hardware compatibility document has no generated matrix markers")
    elif current_match.group(0) != expected:
        errors.append("hardware compatibility matrix is stale; update it from hil/qualification-matrix.json")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"qualification matrix valid: {len(data['platforms'])} platform entries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
