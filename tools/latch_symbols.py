#!/usr/bin/env python3
"""Create and query a compact symbol manifest for exact Latch firmware builds."""
from __future__ import annotations
import argparse, json, subprocess
from bisect import bisect_right
from pathlib import Path


def build_manifest(elf: Path, nm: str = "nm", build_id: str | None = None) -> dict[str, object]:
    command = [nm, "-n", "-S", "--defined-only", str(elf)]
    proc = subprocess.run(command, check=True, capture_output=True, text=True)
    symbols: list[dict[str, object]] = []
    for line in proc.stdout.splitlines():
        parts = line.split(maxsplit=3)
        if len(parts) < 4:
            continue
        try:
            address, size = int(parts[0], 16), int(parts[1], 16)
        except ValueError:
            continue
        kind, name = parts[2], parts[3]
        if kind.lower() not in {"t", "w"}:
            continue
        symbols.append({"address": address, "size": size, "name": name})
    return {"schema": 1, "elf": elf.name, "build_id": build_id, "symbols": symbols}


def lookup(manifest: dict[str, object], address: int) -> dict[str, object] | None:
    symbols = list(manifest.get("symbols", []))
    addresses = [int(item["address"]) for item in symbols]
    index = bisect_right(addresses, address) - 1
    if index < 0:
        return None
    item = dict(symbols[index])
    start, size = int(item["address"]), int(item["size"])
    if size and address >= start + size:
        return None
    item["offset"] = address - start
    return item


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    build = sub.add_parser("build")
    build.add_argument("--elf", type=Path, required=True)
    build.add_argument("--output", type=Path, required=True)
    build.add_argument("--nm", default="nm")
    build.add_argument("--build-id")
    query = sub.add_parser("lookup")
    query.add_argument("--manifest", type=Path, required=True)
    query.add_argument("addresses", nargs="+")
    args = parser.parse_args()
    if args.command == "build":
        args.output.write_text(json.dumps(build_manifest(args.elf, args.nm, args.build_id), indent=2) + "\n",
                               encoding="utf-8")
        return 0
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    result = {text: lookup(manifest, int(text, 0)) for text in args.addresses}
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0
if __name__ == "__main__":
    raise SystemExit(main())
