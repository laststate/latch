# Commercial AUV integration readiness

Latch can be integrated as a crash/evidence subsystem in a commercial AUV, but the
repository alone cannot qualify an AUV. Production approval belongs to the complete
vehicle configuration and its applicable safety/regulatory process.

This document uses commercial AUV integration readiness as an illustrative example of
the level of maturity and assurance toward which the repository is being updated.

## Software gates provided by this repository

- deterministic host tests plus sanitizer and warnings-as-errors jobs;
- a production-source coverage gate over `src/`, `arch/` and `ports/` of at least
  **93% line** and **80% branch** coverage (current local validation: **93.76% line / 80.19% branch**);
- critical mutation campaign for high-value transport, parser and critical-spool decisions;
- daily and weekly bounded fuzz campaigns with retained corpus/artifacts;
- transactional spool/storage recovery tests including interrupted writes;
- Critical/Emergency spool reservation and runtime operational counters;
- retained black-box recording with mission/dive/node context, incident IDs, synchronized time, AUV environment evidence and fault fingerprints;
- health supervision for watchdog/deadline, brownout/power, battery, temperature, heap, spool, boot-loop, leak, vibration and environment-sensor degradation;
- pluggable cryptographic provider boundary, replay protection and secure storage;
- provisioning/key-lifecycle primitives including secure-element attestation and fail-closed secure decommissioning;
- generic authenticated A/B OTA orchestration plus update-state/monotonic version-floor primitives for bootloader integration;
- named fault-injection points exercised through storage, spool and transport durability boundaries;
- fleet collector/report tooling for device namespaces, persistent replay state, crash clustering, build comparison, canary guardrails, symbol lookup and privacy-conscious support bundles;
- signed stable-tag policy, SBOM/provenance and stable-release HIL manifest gate.

## Mandatory product work before deployment

1. Select one exact board/MCU/flash/bootloader/toolchain/profile configuration.
2. Execute every required HIL scenario and retain machine-readable evidence tied to
   the full firmware commit.
3. Integrate a signed A/B or recovery-capable bootloader. Latch update state does not
   verify a firmware signature by itself.
4. Provision device identity/keys using the product's secure manufacturing process;
   use the crypto-provider boundary for an approved implementation/secure element.
5. Measure real-time, stack, memory, endurance and power behavior on the final board.
6. Execute vehicle-level power-cut, brownout, vibration, thermal, pressure and EMC
   qualification plus mission-duration soak tests.
7. Complete the vehicle FMEA/FTA and prove Latch is not a single point of failure for
   control or recovery.
8. Commission an independent security/cryptographic review before making assurance
   claims beyond repository test evidence.

See [AUV observability runtime](auv-observability-runtime.md), [production readiness](production-readiness.md), [HIL](hil.md),
[real-time qualification](realtime-budget.md) and [safety assumptions](safety/assumptions-of-use.md).
