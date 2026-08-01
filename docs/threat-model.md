# Threat model and key lifecycle

This document defines what Latch's security mechanisms do and do not protect.
It is a starting point for a product threat model, not a security certification.

## Assets and trust boundaries

The sensitive assets are captured memory, logs and device identity; the device
master key; durable spool state; and collector credentials. Untrusted inputs
include LEP envelopes received by tools, stream framing, ACK messages, storage
bytes after interrupted writes and every platform callback.

Latch assumes the firmware image and the code executing before its fault
handler are trusted. It does not defend a device after arbitrary code execution,
debug-port compromise, physical key extraction, malicious DMA, or a compromised
bootloader. Secure boot, flash encryption, debug locking, MPU/MMU configuration,
TrustZone and key provisioning remain product responsibilities.

## Controls and residual risks

- LEP length fields, flags, CRCs and TLVs are bounded before use. CRC detects
  corruption but is not authentication.
- XChaCha20-Poly1305 provides confidentiality and integrity when a unique nonce
  comes from an approved CSPRNG and the per-device key remains secret.
- `key_id` selects a key but is public metadata. Authorization must bind device,
  tenant and accepted key IDs at the collector.
- The replay window rejects recent duplicate sequences in one running receiver;
  persistence of replay state across collector restarts is deployment-owned.
- Redaction applies only to explicitly registered dump regions. Breadcrumb and
  log content can still contain secrets if applications place them there.
- Durable `LSAK_STORED` means the collector persisted the exact envelope. It
  does not prove that an authorized operator can later decrypt or retain it.

## Rotation procedure

1. Provision the new random 256-bit device key through an authenticated channel
   and assign a never-before-used nonzero key ID.
2. Configure collectors to accept both old and new IDs during a bounded overlap.
3. Stop concurrent Latch writers and call `ls_secure_storage_rotate_key()` on a
   secure-storage layer backed by the transactional mirror or wear-level adapter.
4. Set the envelope policy to the new key ID and key, capture a canary event,
   and require its durable ACK.
5. Remove the old key only after every queued old-key envelope has been drained
   or its deliberate loss has been recorded.

An interrupted rotation is only recoverable when the underlying backend retains
the prior committed image. A raw in-place flash backend does not provide that
guarantee. Never reuse an ID for different key material, and never silently
erase an unreadable old image to make boot continue.

## Required product review

Review nonce generation under brownout, key zeroization and copies introduced by
the compiler, side channels, secure-element policy, collector authentication,
retention/deletion requirements, denial-of-service limits, rollback behavior and
incident response. Re-run the review when changing crypto providers, bootloader,
compiler, silicon revision, or trust-zone boundaries.
