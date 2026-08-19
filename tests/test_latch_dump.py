# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tests/test_latch_dump.py
#
# Dump tool tests. Record parsing and pretty-print.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Exercise the host decoder's bounded hexadecimal input mode."""

from __future__ import annotations

import json
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path


def run(decoder: str, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [decoder, *arguments],
        check=False,
        capture_output=True,
        text=True,
    )


def encrypted_envelope() -> bytes:
    prefix = b"LSTP" + bytes((1, 1, 1, 0x07)) + struct.pack("<III", 7, 9, 0)
    header = prefix + struct.pack("<I", zlib.crc32(prefix) & 0xFFFFFFFF)
    metadata = bytes(28)
    return header + metadata + struct.pack("<I", zlib.crc32(metadata) & 0xFFFFFFFF) + bytes(16)


def main() -> int:
    decoder, vector = sys.argv[1:]
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)

        valid = root / "valid.hex"
        vector_bytes = "".join(Path(vector).read_text().split())
        separators = (" ", "\t", "\r", "\n", "\f", "\v")
        pairs = [vector_bytes[index : index + 2] for index in range(0, len(vector_bytes), 2)]
        valid.write_text(
            "".join(pair + separators[index % len(separators)] for index, pair in enumerate(pairs))
        )
        result = run(decoder, "--hex", str(valid))
        assert result.returncode == 0, result.stderr
        assert "LEP v1" in result.stdout

        binary = root / "valid.lst"
        binary.write_bytes(bytes.fromhex(vector_bytes))
        result = run(decoder, str(binary))
        assert result.returncode == 0, result.stderr
        assert "LEP v1" in result.stdout
        assert "type=2 (error)" in result.stdout
        assert "tlv type=1 (identity) length=2 value_hex=aabb" in result.stdout

        result = run(decoder, "--json", "--hex", str(valid))
        assert result.returncode == 0, result.stderr
        decoded = json.loads(result.stdout)
        assert decoded["version"] == 1
        assert decoded["event_type_name"] == "error"
        assert decoded["architecture_name"] == "unknown"
        assert decoded["sequence"] == 7
        assert decoded["event_id"] == 9
        assert decoded["event_id_hex"] == "00000009"
        assert decoded["payload_encoding"] == "tlv"
        assert decoded["integrity"] == "not_present"
        assert decoded["tlvs"] == [
            {"type": 1, "name": "identity", "length": 2, "value_hex": "aabb"}
        ]

        stdin = subprocess.run(
            [decoder, "--json", "-"],
            input=binary.read_bytes(),
            check=False,
            capture_output=True,
        )
        assert stdin.returncode == 0, stdin.stderr.decode()
        assert json.loads(stdin.stdout)["event_id"] == 9

        stdin_hex = subprocess.run(
            [decoder, "--hex", "--json", "-"],
            input=valid.read_bytes(),
            check=False,
            capture_output=True,
        )
        assert stdin_hex.returncode == 0, stdin_hex.stderr.decode()
        assert json.loads(stdin_hex.stdout)["tlvs"][0]["name"] == "identity"

        encrypted = root / "encrypted.lep"
        encrypted.write_bytes(encrypted_envelope())
        result = run(decoder, "--json", str(encrypted))
        assert result.returncode == 0, result.stderr
        decoded = json.loads(result.stdout)
        assert decoded["payload_encoding"] == "encrypted"
        assert decoded["integrity"] == "not_verified"
        assert "tlvs" not in decoded

        result = run(decoder, str(encrypted))
        assert result.returncode == 0, result.stderr
        assert "payload: encrypted" in result.stdout

        result = run(decoder, "--help")
        assert result.returncode == 0, result.stderr
        assert "standard input" in result.stdout

        result = run(decoder, "--not-an-option")
        assert result.returncode == 2
        assert "unknown option" in result.stderr

        result = run(decoder, "--hex")
        assert result.returncode == 2
        assert "usage:" in result.stderr

        odd = root / "odd.hex"
        odd.write_text("abc")
        result = run(decoder, "--hex", str(odd))
        assert result.returncode != 0
        assert "odd number of digits" in result.stderr

        invalid = root / "invalid.hex"
        invalid.write_text("00xz")
        result = run(decoder, "--hex", str(invalid))
        assert result.returncode != 0
        assert "non-hex character" in result.stderr

        oversized = root / "oversized.hex"
        oversized.write_text("00" * 4097)
        result = run(decoder, "--hex", str(oversized))
        assert result.returncode != 0
        assert "exceeds LS_MAX_EVENT_SIZE" in result.stderr

        oversized_binary = root / "oversized.lep"
        oversized_binary.write_bytes(bytes(4097))
        result = run(decoder, str(oversized_binary))
        assert result.returncode != 0
        assert "exceeds LS_MAX_EVENT_SIZE" in result.stderr

        corrupt = root / "corrupt.lst"
        corrupt.write_bytes(binary.read_bytes()[:-1] + b"\x00")
        result = run(decoder, "--json", str(corrupt))
        assert result.returncode != 0
        assert result.stdout == ""

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
