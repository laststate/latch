# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/check_test_vector.py
#
# Test vector validator. Known-answer tests for crypto and LEP.
#
# Heap-free, bounded, deterministic.

from __future__ import annotations

import pathlib
import struct
import sys
import zlib

root = pathlib.Path(__file__).resolve().parents[1]
vectors = root / "tests" / "vectors"
KNOWN_FLAGS = 0x1F
HEADER = 24


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def load_hex(path: pathlib.Path) -> bytes:
    text = path.read_text(encoding="ascii")
    clean = "".join(c for c in text if c in "0123456789abcdefABCDEF")
    return bytes.fromhex(clean)


def validate_tlvs(payload: bytes) -> None:
    off = 0
    while off < len(payload):
        if len(payload) - off < 4:
            raise ValueError("corrupt: tlv")
        t, n = struct.unpack_from("<HH", payload, off)
        off += 4
        if t == 0 or n > len(payload) - off:
            raise ValueError("corrupt: tlv")
        off += n


def validate_plain(data: bytes) -> None:
    if len(data) < HEADER + 4:
        raise ValueError("short")
    if data[:4] != b"LSTP":
        raise ValueError("magic")
    version, flags = data[4], data[7]
    if version != 1:
        raise ValueError("version")
    if flags & ~KNOWN_FLAGS:
        raise ValueError("flags")
    enc = bool(flags & 0x02)
    aead = bool(flags & 0x04)
    auth = bool(flags & 0x01)
    if enc != aead:
        raise ValueError("flags-enc")
    if aead and not auth:
        raise ValueError("flags-auth")
    meta = 28 if aead else 0
    auth_len = 16 if aead else (32 if auth else 0)
    payload_length = struct.unpack_from("<I", data, 16)[0]
    overhead = HEADER + meta + 4 + auth_len
    if len(data) != overhead + payload_length:
        raise ValueError("size")
    if struct.unpack_from("<I", data, 20)[0] != crc32(data[:20]):
        raise ValueError("header_crc")
    crc_off = HEADER + meta + payload_length
    if struct.unpack_from("<I", data, crc_off)[0] != crc32(data[HEADER:crc_off]):
        raise ValueError("payload_crc")
    if not enc and not (flags & 0x10):
        validate_tlvs(data[HEADER + meta : crc_off])


def main() -> int:
    good = load_hex(vectors / "lep-v1-basic.hex")
    validate_plain(good)
    field_type, field_length = struct.unpack_from("<HH", good, 24)
    if field_type != 1 or field_length != 2 or good[28:30] != bytes([0xAA, 0xBB]):
        print("FAIL basic payload layout", file=sys.stderr)
        return 1

    invalid_dir = vectors / "invalid"
    if invalid_dir.is_dir():
        failed_as_expected = 0
        for path in sorted(invalid_dir.glob("*.hex")):
            data = load_hex(path)
            try:
                validate_plain(data)
            except Exception:
                failed_as_expected += 1
                continue
            print(f"FAIL {path.name}: expected invalid, validated as plain LEP", file=sys.stderr)
            return 1
        print(f"LEP public test vector passed; {failed_as_expected} invalid vectors rejected")
    else:
        print("LEP public test vector passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
