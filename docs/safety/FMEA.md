# Integration FMEA starter

This FMEA is a reusable starting point. A product program must assign severity,
probability, detectability, owners and verification evidence for its actual vehicle.

| Failure mode | Local effect | Vehicle-level concern | Detection | Required mitigation |
| --- | --- | --- | --- | --- |
| Latch spool full | Normal evidence cannot be queued | Loss of useful diagnostics | `ls_spool_get_stats()` dropped/high-watermark | Reserved Critical/Emergency capacity; rate limits; collector health alert |
| NVM program/erase failure | Record cannot commit | Missing post-fault evidence | storage error + spool counters | qualified flash; ECC/bad-block strategy in product backend; retained fallback |
| Power lost during commit | Partial record | Corrupt/latest record unavailable | commit marker + CRC recovery | transactional storage; brownout/HIL cut-point sweep |
| Corrupt committed record | One incident cannot decode | Forensics gap | CRC/auth failure + corrupt counter | skip bounded corrupt record; retain corruption telemetry; investigate flash health |
| Entropy failure | Encryption cannot safely proceed | Confidentiality/replay risk | RNG callback error | fail closed; hardware RNG/DRBG health tests; no nonce reuse |
| Key loss/rotation error | Old records unavailable or new writes fail | Evidence unavailable | provider/storage error | atomic key lifecycle in product; recovery procedure; secure backup policy if allowed |
| Transport unavailable | Queue grows | Delayed incident reporting | transport failure/retry counters | local retention sizing; backoff; multiple product transport paths where required |
| Collector ACK before durable store | Device deletes only copy | Permanent evidence loss | integration/HIL scenario | ACK only after durable collector commit |
| Fault handler recursion | Capture path faults again | Reset loop / no snapshot | recursion guard | bounded emergency path; independent watchdog; no normal runtime in handler |
| Invalid stack pointer | Unsafe memory read | secondary fault | stack-bound checks | reject snapshot; preserve registers/reset reason only |
| Firmware rollback | Known vulnerable image executes | security/safety regression | pending/floor state | signed bootloader + hardware-backed monotonic counter |
| Latch runtime bug | timing/memory interference | control-loop degradation | watchdog/perf monitoring | keep Latch off primary control path; WCET/stack budgets; sanitizers/HIL |

The product safety case must additionally analyze failures in navigation, propulsion,
pressure hull, battery/BMS, leak sensing, communications, recovery beacon and any
payload whose failure can affect vehicle safety. Those systems are outside Latch.
