#!/usr/bin/env python3
"""Run clang-tidy over every production translation unit in compile_commands."""
from __future__ import annotations
import argparse, json, subprocess
from pathlib import Path

ROOTS = {"src", "arch", "ports"}

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", type=Path, default=Path("build"))
    ap.add_argument("--warnings-as-errors", default="clang-analyzer-*")
    args = ap.parse_args()
    repo = Path(__file__).resolve().parents[1]
    db = json.loads((args.build / "compile_commands.json").read_text(encoding="utf-8"))
    files = set()
    for entry in db:
        p = Path(entry["file"])
        if not p.is_absolute():
            p = Path(entry.get("directory", repo)) / p
        try:
            rel = p.resolve().relative_to(repo.resolve())
        except ValueError:
            continue
        if rel.parts and rel.parts[0] in ROOTS and p.suffix in {".c", ".cc", ".cpp", ".cxx"}:
            files.add(p.resolve())
    if not files:
        raise SystemExit("no production translation units found in compile_commands.json")
    for path in sorted(files):
        subprocess.run([
            "clang-tidy", str(path), "-p", str(args.build),
            f"--warnings-as-errors={args.warnings_as_errors}"
        ], check=True)
    print(f"clang-tidy checked {len(files)} production translation units")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
