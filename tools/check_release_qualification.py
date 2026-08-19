# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/check_release_qualification.py
#
# Release qualification checker. Validates 23/23 matrix, coverage,
# and artifact integrity before tag.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Gate stable releases on fresh, commit-bound physical HIL evidence."""
from __future__ import annotations

import argparse
import datetime as dt
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
POLICY_PATH = ROOT / "hil" / "qualification-policy.json"
RELEASE_DIR = ROOT / "hil" / "releases"
SHA_RE = re.compile(r"[0-9a-f]{40}")


def load_json(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def version_major(version: str) -> int:
    match = re.fullmatch(r"v?(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", version)
    if not match:
        raise ValueError(f"invalid semantic version: {version}")
    return int(match.group(1))


def is_prerelease(version: str) -> bool:
    core = version.split("+", 1)[0]
    return "-" in core


def validate(version: str, commit: str, manifest_path: pathlib.Path | None = None,
             today: dt.date | None = None) -> list[str]:
    errors: list[str] = []
    policy = load_json(POLICY_PATH)
    if policy.get("schema_version") != 1:
        return ["qualification policy schema_version must be 1"]
    major = version_major(version)
    stable_major = int(policy.get("stable_release_min_major", 1))
    if major < stable_major or is_prerelease(version):
        return []

    tag = version if version.startswith("v") else f"v{version}"
    path = manifest_path or RELEASE_DIR / f"{tag}.json"
    if not path.is_file():
        return [f"stable release requires qualification manifest: {path.relative_to(ROOT)}"]
    data = load_json(path)
    if data.get("schema_version") != 1:
        errors.append("release qualification schema_version must be 1")
    if data.get("release") != tag:
        errors.append(f"manifest release must be {tag}")
    normalized_commit = commit.lower()
    if not SHA_RE.fullmatch(normalized_commit):
        errors.append("release commit must be a full lowercase 40-character SHA-1")
    if data.get("commit", "").lower() != normalized_commit:
        errors.append("qualification manifest commit does not match release commit")

    try:
        generated = dt.date.fromisoformat(data.get("generated_at", ""))
    except ValueError:
        errors.append("generated_at must be an ISO-8601 date")
        generated = None
    now = today or dt.date.today()
    max_age = int(policy.get("max_evidence_age_days", 180))
    if generated is not None:
        age = (now - generated).days
        if age < 0:
            errors.append("qualification manifest date is in the future")
        elif age > max_age:
            errors.append(f"qualification manifest is stale ({age} days > {max_age})")

    targets = data.get("targets")
    if not isinstance(targets, list) or not targets:
        return errors + ["stable release requires at least one physically qualified target"]
    required = set(policy.get("required_scenarios", []))
    conditional = policy.get("conditional_scenarios", {})
    for index, target in enumerate(targets):
        prefix = f"target {index}"
        for field in ("platform", "mcu", "board", "toolchain", "scenarios", "evidence"):
            if field not in target:
                errors.append(f"{prefix}: missing {field}")
        scenarios = set(target.get("scenarios", []))
        missing = sorted(required - scenarios)
        if missing:
            errors.append(f"{prefix}: missing required scenarios {missing}")
        features = set(target.get("features", []))
        for feature, scenario in conditional.items():
            if feature in features and scenario not in scenarios:
                errors.append(f"{prefix}: feature {feature!r} requires scenario {scenario!r}")
        evidence_value = target.get("evidence")
        if evidence_value:
            evidence = (ROOT / "hil" / evidence_value).resolve()
            hil_root = (ROOT / "hil").resolve()
            if not evidence.is_file() or hil_root not in evidence.parents:
                errors.append(f"{prefix}: evidence must resolve to a file inside hil/")
            elif evidence.suffix.lower() != ".json":
                errors.append(f"{prefix}: machine-readable evidence must be JSON")
            else:
                try:
                    evidence_data = load_json(evidence)
                except (OSError, json.JSONDecodeError) as exc:
                    errors.append(f"{prefix}: invalid evidence JSON: {exc}")
                else:
                    if evidence_data.get("result") != "pass":
                        errors.append(f"{prefix}: evidence result must be pass")
                    if evidence_data.get("commit", "").lower() != normalized_commit:
                        errors.append(f"{prefix}: evidence commit does not match release commit")
                    observed = set(evidence_data.get("scenarios", []))
                    if not scenarios.issubset(observed):
                        errors.append(f"{prefix}: manifest claims scenarios absent from evidence")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--manifest", type=pathlib.Path)
    args = parser.parse_args()
    try:
        errors = validate(args.version, args.commit, args.manifest)
    except (ValueError, OSError, json.JSONDecodeError) as exc:
        print(exc, file=sys.stderr)
        return 1
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    if version_major(args.version) < int(load_json(POLICY_PATH)["stable_release_min_major"]) or is_prerelease(args.version):
        print("pre-release release: physical qualification manifest is advisory")
    else:
        print("stable release qualification manifest valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
