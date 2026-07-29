# Integration overview

Latch produces LEP v1 envelopes for the Last State stack:

```
Device (Latch) → stream/UART/TCP → Relay → Trace
```

## LastState components

| Component | Role | Public status on 2026-07-29 |
| --- | --- | --- |
| Protocol | LEP v1 wire format and golden vectors | Private; the normative Latch-facing contract is included in this repository |
| Relay | Edge ingest, validation, durable spool, acknowledgement and delivery | Private and not publicly released |
| Trace | Backend storage, workers and UI | Private and not publicly released |

Latch is the first public component and has no build or runtime dependency on
the private repositories. A collector can integrate through the public LEP,
stream-framing and LSAK contracts.

## Contract points

- LEP magic `LSTP`, 24-byte header, CRC-32/IEEE
- Architecture codes 0–4 (unknown, cortex-m, riscv, xtensa, linux)
- TLV types 1–15 as in `include/laststate/envelope.h` and the protocol registry
- Stream framing: `"LS"` prefix + LEP + CRC; optional LSAK acknowledgements

See [lep-v1.md](lep-v1.md) and
[`stream_transport.h`](../include/laststate/stream_transport.h) for normative
Latch-facing details.
