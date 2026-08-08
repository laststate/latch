#!/usr/bin/env python3
"""Create a deterministic support bundle with hashes and conservative secret filtering."""
from __future__ import annotations
import argparse, hashlib, json, re, zipfile
from pathlib import Path

SENSITIVE_SUFFIXES = {".key", ".pem", ".p12", ".pfx", ".jks", ".keystore"}
SENSITIVE_BASENAMES = {".env", ".env.local", ".env.production", "credentials.json"}
SENSITIVE_WORDS = re.compile(r"(^|[._-])(secret|private|credential|token|password|passwd)([._-]|$)", re.I)


def is_sensitive(path: Path) -> bool:
    name = path.name
    return (path.suffix.lower() in SENSITIVE_SUFFIXES or name.lower() in SENSITIVE_BASENAMES or
            bool(SENSITIVE_WORDS.search(name)))


def build_bundle(source: Path, output: Path, device_id: str | None = None,
                 include_sensitive: bool = False) -> dict[str, object]:
    all_files = sorted(path for path in source.rglob("*") if path.is_file() and path != output)
    files: list[Path] = []
    excluded: list[str] = []
    for path in all_files:
        relative = path.relative_to(source).as_posix()
        if not include_sensitive and is_sensitive(path):
            excluded.append(relative)
        else:
            files.append(path)
    manifest: list[dict[str, object]] = []
    for path in files:
        data = path.read_bytes()
        manifest.append({"path": path.relative_to(source).as_posix(), "size": len(data),
                         "sha256": hashlib.sha256(data).hexdigest()})
    metadata = {"schema": 2, "device_id": device_id, "files": manifest,
                "excluded_sensitive_files": excluded, "sensitive_files_included": include_sensitive}
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for item, path in zip(manifest, files):
            info = zipfile.ZipInfo(str(item["path"]), (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, path.read_bytes())
        info = zipfile.ZipInfo("SUPPORT_MANIFEST.json", (1980, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        archive.writestr(info, json.dumps(metadata, indent=2, sort_keys=True).encode() + b"\n")
    return metadata


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--device-id")
    parser.add_argument("--include-sensitive", action="store_true",
                        help="explicitly include files whose names look like secrets/keys")
    args = parser.parse_args()
    build_bundle(args.input, args.output, args.device_id, args.include_sensitive)
    return 0
if __name__ == "__main__":
    raise SystemExit(main())
