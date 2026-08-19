# Latch Transport Spec

## Overview

How LEP envelopes are carried between Latch and a collector. Drivers and product glue are out of scope.

## Profiles

| Profile | Framing | Notes |
|---------|---------|-------|
| Serial / USB-CDC | Latch Stream (default) or COBS; optional LSAK reverse | Primary Latch-to-Relay path |
| TCP / TLS | Latch Stream or length-prefix | Continuous stream needs framing |
| UDP | one datagram = one envelope (or fragments) | Loss possible; retransmit same `event_id` |
| HTTP(S) | raw body = one LEP, or batch | Auth is HTTP-layer |
| MQTT | payload = raw LEP | QoS does not imply a durable LEP ACK unless the bridge stores first |
| File | raw `LSTP...` or concatenated Latch Stream | Directory ingest |
| CAN / CAN-FD / BLE / LoRa / LoRaWAN / RS-485 | fragmentation required when MTU < envelope | Product assigns bus IDs / topics |

## HTTP Batch (gateway to Trace)

Magic `LSBT`, version 1, flags (bit0 = zstd of body), then records:

`id_len u16 | id UTF-8 | payload_len u32 | LEP bytes`

## Bundle (export)

ZIP with `manifest.json` + `events/*.lep`. Optional Ed25519 over a canonical manifest digest. Not the same as envelope AEAD.

## References

- [Latch Encryption Spec](encryption.md)
- [Latch TLV Registry](registry/tlv-types.md)
