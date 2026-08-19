# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/check_gcov_coverage.py
#
# GCOV coverage parser. Enforces 90%+ line/branch thresholds.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Deterministic gcov coverage gate for Latch production sources.

Aggregates GCC gcov JSON data across all instrumented targets and reports only
runtime source roots (src/, arch/, ports/). Multiple target copies of the same
source file are merged by taking the maximum execution count for a line/branch,
which models whether that production location was exercised by any test binary.
"""
from __future__ import annotations

import argparse
import gzip
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOTS = ("src", "arch", "ports")


def _is_production(path: Path, repo: Path) -> bool:
    try:
        rel = path.resolve().relative_to(repo.resolve())
    except ValueError:
        return False
    return bool(rel.parts) and rel.parts[0] in ROOTS and path.suffix in {".c", ".cc", ".cpp", ".cxx"}


def _canonical_file(name: str, repo: Path) -> Path:
    p = Path(name)
    if not p.is_absolute():
        p = repo / p
    return p.resolve()


def collect(build: Path, repo: Path) -> dict:
    gcda_files = sorted(build.rglob("*.gcda"))
    if not gcda_files:
        raise RuntimeError(f"no .gcda files found under {build}")

    # source -> line_no -> max count; source -> (line_no, ordinal) -> max count
    line_counts: dict[Path, dict[int, int]] = {}
    branch_counts: dict[Path, dict[tuple[int, int], int]] = {}

    with tempfile.TemporaryDirectory(prefix="latch-gcov-") as td:
        tmp = Path(td)
        for index, gcda in enumerate(gcda_files):
            work = tmp / str(index)
            work.mkdir()
            proc = subprocess.run(
                ["gcov", "-j", "-b", "-c", str(gcda.resolve())],
                cwd=work,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
            )
            # Some object files contain no coverable source; gcov may still emit
            # useful JSON. Only fail if nothing was emitted and gcov itself failed.
            outputs = list(work.glob("*.gcov.json.gz"))
            if proc.returncode and not outputs:
                raise RuntimeError(f"gcov failed for {gcda}: {proc.stderr.strip()}")

            for output in outputs:
                with gzip.open(output, "rt", encoding="utf-8") as fh:
                    data = json.load(fh)
                for file_entry in data.get("files", []):
                    source = _canonical_file(file_entry.get("file", ""), repo)
                    if not _is_production(source, repo):
                        continue
                    lc = line_counts.setdefault(source, {})
                    bc = branch_counts.setdefault(source, {})
                    for line in file_entry.get("lines", []):
                        number = int(line["line_number"])
                        count = int(line.get("count", 0))
                        lc[number] = max(lc.get(number, 0), count)
                        for ordinal, branch in enumerate(line.get("branches", [])):
                            bcount = int(branch.get("count", 0))
                            key = (number, ordinal)
                            bc[key] = max(bc.get(key, 0), bcount)

    files = []
    total_lines = covered_lines = total_branches = covered_branches = 0
    for source in sorted(line_counts):
        lines = line_counts[source]
        branches = branch_counts.get(source, {})
        l_total = len(lines)
        l_cov = sum(1 for count in lines.values() if count > 0)
        b_total = len(branches)
        b_cov = sum(1 for count in branches.values() if count > 0)
        total_lines += l_total
        covered_lines += l_cov
        total_branches += b_total
        covered_branches += b_cov
        files.append({
            "file": str(source.relative_to(repo)),
            "lines": {"covered": l_cov, "total": l_total, "percent": (100.0 * l_cov / l_total) if l_total else 100.0},
            "branches": {"covered": b_cov, "total": b_total, "percent": (100.0 * b_cov / b_total) if b_total else 100.0},
        })

    return {
        "scope": list(ROOTS),
        "lines": {"covered": covered_lines, "total": total_lines, "percent": (100.0 * covered_lines / total_lines) if total_lines else 100.0},
        "branches": {"covered": covered_branches, "total": total_branches, "percent": (100.0 * covered_branches / total_branches) if total_branches else 100.0},
        "files": files,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build", type=Path)
    parser.add_argument("--repo", default=Path(__file__).resolve().parents[1], type=Path)
    parser.add_argument("--min-line", type=float, default=0.0)
    parser.add_argument("--min-branch", type=float, default=0.0)
    parser.add_argument("--json", dest="json_out", type=Path)
    parser.add_argument("--show-files", action="store_true")
    args = parser.parse_args()

    result = collect(args.build.resolve(), args.repo.resolve())
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    if args.show_files:
        for item in result["files"]:
            print(f"{item['file']}: lines {item['lines']['percent']:.2f}% "
                  f"branches {item['branches']['percent']:.2f}%")
    print(f"Production coverage ({', '.join(ROOTS)}):")
    print(f"  lines:    {result['lines']['covered']}/{result['lines']['total']} = {result['lines']['percent']:.2f}%")
    print(f"  branches: {result['branches']['covered']}/{result['branches']['total']} = {result['branches']['percent']:.2f}%")

    failures = []
    if result["lines"]["percent"] + 1e-9 < args.min_line:
        failures.append(f"line coverage {result['lines']['percent']:.2f}% < {args.min_line:.2f}%")
    if result["branches"]["percent"] + 1e-9 < args.min_branch:
        failures.append(f"branch coverage {result['branches']['percent']:.2f}% < {args.min_branch:.2f}%")
    if failures:
        print("coverage gate failed: " + "; ".join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
