# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/mutation_smoke.py
#
# Mutation testing smoke. Quick sanity on mutant survival rate.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Small deterministic mutation suite for safety-critical decision logic.

This is intentionally not a replacement for a full mutation framework. It
keeps a few high-value mutants in CI so tests must prove they can detect
regressions in HTTP success classification, varint overflow rejection and the
critical spool reservation policy.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import subprocess

@dataclass(frozen=True)
class Mutation:
    name: str
    file: str
    before: str
    after: str
    target: str
    executable: str

MUTATIONS = (
    Mutation(
        "http-success-and-to-or",
        "src/transport/network.c",
        "status >= 200u && status < 300u",
        "status >= 200u || status < 300u",
        "latch-defensive-paths-tests",
        "latch-defensive-paths-tests",
    ),
    Mutation(
        "varint-overflow-check-disabled",
        "src/envelope/compression.c",
        "i == 4 && (byte & 0xf0u)",
        "i == 4 && (byte & 0x00u)",
        "latch-defensive-paths-tests",
        "latch-defensive-paths-tests",
    ),
    Mutation(
        "emergency-spool-reservation-bypassed",
        "src/spool/spool.c",
        "return critical_slot_limit();",
        "return LS_SPOOL_MAX_RECORDS;",
        "latch-spool-priority-tests",
        "latch-spool-priority-tests",
    ),
)


def run(cmd: list[str], cwd: Path, *, expect_failure: bool = False) -> None:
    proc = subprocess.run(cmd, cwd=cwd)
    if expect_failure:
        if proc.returncode == 0:
            raise RuntimeError(f"mutant survived: {' '.join(cmd)}")
    elif proc.returncode != 0:
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(cmd)}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", type=Path, default=Path("build/mutation"))
    args = ap.parse_args()
    repo = Path(__file__).resolve().parents[1]
    build = args.build.resolve()

    if not (build / "CMakeCache.txt").exists():
        run([
            "cmake", "-S", str(repo), "-B", str(build), "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Debug", "-DLS_BUILD_TESTS=ON", "-DLS_BUILD_LINUX=ON",
        ], repo)

    for mutant in MUTATIONS:
        path = repo / mutant.file
        original = path.read_text(encoding="utf-8")
        occurrences = original.count(mutant.before)
        if occurrences != 1:
            raise RuntimeError(f"{mutant.name}: expected one mutation site, found {occurrences}")
        try:
            path.write_text(original.replace(mutant.before, mutant.after, 1), encoding="utf-8")
            run(["cmake", "--build", str(build), "--target", mutant.target, "--parallel"], repo)
            print(f"[mutation] expecting test failure: {mutant.name}")
            run([str(build / mutant.executable)], repo, expect_failure=True)
            print(f"[mutation] killed: {mutant.name}")
        finally:
            path.write_text(original, encoding="utf-8")
            # Restore a clean object/executable before the next mutant.
            run(["cmake", "--build", str(build), "--target", mutant.target, "--parallel"], repo)

    print(f"mutation smoke: {len(MUTATIONS)}/{len(MUTATIONS)} mutants killed")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
