# Crypto compliance checklist (ISO 27001 / SOC 2 / HIPAA)

Maps Latch crypto controls to the frameworks regulated customers ask about.
This is a **self-assessed control mapping**, not a certification — retain the
evidence pack (`audit-evidence.md`) alongside any customer questionnaire.

## ISO/IEC 27001:2022

| Control | Latch implementation | Evidence |
|---------|----------------------|----------|
| A.8.24 Cryptography use | XChaCha20-Poly1305 envelopes, HKDF-SHA256 keys; commercial profile mandates external-audited provider | `crypto-assurance.md`, KAT log |
| A.8.24 Key management | Key IDs + replay window on-wire; provisioning procedure per release | evidence pack rows 6+8 |
| A.8.13 Backup / A.8.32 Change mgmt | Crypto changes gated by KAT + `ctest` vectors | CI log |

## SOC 2 (CC6 — Logical access / encryption)

| Criterion | Latch implementation | Evidence |
|-----------|----------------------|----------|
| CC6.1/6.6 encryption in transit+at rest | LEP AEAD envelopes; Relay TLS + spool encryption where configured | `relay/docs/security.md`, enrollment config |
| CC6.8 integrity | Poly1305 tags; altered-tag rejection tested | KAT log (altered-tag row) |
| CC7.2 monitoring | Failed-auth (`LS_EAUTH`) counters surfaced to Relay metrics | Relay observability docs |

## HIPAA (Technical safeguards §164.312)

| Safeguard | Latch implementation | Evidence |
|-----------|----------------------|----------|
| (a)(2)(iv) encryption | AEAD envelopes end-to-end (device → Relay → Trace) | architecture + KAT |
| (e)(1)/(e)(2) transmission security/integrity | HMAC/AEAD wire + TLS transport | `relay/docs/security.md` |
| (b) audit controls | Envelope key-id + auth-failure logging | log samples in release pack |

## Customer questionnaire block (paste-ready)

> Latch ships a portable built-in cipher suite labelled
> `builtin-unqualified` (tested, not externally audited) and a commercial
> profile that enforces an externally-audited provider
> (`LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER=1`). Per release we retain provider
> version/hashes, upstream audit reference, KAT logs (decrypt + altered-tag
> rejection), provisioning procedure, and integration review — see
> `docs/security/audit-evidence.md`. No independent audit of Latch's full
> integration or customer key lifecycle has been performed to date.
