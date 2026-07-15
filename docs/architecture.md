# Architecture

Latch has a deliberately narrow critical path:

```text
fault / error -> fixed state snapshot -> LEP envelope -> persistent spool -> reboot
                                                        -> best available transport -> acknowledgement
```

The core owns no memory allocator, scheduler, network stack or hardware register map. Integrators supply storage, a timestamp source, reset-reason normalizer, reset callback and one or more transports.

`ls_storage_backend_t` is synchronous because it is called in the capture path. A flash backend should use a power-loss-safe journal or dual region, honour its erase geometry, and make `sync` wait for completion. The supplied spool has per-record CRC and writes `state = PENDING` last; interrupted records remain ignored on recovery.

The LEP encoder places identity, reset metadata, event summary, optional CPU frame, breadcrumbs and metrics into TLVs. Unknown TLVs are skippable by their length. All current multibyte values are emitted for little-endian MCU targets; a future public decoder/test-vector module will lock this down formally before a wire-format major release.

For Cortex-M, only the entry selection belongs in assembly: the handler tests `EXC_RETURN[2]`, reads the original MSP or PSP, and transfers both to C. The application must set fault-handler priorities and reset policy suited to its MCU. Verify an actual target before enabling fault persistence in production.
