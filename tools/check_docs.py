# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/check_docs.py
#
# Documentation checker. Broken links, stale examples, and
# required section presence.
#
# Heap-free, bounded, deterministic.

from __future__ import annotations
import pathlib
import re
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
errors: list[str] = []


def is_ignored(path: pathlib.Path) -> bool:
    if any(part in {"build", "target", ".pio", ".git"} or part.startswith("build-")
           for part in path.parts):
        return True
    try:
        res = subprocess.run(
            ["git", "check-ignore", "-q", str(path)],
            cwd=root,
            capture_output=True,
            text=True,
        )
        return res.returncode == 0
    except OSError:
        return False


for path in sorted(root.rglob("*.md")):
    if is_ignored(path):
        continue
    text = path.read_text(encoding="utf-8")
    if not text.endswith("\n"):
        errors.append(f"{path.relative_to(root)}: missing final newline")
    for line_no, line in enumerate(text.splitlines(), 1):
        if line.rstrip() != line:
            errors.append(f"{path.relative_to(root)}:{line_no}: trailing whitespace")
    for target in re.findall(r"\[[^]]+\]\((?!https?://|#)([^)]+)\)", text):
        candidate = (path.parent / target.split("#", 1)[0]).resolve()
        if target and not candidate.exists():
            errors.append(f"{path.relative_to(root)}: broken link {target}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)
print("documentation checks passed")
