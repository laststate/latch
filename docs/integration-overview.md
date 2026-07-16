# Integration overview

Latch produces LEP v1 envelopes for the Last State stack:

```
Device (Latch) → stream/UART/TCP → Relay → Trace
```

## Related repositories

| Repo | Role |
|------|------|
| [protocol](https://github.com/laststate/protocol) | LEP v1 wire format and golden vectors |
| [relay](https://github.com/laststate/relay) | Edge ingest, durable spool, delivery to Trace |
| [trace](https://github.com/laststate/trace) | Backend: store, workers, UI |

## Contract points

- LEP magic `LSTP`, 24-byte header, CRC-32/IEEE
- Architecture codes 0–4 (unknown, cortex-m, riscv, xtensa, linux)
- TLV types 1–15 as in `include/laststate/envelope.h` and the protocol registry
- Stream framing: `"LS"` prefix + LEP + CRC; optional LSAK acknowledgements

See [lep-v1.md](lep-v1.md) and the protocol repo for normative details.
