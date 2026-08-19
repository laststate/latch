# Known limitations and open release gates (v1.0.0)

This document records what v1.0.0 does **not** claim, and the gates that remain
open. It is the honest counterpart to the release notes. Items here are required
before Latch may be described as qualified for production on a given board or as
an LTS release.

## External review (blocker for a production claim)
- **Independent cryptographic, side-channel and provisioning review** is pending.
  Repository tests are not an audit or certification (`docs/production-readiness.md`,
  `docs/assurance.md`). Required before a product release.

## Hardware qualification
- Only **ESP32** is physically HIL-qualified. Every other supported board is
  **emulator-tested (Renode) only** — not physical-board qualification.
- **Automatic Xtensa panic-frame capture** is an integration boundary on ESP32 and
  has not been re-qualified on physical HIL.
- Per-board physical HIL, and vendor TLS/BLE/LoRaWAN/CAN/secure-element/TrustZone
  qualification remains product-qualification work.

## LTS eligibility gates (from `docs/lts-policy.md`)
1. At least three physically qualified configurations across ≥2 CPU architectures
   and ≥2 flash implementations — **not met** (1 qualifies).
2. 90 consecutive days without an unresolved critical regression — **not met**
   (window not started/recorded).
3. Published independent external security review under `docs/audits` — **not met**.
4. Release artifacts, SBOM, provenance, package metadata, downgrade behavior,
   migration notes and every claimed HIL record pass the release checklist —
   partially met; HIL records are honest but limited.

## Process
- 1.x stable tags must be **cryptographically signed**; the signed `v1.0.0` tag is
  created at tag time by the maintainer (`docs/releasing.md`).
- The Relay v1.0.0 waits on Latch/Trace E2E, HIL, protocol freeze and security
  review; coordinate the cross-repo release.
