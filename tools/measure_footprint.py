# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/measure_footprint.py
#
# Footprint measurement. Parses ELF/map for RAM/flash breakdown.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Produce reproducible, target-agnostic size reports for Latch artifacts.

The report intentionally distinguishes a linked image from a static archive.
An archive contains every member selected at build time, while a final firmware
image also depends on the application, linker script, link-time garbage
collection, startup code, and external libraries.  Neither report is a
measurement of physical-board RAM, Flash, or complete call-stack use.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys
from collections.abc import Iterable
from typing import Any


ARCHIVE_MAGIC = b"!<arch>\n"
ELF_MAGIC = b"\x7fELF"


def parse_nonnegative_integer(value: str) -> int | None:
    """Return a Berkeley-size number, accepting decimal and hexadecimal text."""
    try:
        number = int(value, 16) if value.lower().startswith("0x") else int(value, 10)
    except ValueError:
        return None
    return number if number >= 0 else None


def parse_berkeley_size(output: str) -> tuple[dict[str, int], int]:
    """Sum the text, data, and bss columns emitted by GNU or LLVM ``size``.

    ``size --format=Berkeley`` emits one row per archive member, so summing its
    rows gives a useful archive-member aggregate as well as a normal linked
    image result.
    """
    totals = {"text": 0, "data": 0, "bss": 0}
    rows = 0
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 3:
            continue
        values = [parse_nonnegative_integer(field) for field in fields[:3]]
        if any(value is None for value in values):
            continue
        text, data, bss = values
        assert text is not None and data is not None and bss is not None
        totals["text"] += text
        totals["data"] += data
        totals["bss"] += bss
        rows += 1
    if rows == 0:
        raise ValueError("the size tool did not emit Berkeley text/data/bss rows")
    return totals, rows


def artifact_kind(path: pathlib.Path) -> str:
    """Identify the artifact shape without assuming a target architecture."""
    try:
        with path.open("rb") as artifact:
            header = artifact.read(20)
    except OSError:
        return "unknown"
    if header.startswith(ARCHIVE_MAGIC):
        return "static archive"
    if not header.startswith(ELF_MAGIC) or len(header) < 18:
        return "unknown"
    byte_order = "little" if header[5:6] == b"\x01" else "big"
    elf_type = int.from_bytes(header[16:18], byte_order)
    return {
        1: "ELF relocatable object",
        2: "ELF executable image",
        3: "ELF shared object",
    }.get(elf_type, "ELF artifact")


def run_size_tool(tool: str, artifact: pathlib.Path) -> str:
    """Run a GNU/LLVM-compatible size tool with its stable Berkeley format."""
    commands = (
        [tool, "--format=Berkeley", str(artifact)],
        [tool, "-B", str(artifact)],
    )
    failures: list[str] = []
    for command in commands:
        try:
            completed = subprocess.run(command, check=False, capture_output=True, text=True)
        except FileNotFoundError as error:
            raise RuntimeError(f"size tool not found: {tool}") from error
        if completed.returncode == 0:
            return completed.stdout
        failures.append(completed.stderr.strip() or " ".join(command))
    raise RuntimeError("size tool failed: " + "; ".join(failures))


def stack_usage_files(paths: Iterable[pathlib.Path]) -> list[pathlib.Path]:
    """Return deterministic, de-duplicated compiler ``.su`` paths."""
    found: dict[pathlib.Path, pathlib.Path] = {}
    for path in paths:
        if path.is_file() and path.suffix == ".su":
            resolved = path.resolve()
            found[resolved] = path
        elif path.is_dir():
            for candidate in path.rglob("*.su"):
                if candidate.is_file():
                    resolved = candidate.resolve()
                    found[resolved] = candidate
        else:
            raise ValueError(f"stack-usage path does not exist: {path}")
    return sorted(found.values(), key=lambda path: str(path))


def parse_stack_usage_file(path: pathlib.Path) -> list[dict[str, Any]]:
    """Parse GCC/Clang ``-fstack-usage`` records without inferring call depth."""
    records: list[dict[str, Any]] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        fields = line.split("\t")
        if len(fields) < 3:
            fields = line.rsplit(None, 2)
        if len(fields) < 3:
            raise ValueError(f"{path}:{line_number}: invalid stack-usage record")
        source_and_function, byte_text, classification = fields[0], fields[1], fields[2]
        reported_bytes = parse_nonnegative_integer(byte_text)
        function = source_and_function.rsplit(":", 1)[-1]
        records.append(
            {
                "source": source_and_function,
                "function": function,
                "reported_frame_bytes": reported_bytes,
                "classification": classification.strip(),
                "stack_usage_file": str(path),
            }
        )
    return records


def stack_summary(paths: Iterable[pathlib.Path]) -> dict[str, Any]:
    files = stack_usage_files(paths)
    records = [record for path in files for record in parse_stack_usage_file(path)]
    numeric = [record for record in records if record["reported_frame_bytes"] is not None]
    static = [record for record in numeric if record["classification"] == "static"]
    variable = [record for record in records if record["classification"] != "static"]
    largest = sorted(
        numeric,
        key=lambda record: (-record["reported_frame_bytes"], record["function"], record["source"]),
    )[:10]
    return {
        "compiler_reports": len(files),
        "functions_reported": len(records),
        "max_reported_frame_bytes": max(
            (record["reported_frame_bytes"] for record in numeric), default=None
        ),
        "max_static_frame_bytes": max(
            (record["reported_frame_bytes"] for record in static), default=None
        ),
        "variable_or_unknown_frame_functions": len(variable),
        "largest_reported_frames": largest,
    }


def measurement_scope(kind: str) -> str:
    if kind == "static archive":
        return "archive-member aggregate (not a final linked firmware image)"
    if kind == "ELF executable image":
        return "linked image section aggregate"
    if kind == "ELF relocatable object":
        return "relocatable-object section aggregate (not a final linked firmware image)"
    return "artifact section aggregate"


def make_report(
    artifact: pathlib.Path,
    tool: str,
    sections: dict[str, int],
    rows: int,
    stack: dict[str, Any],
    label: str | None,
) -> dict[str, Any]:
    kind = artifact_kind(artifact)
    return {
        "schema_version": 1,
        "label": label or artifact.name,
        "artifact": str(artifact),
        "artifact_kind": kind,
        "measurement_scope": measurement_scope(kind),
        "size_tool": tool,
        "size_rows": rows,
        "sections_bytes": sections,
        "flash_estimate_bytes": sections["text"] + sections["data"],
        "initialized_ram_bytes": sections["data"],
        "zero_initialized_ram_bytes": sections["bss"],
        "static_ram_estimate_bytes": sections["data"] + sections["bss"],
        "compiler_stack_usage": stack,
        "limitations": [
            "Flash and RAM values are section aggregates, not physical-board measurements.",
            "Final firmware size also depends on the application, linker script, startup code, external libraries, alignment, and linker garbage collection.",
            "Compiler stack-usage records describe individual frames only; they do not establish worst-case call-stack depth, interrupt nesting, or runtime stack allocation.",
        ],
    }


def render_markdown(report: dict[str, Any]) -> str:
    sections = report["sections_bytes"]
    stack = report["compiler_stack_usage"]
    lines = [
        f"# Footprint report: {report['label']}",
        "",
        f"- Artifact: `{report['artifact']}` ({report['artifact_kind']})",
        f"- Scope: {report['measurement_scope']}",
        f"- Size tool: `{report['size_tool']}`; section rows: {report['size_rows']}",
        "",
        "| Metric | Bytes |",
        "| --- | ---: |",
        f"| `.text` | {sections['text']} |",
        f"| `.data` | {sections['data']} |",
        f"| `.bss` | {sections['bss']} |",
        f"| Flash estimate (`.text + .data`) | {report['flash_estimate_bytes']} |",
        f"| Static RAM estimate (`.data + .bss`) | {report['static_ram_estimate_bytes']} |",
        "",
        "## Compiler stack-usage records",
        "",
        f"- Reports: {stack['compiler_reports']}; functions: {stack['functions_reported']}",
        f"- Largest reported frame: {stack['max_reported_frame_bytes'] if stack['max_reported_frame_bytes'] is not None else 'n/a'} bytes",
        f"- Largest static frame: {stack['max_static_frame_bytes'] if stack['max_static_frame_bytes'] is not None else 'n/a'} bytes",
        f"- Variable or unknown frame records: {stack['variable_or_unknown_frame_functions']}",
    ]
    if stack["largest_reported_frames"]:
        lines.extend(["", "| Function | Frame bytes | Classification |", "| --- | ---: | --- |"])
        for record in stack["largest_reported_frames"]:
            lines.append(
                f"| `{record['function']}` | {record['reported_frame_bytes']} | {record['classification']} |"
            )
    lines.extend(["", "## Limits", ""])
    lines.extend(f"- {limitation}" for limitation in report["limitations"])
    return "\n".join(lines) + "\n"


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact", type=pathlib.Path, help="archive, ELF, or object to inspect")
    parser.add_argument(
        "--tool",
        default="size",
        help="GNU or LLVM size-compatible executable (default: %(default)s)",
    )
    parser.add_argument(
        "--stack-usage-dir",
        type=pathlib.Path,
        action="append",
        default=[],
        help="directory (or .su file) emitted with -fstack-usage; may be repeated",
    )
    parser.add_argument("--label", help="stable report heading; defaults to the artifact filename")
    parser.add_argument(
        "--format",
        choices=("markdown", "json"),
        default="markdown",
        help="report format (default: %(default)s)",
    )
    parser.add_argument("--output", type=pathlib.Path, help="write report to this path instead of stdout")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_argument_parser().parse_args(argv)
    if not args.artifact.is_file():
        raise SystemExit(f"artifact does not exist or is not a file: {args.artifact}")
    try:
        sections, rows = parse_berkeley_size(run_size_tool(args.tool, args.artifact))
        report = make_report(
            args.artifact,
            args.tool,
            sections,
            rows,
            stack_summary(args.stack_usage_dir),
            args.label,
        )
    except (OSError, RuntimeError, ValueError) as error:
        raise SystemExit(f"footprint measurement failed: {error}") from error
    output = (
        json.dumps(report, indent=2, sort_keys=True) + "\n"
        if args.format == "json"
        else render_markdown(report)
    )
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
    else:
        sys.stdout.write(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
