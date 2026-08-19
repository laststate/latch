# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tests/test_latch_collector.py
#
# Collector tests. Device enrollment and record ingestion.
#
# Heap-free, bounded, deterministic.

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
    FleetIndex,
    LSAK_DUPLICATE,
    LSAK_STORED,
    ProtocolError,
    StreamParser,
    collect,
    crc32,
    extract_metadata,
    validate_envelope,
)



def tlv(kind: int, value: bytes) -> bytes:
    return struct.pack("<HH", kind, len(value)) + value


def synthetic_envelope() -> bytes:
    identity_values = [b"proj", b"auv-07", b"AUV", b"A", b"BOM1", b"batch",
                       b"0.3.0", b"build-1234", b"boot", b"deadbeef", b"canary",
                       b"host", b"rtos", b"atlantic", b"fleet-a"]
    identity = b"".join(bytes((index + 1, len(value))) + value
                        for index, value in enumerate(identity_values))
    event = bytes((0, 4, 1)) + struct.pack("<IIIIIII", 1, 2, 3, 0x12345678, 1, 10, 20)
    mission_prefix = (bytes((1,)) + struct.pack("<IIIIIIIQQB", 11, 12, 13, 14, 2, 12345, 99,
                                                0x1122334455667788, 0x99AABBCCDDEEFF00, 1))
    mission_strings = b"".join((bytes((1, 4)) + b"m-01", bytes((2, 4)) + b"d-01",
                                bytes((3, 3)) + b"nav", bytes((4, 6)) + b"survey"))
    payload = tlv(1, identity) + tlv(3, event) + tlv(18, mission_prefix + mission_strings)
    header = bytearray(b"LSTP" + bytes((1, 2, 0, 0)) + struct.pack("<III", 100, 0x77889900, len(payload)))
    header.extend(struct.pack("<I", crc32(header)))
    return bytes(header) + payload + struct.pack("<I", crc32(payload))

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
        assert (Path(directory) / "unknown" / "00000009.lep").read_bytes() == vector
        assert ack.getvalue()[5] == LSAK_STORED
        assert struct.unpack_from("<I", ack.getvalue(), 8)[0] == 9

        duplicate = io.BytesIO()
        assert collect([framed(vector)], duplicate, store) == 1
        assert duplicate.getvalue()[5] == LSAK_DUPLICATE

        fleet_store = DurableStore(Path(directory), "auv-07")
        fleet_ack = io.BytesIO()
        assert collect([framed(vector)], fleet_ack, fleet_store) == 1
        assert (Path(directory) / "auv-07" / "00000009.lep").read_bytes() == vector
        try:
            DurableStore(Path(directory), "../escape")
        except ValueError:
            pass
        else:
            raise AssertionError("unsafe device namespace accepted")

    synthetic = synthetic_envelope()
    metadata = extract_metadata(synthetic)
    assert metadata.device_id == "auv-07"
    assert metadata.build_id == "build-1234"
    assert metadata.fingerprint == 0x12345678
    assert metadata.mission_id == "m-01"
    assert metadata.dive_id == "d-01"
    assert metadata.incident_id == "112233445566778899aabbccddeeff00"
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        index = FleetIndex(root / "fleet.sqlite", strict_replay=True)
        store = DurableStore(root / "events", index=index)
        info = validate_envelope(synthetic)
        assert store.persist(synthetic, info) == LSAK_STORED
        assert (root / "events" / "auv-07" / f"{info.event_id:08x}.lep").read_bytes() == synthetic
        assert store.persist(synthetic, info) == LSAK_DUPLICATE
        clusters = index.crash_clusters()
        assert len(clusters) == 1 and clusters[0]["fingerprint"] == 0x12345678
        row = index.db.execute("SELECT highest_sequence,event_count,last_build_id FROM devices WHERE device_id='auv-07'").fetchone()
        assert row == (100, 1, "build-1234")
        index.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
