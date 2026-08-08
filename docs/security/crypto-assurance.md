# Cryptography assurance policy

Latch has two deliberately different crypto modes.

## Development / compatibility mode

The portable built-in HKDF-SHA256 and XChaCha20-Poly1305 implementation remains available so
freestanding targets, tests, vectors and interoperability checks can run without a hosted crypto
library. It is labelled `builtin-unqualified`. It has strong automated testing but **does not claim
an independent external audit**.

## Commercial profile

Configure with:

```sh
cmake -S . -B build -DLS_COMMERCIAL_PROFILE=ON ...
```

This sets `LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER=1` and the minimum assurance level to
`LS_CRYPTO_ASSURANCE_EXTERNAL_AUDITED`. With that profile:

- no provider => crypto operations fail with `LS_EAUTH`;
- a test-only or merely unspecified provider => installation fails;
- missing provider version/audit reference => installation fails;
- provider callback errors never fall back to the built-in implementation;
- provider installation runs fixed HKDF-SHA256 and XChaCha20-Poly1305 known-answer tests;
- the KAT includes successful decrypt and rejection of an altered authentication tag.

The assurance marker is a policy input, not a magic proof. Product owners must pin the exact crypto
library/secure-element version and retain the supplier/audit evidence referenced by the provider.

## libsodium adapter

`ports/libsodium/` provides an optional adapter. Build it with
`-DLS_BUILD_LIBSODIUM_PROVIDER=ON`. The adapter labels itself externally audited because upstream
libsodium documentation records a third-party security audit as complete. This does **not** mean
Latch's full integration or a vehicle's key lifecycle has been independently audited.

Upstream evidence consulted for this policy:

- https://libsodium.gitbook.io/doc/roadmap
- https://libsodium.gitbook.io/doc
- https://libsodium.gitbook.io/doc/commercial_support

For an AUV release, retain the exact library version, binary/source hashes, build options, KAT result,
provisioning procedure and integration review with the release qualification evidence.
