#!/usr/bin/env python3
"""Small durable reference collector for Latch's LS/LSAK serial protocol."""

from __future__ import annotations

import argparse
import binascii
import json
import os
import struct
import sys
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import BinaryIO, Iterable

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


def crc32(data: bytes) -> int:
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
        payload = memoryview(data)[LEP_HEADER_SIZE : LEP_HEADER_SIZE + payload_length]
        offset = 0
        while offset < len(payload):
            if len(payload) - offset < 4:
                raise ProtocolError("truncated LEP TLV")
            field_type, field_length = struct.unpack_from("<HH", payload, offset)
            offset += 4
            if field_type == 0 or field_length > len(payload) - offset:
                raise ProtocolError("invalid LEP TLV")
            offset += field_length
    return EnvelopeInfo(version, event_type, architecture, flags, sequence, event_id, payload_length)


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


class DurableStore:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.root.mkdir(parents=True, exist_ok=True)

    def persist(self, envelope: bytes, info: EnvelopeInfo) -> int:
        destination = self.root / f"{info.event_id:08x}.lep"
        if destination.exists():
            if destination.read_bytes() != envelope:
                raise ProtocolError("event ID collision with different bytes")
            return LSAK_DUPLICATE
        descriptor, temporary = tempfile.mkstemp(prefix=".latch-", dir=self.root)
        try:
            with os.fdopen(descriptor, "wb") as output:
                output.write(envelope)
                output.flush()
                os.fsync(output.fileno())
            os.replace(temporary, destination)
            if hasattr(os, "O_DIRECTORY"):
                directory_fd = os.open(self.root, os.O_RDONLY | os.O_DIRECTORY)
                try:
                    os.fsync(directory_fd)
                finally:
                    os.close(directory_fd)
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
            status = store.persist(envelope, info)
            output.write(make_ack(status, info.event_id))
            output.flush()
            print(json.dumps({**asdict(info), "status": "stored" if status == 1 else "duplicate"}),
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
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--maximum-envelope", type=int, default=4096)
    args = parser.parse_args()
    store = DurableStore(args.output)
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
    except (OSError, ProtocolError) as error:
        print(f"latch-collector: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
