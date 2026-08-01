# AI validation guide

An AI handoff is useful only when its evidence can be reproduced. Select the
smallest validation set that covers the change, then state all omissions.

## Baseline for portable C changes

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
python tools/check_docs.py
python tools/check_test_vector.py
python tools/test_minify_sources.py
```

For changes that may depend on optimization, initialization, or configuration,
also run the release preset:

```sh
cmake --preset host-release
cmake --build --preset host-release
ctest --preset host-release
```

## Add these checks by surface

| Surface | Evidence expected |
| --- | --- |
| Public C API | C/C++ regression tests, compatibility review, docs, and Rust binding review when applicable. |
| LEP, decoder, compression, stream | Valid and malformed inputs, bounds/overflow cases, vector compatibility, matching fuzz harness review. |
| Cryptography or secure storage | Known-answer/negative/tamper tests, key/workspace wiping and power-loss behavior, [SECURITY.md](../../SECURITY.md) review. |
| Spool, flash mirror, wear leveling | Simulated interruption/corruption/recovery tests; no claim of physical flash qualification without HIL. |
| Architecture fault path | Host-model tests plus stated ABI, stack, linker, and retained-memory assumptions; HIL only if authorized. |
| Port/integration | Exact SDK/toolchain build evidence and documented hardware status. |
| Rust | `cargo fmt -- --check`, Clippy with warnings denied, and crate tests. |
| Docs | `python tools/check_docs.py`; verify every technical claim against source or named evidence. |
| Workflow/dependency | Relevant workflow syntax/behavior; preserve pins and security gates. |

## Evidence format

Use a concise, falsifiable handoff:

```text
Changed
- <behavior and file-level summary>

Validated
- <exact command> — passed
- <exact command> — passed

Not run
- <check> — <why it was not relevant or unavailable>

Risk / assumptions
- <compatibility, security, storage, memory, or hardware assumption>
```

Do not substitute a generic “tests pass” statement for commands. Do not infer a
physical result from a host test, or a production qualification from CI.
