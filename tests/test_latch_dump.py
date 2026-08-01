#!/usr/bin/env python3
"""Exercise the host decoder's bounded hexadecimal input mode."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import json
from pathlib import Path


def run(decoder: str, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [decoder, *arguments],
        check=False,
        capture_output=True,
        text=True,
    )


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

        result = run(decoder, "--json", "--hex", str(valid))
        assert result.returncode == 0, result.stderr
        decoded = json.loads(result.stdout)
        assert decoded["version"] == 1
        assert decoded["sequence"] == 7
        assert decoded["event_id"] == 9
        assert decoded["event_id_hex"] == "00000009"
        assert decoded["tlvs"] == [{"type": 1, "length": 2, "value_hex": "aabb"}]

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

        corrupt = root / "corrupt.lst"
        corrupt.write_bytes(binary.read_bytes()[:-1] + b"\x00")
        result = run(decoder, "--json", str(corrupt))
        assert result.returncode != 0
        assert result.stdout == ""

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
