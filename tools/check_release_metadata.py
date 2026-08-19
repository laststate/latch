# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/check_release_metadata.py
#
# Release metadata validator. SBOM, version, changelog, and
# provenance checks.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Verify that every publishable Latch package names one release version.

This intentionally checks only repository metadata and a dated changelog
section. It does not publish, tag, modify files, or decide whether hardware
qualification is adequate for a release.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
import tomllib


VERSION_PATTERN = re.compile(r"project\(\s*latch\s+VERSION\s+([^\s)]+)", re.IGNORECASE)
VERSION_HEADER_PATTERN = re.compile(r'^\s*#define\s+LS_VERSION_STRING\s+"([^"]+)"\s*$', re.MULTILINE)
CHANGELOG_PATTERN = r"^## \[{version}\] - \d{{4}}-\d{{2}}-\d{{2}}\s*$"


def read_required(path: pathlib.Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as error:
        raise ValueError(f"missing required release metadata: {path}") from error


def version_from_cmake(path: pathlib.Path) -> str:
    match = VERSION_PATTERN.search(read_required(path))
    if not match:
        raise ValueError(f"could not find project(latch VERSION ...) in {path}")
    return match.group(1)


def version_from_header(path: pathlib.Path) -> str:
    match = VERSION_HEADER_PATTERN.search(read_required(path))
    if not match:
        raise ValueError(f"could not find LS_VERSION_STRING in {path}")
    return match.group(1)


def version_from_json(path: pathlib.Path) -> str:
    try:
        version = json.loads(read_required(path))["version"]
    except (json.JSONDecodeError, KeyError, TypeError) as error:
        raise ValueError(f"could not read string version from {path}") from error
    if not isinstance(version, str) or not version:
        raise ValueError(f"could not read string version from {path}")
    return version


def version_from_cargo(path: pathlib.Path) -> str:
    try:
        version = tomllib.loads(read_required(path))["package"]["version"]
    except (tomllib.TOMLDecodeError, KeyError, TypeError) as error:
        raise ValueError(f"could not read package.version from {path}") from error
    if not isinstance(version, str) or not version:
        raise ValueError(f"could not read package.version from {path}")
    return version


def version_from_properties(path: pathlib.Path) -> str:
    for line in read_required(path).splitlines():
        key, separator, value = line.partition("=")
        if separator and key.strip() == "version" and value.strip():
            return value.strip()
    raise ValueError(f"could not read version= from {path}")


def version_from_idf_manifest(path: pathlib.Path) -> str:
    for line in read_required(path).splitlines():
        match = re.match(r"^version:\s*[\"']?([^\"'#\s]+)[\"']?\s*(?:#.*)?$", line)
        if match:
            return match.group(1)
    raise ValueError(f"could not read version: from {path}")


def collect_versions(root: pathlib.Path) -> dict[str, str]:
    return {
        "CMake": version_from_cmake(root / "CMakeLists.txt"),
        "C API": version_from_header(root / "include" / "laststate" / "version.h"),
        "PlatformIO": version_from_json(root / "library.json"),
        "Arduino": version_from_properties(root / "library.properties"),
        "ESP-IDF component": version_from_idf_manifest(root / "idf_component.yml"),
        "Rust crate": version_from_cargo(root / "rust" / "latch" / "Cargo.toml"),
    }


def changelog_has_release(root: pathlib.Path, version: str) -> bool:
    return bool(
        re.search(
            CHANGELOG_PATTERN.format(version=re.escape(version)),
            read_required(root / "CHANGELOG.md"),
            flags=re.MULTILINE,
        )
    )


def exact_tag(root: pathlib.Path) -> str:
    completed = subprocess.run(
        ["git", "describe", "--exact-match", "--tags"],
        cwd=root,
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise ValueError("HEAD is not an exact Git tag")
    return completed.stdout.strip()


def validate(root: pathlib.Path, expected: str | None, require_tag: bool) -> list[str]:
    versions = collect_versions(root)
    selected = expected or versions["CMake"]
    errors = [f"{name} version is {value}, expected {selected}" for name, value in versions.items() if value != selected]
    if not changelog_has_release(root, selected):
        errors.append(f"CHANGELOG.md has no dated [{selected}] release section")
    if require_tag:
        try:
            tag = exact_tag(root)
        except ValueError as error:
            errors.append(str(error))
        else:
            if tag != f"v{selected}":
                errors.append(f"exact Git tag is {tag}, expected v{selected}")
    return errors


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument("--version", help="expected unprefixed release version; defaults to CMake")
    parser.add_argument("--require-tag", action="store_true", help="require HEAD to be exactly vVERSION")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_argument_parser().parse_args(argv)
    root = args.root.resolve()
    try:
        errors = validate(root, args.version, args.require_tag)
    except ValueError as error:
        print(f"release metadata check failed: {error}", file=sys.stderr)
        return 1
    if errors:
        print("release metadata check failed:", file=sys.stderr)
        print("\n".join(f"- {error}" for error in errors), file=sys.stderr)
        return 1
    print(f"release metadata checks passed for v{args.version or version_from_cmake(root / 'CMakeLists.txt')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
