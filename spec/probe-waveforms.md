## Overview

Probe waveforms represent hardware signal captures (e.g., from logic analyzers, oscilloscopes) within Latch envelopes.

## Waveform Format

- **TLV Tag:** `0x11` (PROBE_WAVEFORM)
- **Sample Rate:** 4 bytes (Hz)
- **Channel Count:** 2 bytes
- **Sample Count:** 4 bytes
- **Data:** Variable (interleaved samples, 16-bit little-endian)
- **Compression:** 1 byte (0x00 = raw, 0x01 = delta, 0x02 = run-length)

## Channels

Each channel is identified by a 1-byte ID:
- `0x00`: Analog input 0
- `0x01`: Analog input 1
- `0x10`: Digital input 0
- `0x11`: Digital input 1
- ...

## Compression

### Raw (0x00)
No compression. Samples stored as-is.

### Delta (0x01)
Differential encoding: `sample[i] - sample[i-1]`

### Run-Length (0x02)
Consecutive identical samples are encoded as `(count, value)`.

## Constraints

- Maximum waveform size: 1 MB
- Maximum sample rate: 100 MSa/s
- Maximum channels: 16

## References

- [Latch TLV Registry](registry/tlv-types.md)
- [Latch Attachments Spec](attachments.md)
