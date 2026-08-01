#!/usr/bin/env python3
"""Extract a CPack archive and build a clean external C/C++ consumer."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--consumer", type=Path, required=True)
    parser.add_argument("--config", default="Release")
    args = parser.parse_args()
    archive = args.archive.resolve()
    if not archive.is_file():
        raise SystemExit(f"archive does not exist: {archive}")
    with tempfile.TemporaryDirectory(prefix="latch-package-") as directory:
        root = Path(directory)
        shutil.unpack_archive(archive, root / "prefix")
        prefixes = list((root / "prefix").glob("*/lib/cmake/laststate-latch"))
        if len(prefixes) != 1:
            raise SystemExit(f"expected one packaged CMake config, found {len(prefixes)}")
        prefix = prefixes[0].parents[2]
        build = root / "consumer-build"
        run(["cmake", "-S", str(args.consumer.resolve()), "-B", str(build),
             f"-DCMAKE_PREFIX_PATH={prefix}"])
        run(["cmake", "--build", str(build), "--config", args.config])
        c_program = build / ("c-consumer.exe" if shutil.which("where") else "c-consumer")
        if not c_program.exists() and (build / args.config / "c-consumer.exe").exists():
            c_program = build / args.config / "c-consumer.exe"
        run([str(c_program)])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
