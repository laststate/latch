## Overview

Batch envelopes allow multiple Latch envelopes to be transmitted together for efficiency.

## Batch Format

```
[BATCH_HEADER] [ENVELOPE_1] [ENVELOPE_2] ... [ENVELOPE_N] [BATCH_FOOTER]
```

### Batch Header

- **Magic:** 4 bytes (0x4C 0x41 0x54 0x43 = "LATC")
- **Version:** 1 byte (current: 1)
- **Count:** 2 bytes (number of envelopes)
- **Timestamp:** 8 bytes (batch creation time)

### Batch Footer

- **Checksum:** 4 bytes (CRC32 of all envelope payloads)
- **Signature:** Variable (Ed25519 over header + all payloads)

## Usage

Batches are optional but recommended for:
- High-throughput scenarios
- Network efficiency
- Atomic delivery guarantees

## Constraints

- Maximum batch size: 64 KB
- Maximum envelopes per batch: 1000
- Envelopes within a batch must be from the same source

## References

- [Latch TLV Registry](registry/tlv-types.md)
- [Latch Encryption Spec](encryption.md)
