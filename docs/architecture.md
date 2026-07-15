# Architecture

Latch has a deliberately narrow critical path:

```text
fault / error -> fixed state snapshot -> LEP envelope -> persistent spool -> reboot
                                                        -> best available transport -> acknowledgement
```

The core owns no memory allocator, scheduler, network stack or hardware register map. Integrators supply storage, a timestamp source, reset-reason normalizer, reset callback and one or more transports.

The thread, ISR and fault-context ownership rules are defined in [Concurrency](concurrency.md).

`ls_storage_backend_t` is synchronous because it is called in the capture path. A flash backend should use a power-loss-safe journal or dual region, honour its erase geometry, and make `sync` wait for completion. The supplied spool has per-record CRC and writes `state = PENDING` last; interrupted records remain ignored on recovery.

The LEP encoder places identity, reset metadata, event summary, optional CPU frame, breadcrumbs and metrics into TLVs. Unknown TLVs are skippable by their length. [LEP v1](lep-v1.md) defines canonical little-endian encoding, bounds checks, security layouts, truncation and versioning; the C and Rust decoders share golden vectors.

For Cortex-M, only the entry selection belongs in assembly: the handler tests `EXC_RETURN[2]`, reads the original MSP or PSP, and transfers both to C. The handler switches to a dedicated emergency stack, validates the selected frame against registered stack bounds and writes only a small `.noinit` snapshot. Normal boot promotes a valid retained snapshot into the transactional spool and clears it only after that append succeeds. The application must set fault-handler priorities and reset policy suited to its MCU. Verify an actual target before enabling fault persistence in production.
