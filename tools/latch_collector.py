#!/usr/bin/env python3
"""Durable reference collector and small fleet index for Latch LEP streams.

The collector intentionally stays dependency-free for file/stdin operation.  An
optional SQLite index adds persistent replay tracking, crash clustering and
firmware/mission metadata suitable for lab fleets and qualification rigs. It is
still a reference service, not a substitute for authenticated production ingest.
"""

from __future__ import annotations

import argparse
import binascii
import json
import os
import re
import sqlite3
import struct
import sys
import tempfile
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import BinaryIO, Iterable, Iterator

STREAM_MAGIC = b"LS"
STREAM_VERSION = 1
STREAM_HEADER_SIZE = 8
STREAM_TRAILER_SIZE = 4
LSAK_MAGIC = b"LSAK"
LSAK_VERSION = 1
LSAK_STORED = 1
LSAK_DUPLICATE = 2
LEP_MAGIC = b"LSTP"
LEP_HEADER_SIZE = 24
LEP_KNOWN_FLAGS = 0x0F
LEP_AUTHENTICATED = 0x01
LEP_ENCRYPTED = 0x02
LEP_AEAD = 0x04
LEP_SECURITY_METADATA_SIZE = 28
AEAD_TAG_SIZE = 16
HMAC_SIZE = 32
TLV_IDENTITY = 1
TLV_EVENT = 3
TLV_MISSION = 18


class ProtocolError(ValueError):
    """The input is bounded but malformed or unsupported."""


@dataclass(frozen=True)
class EnvelopeInfo:
    version: int
    event_type: int
    architecture: int
    flags: int
    sequence: int
    event_id: int
    payload_length: int


@dataclass(frozen=True)
class EnvelopeMetadata:
    device_id: str | None = None
    build_id: str | None = None
    firmware_version: str | None = None
    variant: str | None = None
    device_group: str | None = None
    fingerprint: int | None = None
    priority: int | None = None
    severity: int | None = None
    mission_id: str | None = None
    dive_id: str | None = None
    node_id: str | None = None
    vehicle_mode: str | None = None
    incident_id: str | None = None


def crc32(data: bytes | memoryview) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def validate_envelope(data: bytes) -> EnvelopeInfo:
    if len(data) < LEP_HEADER_SIZE + 4 or data[:4] != LEP_MAGIC:
        raise ProtocolError("invalid LEP magic or truncated header")
    version, event_type, architecture, flags = data[4:8]
    if version != 1:
        raise ProtocolError(f"unsupported LEP version {version}")
    if flags & ~LEP_KNOWN_FLAGS:
        raise ProtocolError("unknown LEP flags")
    authenticated = bool(flags & LEP_AUTHENTICATED)
    encrypted = bool(flags & LEP_ENCRYPTED)
    aead = bool(flags & LEP_AEAD)
    if encrypted != aead or (aead and not authenticated):
        raise ProtocolError("inconsistent LEP security flags")
    sequence, event_id, payload_length, header_crc = struct.unpack_from("<IIII", data, 8)
    if header_crc != crc32(data[:20]):
        raise ProtocolError("invalid LEP header CRC")
    metadata_size = LEP_SECURITY_METADATA_SIZE if encrypted else 0
    authentication_size = AEAD_TAG_SIZE if encrypted else (HMAC_SIZE if authenticated else 0)
    expected = LEP_HEADER_SIZE + metadata_size + payload_length + 4 + authentication_size
    if expected != len(data):
        raise ProtocolError("LEP length does not match its header")
    crc_offset = LEP_HEADER_SIZE + metadata_size + payload_length
    if struct.unpack_from("<I", data, crc_offset)[0] != crc32(data[LEP_HEADER_SIZE:crc_offset]):
        raise ProtocolError("invalid LEP payload CRC")
    if not encrypted:
        for _ in iter_tlvs(data, payload_length):
            pass
    return EnvelopeInfo(version, event_type, architecture, flags, sequence, event_id, payload_length)


def iter_tlvs(data: bytes, payload_length: int | None = None) -> Iterator[tuple[int, bytes]]:
    info_payload_length = payload_length
    flags = data[7] if len(data) >= 8 else 0
    if flags & LEP_ENCRYPTED:
        return
    if info_payload_length is None:
        if len(data) < LEP_HEADER_SIZE:
            raise ProtocolError("truncated LEP header")
        info_payload_length = struct.unpack_from("<I", data, 16)[0]
    payload = memoryview(data)[LEP_HEADER_SIZE : LEP_HEADER_SIZE + info_payload_length]
    offset = 0
    while offset < len(payload):
        if len(payload) - offset < 4:
            raise ProtocolError("truncated LEP TLV")
        field_type, field_length = struct.unpack_from("<HH", payload, offset)
        offset += 4
        if field_type == 0 or field_length > len(payload) - offset:
            raise ProtocolError("invalid LEP TLV")
        yield field_type, bytes(payload[offset : offset + field_length])
        offset += field_length


def _string_fields(value: bytes, fixed_prefix: int = 0) -> dict[int, str]:
    fields: dict[int, str] = {}
    offset = fixed_prefix
    while offset < len(value):
        if len(value) - offset < 2:
            break
        field, length = value[offset], value[offset + 1]
        offset += 2
        if length > len(value) - offset:
            break
        fields[field] = value[offset : offset + length].decode("utf-8", "replace")
        offset += length
    return fields


def extract_metadata(data: bytes) -> EnvelopeMetadata:
    info = validate_envelope(data)
    if info.flags & LEP_ENCRYPTED:
        return EnvelopeMetadata()
    identity: dict[int, str] = {}
    mission: dict[int, str] = {}
    fingerprint = priority = severity = None
    incident_id = None
    for field_type, value in iter_tlvs(data, info.payload_length):
        if field_type == TLV_IDENTITY:
            identity.update(_string_fields(value))
        elif field_type == TLV_EVENT and len(value) >= 31:
            priority, severity = value[0], value[1]
            fingerprint = struct.unpack_from("<I", value, 15)[0]
        elif field_type == TLV_MISSION and len(value) >= 46 and value[0] == 1:
            incident_hi = struct.unpack_from("<Q", value, 29)[0]
            incident_lo = struct.unpack_from("<Q", value, 37)[0]
            if value[45] and (incident_hi or incident_lo):
                incident_id = f"{incident_hi:016x}{incident_lo:016x}"
            mission.update(_string_fields(value, 46))
    return EnvelopeMetadata(
        device_id=identity.get(2),
        build_id=identity.get(8),
        firmware_version=identity.get(7),
        variant=identity.get(11),
        device_group=identity.get(15),
        fingerprint=fingerprint,
        priority=priority,
        severity=severity,
        mission_id=mission.get(1),
        dive_id=mission.get(2),
        node_id=mission.get(3),
        vehicle_mode=mission.get(4),
        incident_id=incident_id,
    )


def make_ack(status: int, event_id: int) -> bytes:
    return LSAK_MAGIC + bytes((LSAK_VERSION, status, 0, 0)) + struct.pack("<I", event_id)


class StreamParser:
    """Incrementally resynchronizes and returns CRC-checked LEP frames."""

    def __init__(self, maximum_envelope: int = 4096) -> None:
        if maximum_envelope < LEP_HEADER_SIZE + 4:
            raise ValueError("maximum_envelope is too small")
        self.maximum_envelope = maximum_envelope
        self.buffer = bytearray()

    def feed(self, chunk: bytes) -> list[bytes]:
        self.buffer.extend(chunk)
        frames: list[bytes] = []
        while True:
            marker = self.buffer.find(STREAM_MAGIC)
            if marker < 0:
                self.buffer[:] = self.buffer[-1:] if self.buffer.endswith(b"L") else b""
                return frames
            if marker:
                del self.buffer[:marker]
            if len(self.buffer) < STREAM_HEADER_SIZE:
                return frames
            version, flags = self.buffer[2], self.buffer[3]
            length = struct.unpack_from("<I", self.buffer, 4)[0]
            if version != STREAM_VERSION or flags != 0 or length > self.maximum_envelope:
                del self.buffer[0]
                continue
            total = STREAM_HEADER_SIZE + length + STREAM_TRAILER_SIZE
            if len(self.buffer) < total:
                return frames
            envelope = bytes(self.buffer[STREAM_HEADER_SIZE : STREAM_HEADER_SIZE + length])
            expected_crc = struct.unpack_from("<I", self.buffer, STREAM_HEADER_SIZE + length)[0]
            del self.buffer[:total]
            if expected_crc != crc32(envelope):
                continue
            validate_envelope(envelope)
            frames.append(envelope)


class FleetIndex:
    """Small persistent fleet index used by lab collectors and qualification rigs."""

    def __init__(self, path: Path, strict_replay: bool = False) -> None:
        self.path = path
        self.strict_replay = strict_replay
        path.parent.mkdir(parents=True, exist_ok=True)
        self.db = sqlite3.connect(path)
        self.db.execute("PRAGMA journal_mode=WAL")
        self.db.execute("PRAGMA synchronous=FULL")
        self.db.executescript(
            """
            CREATE TABLE IF NOT EXISTS events(
              device_id TEXT NOT NULL,
              event_id INTEGER NOT NULL,
              sequence INTEGER NOT NULL,
              event_type INTEGER NOT NULL,
              architecture INTEGER NOT NULL,
              flags INTEGER NOT NULL,
              fingerprint INTEGER,
              priority INTEGER,
              severity INTEGER,
              build_id TEXT,
              firmware_version TEXT,
              variant TEXT,
              device_group TEXT,
              mission_id TEXT,
              dive_id TEXT,
              node_id TEXT,
              vehicle_mode TEXT,
              incident_id TEXT,
              file_path TEXT NOT NULL,
              received_ns INTEGER NOT NULL,
              sha256 TEXT NOT NULL,
              PRIMARY KEY(device_id, event_id)
            );
            CREATE INDEX IF NOT EXISTS events_fingerprint ON events(fingerprint, build_id);
            CREATE INDEX IF NOT EXISTS events_mission ON events(mission_id, dive_id);
            CREATE TABLE IF NOT EXISTS devices(
              device_id TEXT PRIMARY KEY,
              highest_sequence INTEGER NOT NULL,
              event_count INTEGER NOT NULL,
              last_build_id TEXT,
              last_seen_ns INTEGER NOT NULL
            );
            """
        )
        self.db.commit()

    def close(self) -> None:
        self.db.close()

    def record(self, device_id: str, envelope: bytes, info: EnvelopeInfo, metadata: EnvelopeMetadata,
               file_path: Path) -> None:
        import hashlib

        row = self.db.execute(
            "SELECT highest_sequence FROM devices WHERE device_id=?", (device_id,)
        ).fetchone()
        highest = int(row[0]) if row else 0
        if self.strict_replay and highest and info.sequence + 64 < highest:
            raise ProtocolError(
                f"stale sequence {info.sequence} for {device_id}; highest persisted is {highest}"
            )
        digest = hashlib.sha256(envelope).hexdigest()
        existing = self.db.execute(
            "SELECT sha256 FROM events WHERE device_id=? AND event_id=?", (device_id, info.event_id)
        ).fetchone()
        if existing and existing[0] != digest:
            raise ProtocolError("fleet index event ID collision with different bytes")
        if not existing:
            self.db.execute(
                """INSERT INTO events VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)""",
                (
                    device_id, info.event_id, info.sequence, info.event_type, info.architecture,
                    info.flags, metadata.fingerprint, metadata.priority, metadata.severity,
                    metadata.build_id, metadata.firmware_version, metadata.variant,
                    metadata.device_group, metadata.mission_id, metadata.dive_id, metadata.node_id,
                    metadata.vehicle_mode, metadata.incident_id, str(file_path), time.time_ns(), digest,
                ),
            )
        new_highest = max(highest, info.sequence)
        self.db.execute(
            """INSERT INTO devices(device_id,highest_sequence,event_count,last_build_id,last_seen_ns)
               VALUES(?,?,?,?,?)
               ON CONFLICT(device_id) DO UPDATE SET
                 highest_sequence=max(highest_sequence, excluded.highest_sequence),
                 event_count=event_count + ?,
                 last_build_id=COALESCE(excluded.last_build_id,last_build_id),
                 last_seen_ns=excluded.last_seen_ns""",
            (device_id, new_highest, 1 if not row else 0, metadata.build_id, time.time_ns(),
             0 if existing else 1),
        )
        self.db.commit()

    def crash_clusters(self) -> list[dict[str, object]]:
        rows = self.db.execute(
            """SELECT fingerprint, build_id, COUNT(*), COUNT(DISTINCT device_id),
                      MIN(received_ns), MAX(received_ns)
               FROM events WHERE fingerprint IS NOT NULL AND fingerprint != 0
               GROUP BY fingerprint, build_id ORDER BY COUNT(*) DESC, fingerprint"""
        ).fetchall()
        return [
            {"fingerprint": row[0], "build_id": row[1], "events": row[2], "devices": row[3],
             "first_seen_ns": row[4], "last_seen_ns": row[5]}
            for row in rows
        ]


class DurableStore:
    def __init__(self, root: Path, device_id: str | None = None, index: FleetIndex | None = None) -> None:
        if device_id is not None and not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,63}", device_id):
            raise ValueError("device_id must be 1-64 safe filename characters")
        self.device_id = device_id
        self.base_root = root
        self.index = index
        self.base_root.mkdir(parents=True, exist_ok=True)

    @staticmethod
    def _safe_namespace(device_id: str | None) -> str:
        if device_id and re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,63}", device_id):
            return device_id
        return "unknown"

    def persist(self, envelope: bytes, info: EnvelopeInfo) -> int:
        metadata = extract_metadata(envelope)
        effective_device = self.device_id or metadata.device_id or "unknown"
        # Always namespace event IDs by device. A 32-bit event ID is intentionally
        # compact on-device and is not globally unique across a fleet.
        namespace = self._safe_namespace(self.device_id or metadata.device_id)
        root = self.base_root / namespace
        root.mkdir(parents=True, exist_ok=True)
        destination = root / f"{info.event_id:08x}.lep"
        if destination.exists():
            if destination.read_bytes() != envelope:
                raise ProtocolError("event ID collision with different bytes")
            if self.index:
                self.index.record(effective_device, envelope, info, metadata, destination)
            return LSAK_DUPLICATE
        descriptor, temporary = tempfile.mkstemp(prefix=".latch-", dir=root)
        try:
            with os.fdopen(descriptor, "wb") as output:
                output.write(envelope)
                output.flush()
                os.fsync(output.fileno())
            os.replace(temporary, destination)
            if hasattr(os, "O_DIRECTORY"):
                directory_fd = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
                try:
                    os.fsync(directory_fd)
                finally:
                    os.close(directory_fd)
            if self.index:
                self.index.record(effective_device, envelope, info, metadata, destination)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
        return LSAK_STORED


def collect(chunks: Iterable[bytes], output: BinaryIO, store: DurableStore,
            maximum_envelope: int = 4096) -> int:
    parser = StreamParser(maximum_envelope)
    count = 0
    for chunk in chunks:
        for envelope in parser.feed(chunk):
            info = validate_envelope(envelope)
            metadata = extract_metadata(envelope)
            status = store.persist(envelope, info)
            output.write(make_ack(status, info.event_id))
            output.flush()
            print(json.dumps({**asdict(info), **asdict(metadata),
                              "device_id": store.device_id or metadata.device_id,
                              "status": "stored" if status == 1 else "duplicate"}),
                  file=sys.stderr)
            count += 1
    return count


def file_chunks(stream: BinaryIO, size: int = 256) -> Iterable[bytes]:
    while chunk := stream.read(size):
        yield chunk


def serial_chunks(connection: BinaryIO, size: int = 256) -> Iterable[bytes]:
    while True:
        chunk = connection.read(size)
        if chunk:
            yield chunk


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--input", type=Path, help="framed binary input; use - for stdin")
    source.add_argument("--serial", help="serial device such as COM3 or /dev/ttyUSB0")
    parser.add_argument("--output", type=Path, required=True, help="durable envelope directory")
    parser.add_argument("--device-id", help="optional fleet/device namespace for stored event IDs")
    parser.add_argument("--index", type=Path, help="optional SQLite fleet index")
    parser.add_argument("--strict-replay", action="store_true",
                        help="reject sequences more than 64 behind the device high-water mark")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--maximum-envelope", type=int, default=4096)
    args = parser.parse_args()
    index = FleetIndex(args.index, args.strict_replay) if args.index else None
    try:
        store = DurableStore(args.output, args.device_id, index)
    except ValueError as error:
        if index:
            index.close()
        parser.error(str(error))
    try:
        if args.serial:
            try:
                import serial  # type: ignore[import-not-found]
            except ImportError as error:
                raise SystemExit("serial mode requires: python -m pip install pyserial") from error
            with serial.Serial(args.serial, args.baud, timeout=1) as connection:
                collect(serial_chunks(connection), connection, store, args.maximum_envelope)
        else:
            stream = sys.stdin.buffer if str(args.input) == "-" else args.input.open("rb")
            try:
                collect(file_chunks(stream), sys.stdout.buffer, store, args.maximum_envelope)
            finally:
                if stream is not sys.stdin.buffer:
                    stream.close()
    except (OSError, ProtocolError, sqlite3.Error) as error:
        print(f"latch-collector: {error}", file=sys.stderr)
        return 1
    finally:
        if index:
            index.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
