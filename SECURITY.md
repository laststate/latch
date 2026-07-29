# Security policy

Report suspected vulnerabilities through the repository's [private vulnerability reporting form](https://github.com/laststate/latch/security/advisories/new). Do not open a public issue containing device keys, captured memory, exploit details or production endpoint credentials.

## Cryptographic design

New authenticated envelopes use XChaCha20-Poly1305 with a 256-bit key, a 192-bit CSPRNG nonce and the full 128-bit tag. The fixed LEP header, nonce and `key_id` are authenticated as AAD. HKDF-SHA-256 derives domain-separated, per-envelope keys from the provisioned master key, `key_id`, sequence and event ID. Decryption authenticates before writing plaintext and comparisons are constant-time.

At-rest storage uses a separate HKDF domain and a generation-specific key. Each complete image is authenticated and encrypted before a transactional wear-level slot is committed. Repeated adjacent nonces are rejected, corrupted or partial images never replace the last committed image, and key material/workspaces are wiped after use.

The older HMAC-SHA-256 envelope format is verification-only unless `allow_legacy_hmac` is explicitly enabled. Raw ChaCha20 remains exposed for interoperability tests and must never be used alone for application data.

## Provisioning requirements

- Provision a unique random 256-bit master key per device. Do not derive it from a serial number, MAC address, password or firmware secret.
- Register a cryptographically secure random provider before enabling envelope or at-rest encryption. The API deliberately returns `LS_ENOTSUP` instead of falling back to the runtime PRNG.
- Increment `key_id` on every key rotation and retain old keys server-side only for the required migration window.
- Keep the master key in a secure element or hardware-backed keystore where available. The CryptoAuthLib adapter exposes hardware RNG, P-256 signing, certificate reconstruction and hardware HMAC derivation.
- Use verified TLS in addition to envelope encryption. Never set peer verification to optional in production.
- Treat dumps as sensitive even when encrypted. Register the smallest safe regions and apply exclusion, zeroing or hashing before capture.

## Assurance and limitations

The implementation is heap-free and includes RFC known-answer vectors, libsodium interoperability, negative/tamper tests, sanitizer runs, Valgrind, fuzzing and simulated power-loss recovery. These checks do not replace an independent cryptographic review, side-channel evaluation on the target MCU, secure provisioning review or product certification. Do not claim FIPS, Common Criteria or PSA certification based on this repository alone.
