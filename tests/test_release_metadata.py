# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tests/test_release_metadata.py
#
# Release metadata tests. SBOM, version, changelog integrity.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Focused tests for the release metadata gate."""

from __future__ import annotations

import pathlib
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from check_release_metadata import collect_versions, validate  # noqa: E402


def write_tree(root: pathlib.Path, version: str = "0.3.0") -> None:
    (root / "rust" / "latch").mkdir(parents=True)
    (root / "CMakeLists.txt").write_text(
        f"project(latch VERSION {version} LANGUAGES C)\n", encoding="utf-8"
    )
    (root / "include" / "laststate").mkdir(parents=True)
    (root / "include" / "laststate" / "version.h").write_text(
        f'#define LS_VERSION_STRING "{version}"\n', encoding="utf-8"
    )
    (root / "library.json").write_text(f'{{"version": "{version}"}}\n', encoding="utf-8")
    (root / "library.properties").write_text(
        f"name=Latch\nversion={version}\n", encoding="utf-8"
    )
    (root / "idf_component.yml").write_text(
        f'name: "latch"\nversion: "{version}"\n', encoding="utf-8"
    )
    (root / "rust" / "latch" / "Cargo.toml").write_text(
        f'[package]\nname = "laststate-latch"\nversion = "{version}"\n', encoding="utf-8"
    )
    (root / "CHANGELOG.md").write_text(
        f"# Changelog\n\n## [{version}] - 2026-08-01\n", encoding="utf-8"
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        write_tree(root)
        assert collect_versions(root) == {
            "CMake": "0.3.0",
            "C API": "0.3.0",
            "PlatformIO": "0.3.0",
            "Arduino": "0.3.0",
            "ESP-IDF component": "0.3.0",
            "Rust crate": "0.3.0",
        }
        assert validate(root, "0.3.0", False) == []

        (root / "library.json").write_text('{"version": "0.3.1"}\n', encoding="utf-8")
        errors = validate(root, "0.3.0", False)
        assert "PlatformIO version is 0.3.1, expected 0.3.0" in errors

        (root / "library.json").write_text('{"version": "0.3.0"}\n', encoding="utf-8")
        (root / "CHANGELOG.md").write_text("# Changelog\n", encoding="utf-8")
        assert "CHANGELOG.md has no dated [0.3.0] release section" in validate(root, "0.3.0", False)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
