# Authenticated encryption and secure storage

Latch uses XChaCha20-Poly1305 for new LEP envelopes and authenticated at-rest images. The master key is never used directly: HKDF-SHA-256 derives independent envelope and storage keys, and storage keys additionally change with every committed generation.

```c
uint8_t device_key[LS_SECURITY_KEY_SIZE]; /* provision from a secure source */
ls_security_set_random_provider(platform_csprng, platform_context);
ls_security_set_key(device_key, sizeof device_key);

ls_security_policy_t policy = {
    .algorithm = LS_SECURITY_XCHACHA20_POLY1305,
    .key_id = 7,
    .reject_plaintext = true,
    .allow_legacy_hmac = false,
};
ls_security_set_policy(&policy);
```

The random callback must return unpredictable bytes from a TRNG or approved DRBG. Failure is propagated; Latch does not substitute its sampling PRNG. `key_id` is public authenticated metadata and must change when the master key changes.

For Flash, wrap the raw erase/program driver with wear leveling, then encryption:

```c
size_t sealed = ls_secure_storage_sealed_size(LOGICAL_BYTES);
ls_flash_wear_level_t wear;
ls_flash_wear_init(&wear, &raw_flash, wear_workspace, sealed, 8);

ls_secure_storage_t secure;
ls_secure_storage_init(&secure, &wear.backend,
                       crypto_workspace, sizeof crypto_workspace,
                       LOGICAL_BYTES, device_key, 7,
                       platform_csprng, platform_context);
ls_storage_register(&secure.backend);
```

The raw Flash capacity must be at least `ls_flash_wear_physical_size(sealed, erase_size, slots)`. Each logical update creates a complete encrypted candidate in a different slot, verifies structural CRCs on boot and selects only the newest committed authenticated image. `ls_flash_wear_stats()` exposes erase distribution and failed commits.

Never reuse a workspace between concurrently active backends. Call `ls_secure_storage_destroy()` before releasing or repurposing its key/workspace memory.

For key replacement, use an atomic mirror/wear-level backend and follow the
dual-acceptance procedure in the [threat model](threat-model.md). The
`ls_secure_storage_rotate_key()` API decrypts the current committed image,
re-seals it under a new nonzero key ID, and restores the in-memory old-key state
if the commit fails. Envelope keys must be switched separately after old queued
events are drained.
