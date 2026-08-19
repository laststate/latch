## Overview

This document specifies the signature scheme used by Latch envelopes to ensure integrity and authenticity of telemetry data.

## Signature Algorithm

Latch uses **Ed25519** for envelope signatures:

- **Algorithm:** Ed25519 (RFC 8032)
- **Key Type:** Public/private keypair
- **Signature Size:** 64 bytes
- **Hash:** SHA-512 (internal to Ed25519)

## Signature Process

1. **Serialize** the envelope body (TLV-encoded)
2. **Compute** Ed25519 signature over the serialized body
3. **Append** signature as TLV tag `0x01` (SIGNATURE)
4. **Include** public key as TLV tag `0x02` (SIGNING_KEY)

## Verification

1. **Extract** signature and public key from envelope
2. **Verify** Ed25519 signature against envelope body
3. **Reject** if signature is invalid

## Security Considerations

- Public keys should be rotated periodically
- Private keys must be stored securely (HSM recommended)
- Signature verification is mandatory before processing envelopes

## References

- [RFC 8032](https://tools.ietf.org/html/rfc8032) - Ed25519 Signature Scheme
- [Latch Encryption Spec](encryption.md)
