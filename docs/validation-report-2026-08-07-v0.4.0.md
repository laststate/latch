# Validation report — 2026-08-07 — v0.4.0

## Scope

Assurance hardening focused on commercial cryptography and mutation testing.

## Results

| Gate | Result |
|---|---|
| Host suite | 59/59 PASS |
| GCC Release `-Werror` | 59/59 PASS |
| Clang Release `-Werror` | 59/59 PASS |
| ASan + UBSan | 59/59 PASS |
| Production coverage | 93.80% lines / 80.07% branches |
| Critical mutation campaign | 40/40 killed, 100%, 0 invalid |
| Commercial profile build | PASS |
| Release metadata v0.4.0 | PASS |
| Safety-C policy | PASS |
| HIL matrix validation | PASS |
| Workflow YAML parse | PASS |

## Crypto assurance result

The in-tree portable crypto implementation is retained for development, freestanding compatibility
and vectors, but is explicitly labelled `builtin-unqualified`. With `LS_COMMERCIAL_PROFILE=ON`, the
Latch runtime refuses cryptographic operations until a provider with
`LS_CRYPTO_ASSURANCE_EXTERNAL_AUDITED` is installed and passes fixed HKDF-SHA256 and
XChaCha20-Poly1305 KATs, including a negative authentication test. Legacy HMAC policy is disabled in
that profile. An optional libsodium adapter is supplied.

This is a trust-path correction, not a fabricated independent audit of Latch itself. The exact
third-party library/secure-element version and the product integration still require retained audit
and qualification evidence.

## Mutation assurance result

The broad campaign initially exposed survivors. Tests were strengthened rather than removing those
mutants. The final 40-mutant run covers crypto, secure storage, flash/wear-level, spool, transport,
OTA, provisioning, supervisor logic and parsing and finished with no survivors or invalid mutants.
