# Crypto audit evidence (per-release retention)

Independent audit of Latch's full integration and of any vehicle's key
lifecycle has **not** been performed — see `crypto-assurance.md`. This file
defines exactly what evidence a product owner must retain so that a future
audit (or a customer security review) can verify the crypto actually shipped.

## What "audited" means here (and what it does not)

- The **built-in** HKDF-SHA256 / XChaCha20-Poly1305 implementation is
  `builtin-unqualified`: strong automated vectors, no external audit claim.
- The **commercial profile** (`LS_COMMERCIAL_PROFILE=ON`) refuses to run
  unless an externally-audited provider is pinned; the provider's own audit
  reference must be recorded below.
- The **libsodium adapter** (`ports/libsodium/`) inherits upstream libsodium's
  third-party audit — that covers libsodium itself, not Latch's integration,
  provisioning, or key storage.

## Per-release evidence pack (retain with qualification evidence)

| # | Artifact | Example |
|---|----------|---------|
| 1 | Provider name + exact version | `libsodium 1.0.19-stable`, git sha / tarball sha256 |
| 2 | Binary/source hashes + build options | `sha256sum lib/libsodium.a`, `-DLS_BUILD_LIBSODIUM_PROVIDER=ON` flags |
| 3 | Upstream audit reference | report title, auditor, date, URL (e.g. libsodium audit per `crypto-assurance.md` links) |
| 4 | KAT result log | `ctest -R crypto_kat` output showing HKDF + XChaCha decrypt success **and** altered-tag rejection |
| 5 | Provider install log | `LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER=1` install success / `LS_EAUTH` on missing provider |
| 6 | Provisioning procedure | how device keys are generated, injected, stored (secure element part # if any) |
| 7 | Integration review note | reviewer, date, scope (envelope path, replay-window, key-id handling) |
| 8 | Key lifecycle statement | rotation period, revocation path, what happens on compromise |

## KAT procedure (copy into release notes)

```sh
cmake -S . -B build -DLS_COMMERCIAL_PROFILE=ON -DLS_BUILD_LIBSODIUM_PROVIDER=ON ...
cmake --build build
ctest --preset host-debug -R 'crypto|kat|assurance' --output-on-failure
# Expected: HKDF-SHA256 known-answer PASS, XChaCha20-Poly1305 decrypt PASS,
# altered-tag rejection PASS, unqualified-builtin rejection PASS.
```

## Upstream references (consulted for this policy)

- https://libsodium.gitbook.io/doc/roadmap
- https://libsodium.gitbook.io/doc
- https://libsodium.gitbook.io/doc/commercial_support

Pin the exact pages/revisions you relied on in the release pack — upstream
docs move; your pack must not depend on a live URL.
