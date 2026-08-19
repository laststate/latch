# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tests/test_footprint.py
#
# Footprint measurement script. Parses map/elf for RAM/flash use.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Unit tests for the target-agnostic footprint report generator."""

from __future__ import annotations

import json
import pathlib
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from measure_footprint import (  # noqa: E402
    artifact_kind,
    make_report,
    parse_berkeley_size,
    render_markdown,
    stack_summary,
)


def main() -> int:
    sections, rows = parse_berkeley_size(
        """text    data     bss     dec     hex filename
   100      20      30     150      96 core.o (ex liblatch.a)
    50       5      10      65      41 capture.o (ex liblatch.a)
"""
    )
    assert rows == 2
    assert sections == {"text": 150, "data": 25, "bss": 40}
    assert parse_berkeley_size("text data bss\n0010 0x10 0002\n")[0] == {
        "text": 10,
        "data": 16,
        "bss": 2,
    }

    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        archive = root / "liblatch.a"
        archive.write_bytes(b"!<arch>\n")
        assert artifact_kind(archive) == "static archive"

        stack_dir = root / "stack"
        stack_dir.mkdir()
        (stack_dir / "latch.su").write_text(
            "src/core/runtime.c:10:1:ls_init\t32\tstatic\n"
            "src/core/runtime.c:20:1:ls_capture\t48\tdynamic,bounded\n"
            "src/core/runtime.c:30:1:ls_flush\t64\tdynamic\n",
            encoding="utf-8",
        )
        stack = stack_summary([stack_dir])
        assert stack["compiler_reports"] == 1
        assert stack["functions_reported"] == 3
        assert stack["max_reported_frame_bytes"] == 64
        assert stack["max_static_frame_bytes"] == 32
        assert stack["variable_or_unknown_frame_functions"] == 2
        assert [record["function"] for record in stack["largest_reported_frames"]] == [
            "ls_flush",
            "ls_capture",
            "ls_init",
        ]

        report = make_report(archive, "arm-none-eabi-size", sections, rows, stack, "fixture")
        assert report["flash_estimate_bytes"] == 175
        assert report["static_ram_estimate_bytes"] == 65
        assert report["measurement_scope"].startswith("archive-member aggregate")
        assert json.loads(json.dumps(report))["schema_version"] == 1
        markdown = render_markdown(report)
        assert "Flash estimate (`.text + .data`) | 175" in markdown
        assert "worst-case call-stack depth" in markdown

        elf = root / "firmware.elf"
        elf.write_bytes(b"\x7fELF\x01\x01\x01" + (b"\x00" * 9) + b"\x02\x00")
        assert artifact_kind(elf) == "ELF executable image"

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
