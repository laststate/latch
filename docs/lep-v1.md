# LEP v1 wire format

This document defines the Latch Envelope Protocol version 1. Senders emit the canonical representation described here. Receivers must reject malformed lengths, unknown flag bits and invalid CRCs before processing payload data.

## Byte order and framing

All multi-byte integers are unsigned little-endian. A complete envelope is exactly one header, optional security metadata, payload, payload CRC32 and optional authentication data. CRC32 uses the IEEE polynomial with initial value `0xffffffff` and final bitwise inversion.

The fixed 24-byte header is:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | Magic `0x5054534c` (`LSTP` bytes) |
| 4 | 1 | Version (`1`) |
| 5 | 1 | Event type |
| 6 | 1 | Architecture |
| 7 | 1 | Flags |
| 8 | 4 | Sequence |
| 12 | 4 | Event ID |
| 16 | 4 | Payload length |
| 20 | 4 | CRC32 of bytes 0 through 19 |

The total length is `24 + metadata_length + payload_length + 4 + authentication_length`. Integer overflow or any mismatch is invalid.

## Flags and security layouts

`AUTHENTICATED` is bit 0, `ENCRYPTED` bit 1, `AEAD` bit 2 and `TRUNCATED` bit 3. Gateways may use bit 4 `COMPRESSED` (zstd); Latch devices do not set bit 4. No other bits are valid. `ENCRYPTED` and `AEAD` must either both be set or both be clear; AEAD also requires `AUTHENTICATED`.

Without AEAD, metadata length is zero and the payload immediately follows the header. The four-byte payload CRC32 follows the payload. If only `AUTHENTICATED` is set, a 32-byte HMAC-SHA-256 of header, payload and payload CRC follows the CRC.

With AEAD, metadata length is 28 bytes: a 24-byte XChaCha20 nonce followed by a four-byte key ID. The ciphertext has `payload_length` bytes. Its four-byte CRC32 covers metadata and ciphertext. A 16-byte Poly1305 tag follows the CRC. The header and metadata are AEAD associated data. Implementations must authenticate before exposing decrypted TLVs.

`TRUNCATED` tells receivers that optional fields were omitted because the sender had insufficient envelope capacity; it does not make a malformed frame acceptable.

## Payload

An unencrypted payload is a sequence of TLVs. Each TLV is a two-byte nonzero type, a two-byte value length and that many value bytes. TLVs are contiguous with no padding. Unknown types are skipped by length. A receiver must reject a zero type, an incomplete TLV header or a value extending past the payload boundary.

Types 1 through 15 are currently assigned to identity, reset, event, CPU, fault, breadcrumb, metric, power, health, assert, peripheral, log, memory, stack and heap data respectively. New types may be added in future minor protocol revisions; existing type semantics are immutable in v1.

## Versioning and replay

Version 1 is the only currently supported version. A receiver must reject unsupported versions rather than guessing an alternate layout. Sequence numbers are unsigned 32-bit counters. A receiver that needs replay protection keeps the highest accepted sequence and a 64-message sliding window; duplicate or stale values outside that window are rejected.

The C and Rust decoders share the golden vectors in `tests/test_envelope_errors.c` and `rust/latch/src/lib.rs`. Any wire-format change requires new vectors and a protocol version change.
