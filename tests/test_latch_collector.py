#!/usr/bin/env python3
from __future__ import annotations

import io
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from latch_collector import (  # noqa: E402
    DurableStore,
    LSAK_DUPLICATE,
    LSAK_STORED,
    ProtocolError,
    StreamParser,
    collect,
    crc32,
    validate_envelope,
)


def framed(envelope: bytes) -> bytes:
    return b"LS\x01\x00" + struct.pack("<I", len(envelope)) + envelope + struct.pack("<I", crc32(envelope))


def main() -> int:
    vector = bytes.fromhex(Path(sys.argv[1]).read_text(encoding="utf-8"))
    info = validate_envelope(vector)
    assert (info.sequence, info.event_id) == (7, 9)

    parser = StreamParser()
    output: list[bytes] = []
    stream = b"garbage" + framed(vector)
    for byte in stream:
        output.extend(parser.feed(bytes((byte,))))
    assert output == [vector]

    corrupt = bytearray(framed(vector))
    corrupt[-1] ^= 1
    assert StreamParser().feed(corrupt) == []
    assert StreamParser(64).feed(b"LS\x01\x00" + struct.pack("<I", 65)) == []

    bad = bytearray(vector)
    bad[-1] ^= 1
    try:
        validate_envelope(bytes(bad))
    except ProtocolError:
        pass
    else:
        raise AssertionError("bad payload CRC accepted")

    with tempfile.TemporaryDirectory() as directory:
        store = DurableStore(Path(directory))
        ack = io.BytesIO()
        assert collect([framed(vector)], ack, store) == 1
        assert (Path(directory) / "00000009.lep").read_bytes() == vector
        assert ack.getvalue()[5] == LSAK_STORED
        assert struct.unpack_from("<I", ack.getvalue(), 8)[0] == 9

        duplicate = io.BytesIO()
        assert collect([framed(vector)], duplicate, store) == 1
        assert duplicate.getvalue()[5] == LSAK_DUPLICATE
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
