# Public reference collector

`tools/latch_collector.py` is a deliberately small, auditable collector for the
public `LS`/`LSAK` stream contract. It incrementally resynchronizes a byte
stream, bounds frame sizes, verifies both the outer frame CRC and the complete
LEP envelope, persists the exact envelope with `fsync` plus atomic rename, and
only then replies with `LSAK_STORED`. Repeated event IDs receive
`LSAK_DUPLICATE` without creating another record.

Read a captured framed stream without dependencies:

```sh
python tools/latch_collector.py --input capture.bin --output collected > acknowledgements.bin
```

Connect directly to a board after installing the optional serial dependency:

```sh
python -m pip install pyserial
python tools/latch_collector.py --serial COM3 --baud 115200 --output collected
```

On Linux, replace `COM3` with a device such as `/dev/ttyUSB0`. The collector
writes one `<event-id>.lep` file per event and emits one JSON summary per ACK to
stderr, leaving stdout available for binary ACKs in file/stdin mode.

This is a reference implementation, not a hosted service. A production
collector must additionally authenticate devices, apply tenant authorization,
rate limits, retention policy and monitoring. Encrypted LEP frames can be
validated and durably stored without decrypting them; key management remains a
deployment responsibility.
