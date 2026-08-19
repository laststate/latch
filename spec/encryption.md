# Latch Encryption Spec

## Overview

Security flags on the LEP header select one of three envelope modes: none, HMAC-only, or AEAD.

## Flags

| Flag | Bit | Role |
|------|----:|------|
| AUTHENTICATED | 0 | Auth trailer present |
| ENCRYPTED | 1 | Payload ciphertext |
| AEAD | 2 | Metadata block present |

Rules: ENCRYPTED implies AEAD; AEAD requires AUTHENTICATED. TRUNCATED (3) and COMPRESSED (4) may combine with the security flags.

## AEAD — XChaCha20-Poly1305 (device path)

| Item | Value |
|------|-------|
| Algorithm | XChaCha20-Poly1305 |
| Metadata | 28 bytes: 24-byte nonce + `key_id` u32 LE |
| AAD | header(24) \|\| metadata(28) |
| IKM | 32-byte device key |
| KDF | HKDF-SHA256 |
| salt | `key_id \|\| sequence \|\| event_id` (12 bytes LE) |
| info | `laststate/latch/envelope/v1` (ASCII, no NUL) |
| Output | 32-byte derived key |

Wire layout: header(24) \|\| metadata(28) \|\| ciphertext \|\| CRC32(metadata \|\| ciphertext) \|\| Poly1305 tag(16).

MUST NOT reuse (derived_key, nonce). Implementations MUST verify the tag before exposing plaintext.

## Auth-only — HMAC-SHA-256

Wire layout: header(24) \|\| payload \|\| CRC32(4) \|\| HMAC(32).

```
mac = HMAC-SHA-256(key, header || payload || payload_crc)
```

Key id is out of band (config); prefer the same numeric id space as AEAD `key_id`.

## Replay

Default sequence window size: **64**.

- Same `event_id` + same hash: `ACK_DUPLICATE`
- Same `event_id` + different hash: reject
- New sequence in window: accept

## Non-goals (v1.0)

- Asymmetric envelope signatures (see [signatures.md](signatures.md) for bundles)
- Gateway-only historical metadata packing

## References

- [Latch Transport Spec](transports.md)
- [Latch TLV Registry](registry/tlv-types.md)
