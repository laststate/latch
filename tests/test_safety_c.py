# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tests/test_safety_c.py
#
# Safety-C checklist tests. MISRA, CERT, coding standard rules.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
from __future__ import annotations
import subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main() -> int:
    return subprocess.call([sys.executable, str(ROOT / "tools" / "check_safety_c.py"), "--root", str(ROOT)])

if __name__ == "__main__":
    raise SystemExit(main())
