# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/check_safety_c.py
#
# Safety-C coding standard checker. MISRA/CERT rule enforcement.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Fail on a small, auditable set of unsafe/host-only C patterns in production trees.

This is not a MISRA/CERT certification tool. It is a deterministic repository
policy gate that prevents accidental dynamic allocation and historically unsafe
C string/format APIs from entering the embedded runtime.
"""
from __future__ import annotations
import argparse, re, sys
from pathlib import Path

FORBIDDEN = {
    "malloc": "dynamic allocation is forbidden in the production runtime",
    "calloc": "dynamic allocation is forbidden in the production runtime",
    "realloc": "dynamic allocation is forbidden in the production runtime",
    "free": "dynamic allocation is forbidden in the production runtime",
    "gets": "unbounded input API",
    "strcpy": "unbounded string copy API",
    "strcat": "unbounded string concatenation API",
    "sprintf": "unbounded formatting API",
    "vsprintf": "unbounded formatting API",
    "system": "shell execution is not permitted in embedded runtime",
}

TOKEN = re.compile(r"\b(" + "|".join(map(re.escape, FORBIDDEN)) + r")\s*\(")


def strip_comments_and_literals(text: str) -> str:
    # Preserve line breaks/length approximately so diagnostics remain useful.
    pattern = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
    return pattern.sub(lambda m: "".join("\n" if c == "\n" else " " for c in m.group(0)), text)


def scan(root: Path, trees: list[str]) -> list[str]:
    findings: list[str] = []
    for tree in trees:
        directory = root / tree
        if not directory.exists():
            continue
        for path in sorted(directory.rglob("*")):
            if path.suffix not in {".c", ".h"} or not path.is_file():
                continue
            cleaned = strip_comments_and_literals(path.read_text(encoding="utf-8", errors="replace"))
            for match in TOKEN.finditer(cleaned):
                line = cleaned.count("\n", 0, match.start()) + 1
                api = match.group(1)
                findings.append(f"{path.relative_to(root)}:{line}: {api}(): {FORBIDDEN[api]}")
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--tree", action="append", dest="trees")
    args = parser.parse_args()
    trees = args.trees or ["src", "arch", "ports"]
    findings = scan(args.root.resolve(), trees)
    if findings:
        print("Safety-C policy violations:", file=sys.stderr)
        print("\n".join(findings), file=sys.stderr)
        return 1
    print(f"Safety-C policy PASS: scanned {', '.join(trees)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
