from __future__ import annotations

import pathlib
import struct
import zlib

root = pathlib.Path(__file__).resolve().parents[1]
data = bytes.fromhex((root / "tests/vectors/lep-v1-basic.hex").read_text(encoding="ascii"))
assert data[:4] == b"LSTP"
assert data[4] == 1
payload_length = struct.unpack_from("<I", data, 16)[0]
assert len(data) == 24 + payload_length + 4
assert struct.unpack_from("<I", data, 20)[0] == zlib.crc32(data[:20]) & 0xFFFFFFFF
payload = data[24 : 24 + payload_length]
assert struct.unpack_from("<I", data, 24 + payload_length)[0] == zlib.crc32(payload) & 0xFFFFFFFF
field_type, field_length = struct.unpack_from("<HH", payload)
assert field_type == 1 and field_length == 2 and payload[4:] == b"\xaa\xbb"
print("LEP public test vector passed")
