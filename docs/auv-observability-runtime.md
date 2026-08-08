# AUV observability runtime

Latch 0.3 adds an AUV-oriented evidence layer while deliberately remaining outside the vehicle control and safe-state path. A Latch failure must not prevent propulsion shutdown, leak response, emergency ascent, acoustic recovery, watchdog reset or another safety function.

## Retained black box

`laststate/blackbox.h` provides a fixed-capacity, heap-free retained ring intended for a target's `.noinit`/retained-RAM region. Each record carries its own CRC. Normal writers use the configured critical-section callbacks; the CPU-fault path only freezes the recorder and does not enter normal runtime locking. `ls_blackbox_get_recent()` lets LEP export a bounded tail without placing the full ring on a task stack.

Profiles allow quiet, normal and anomaly capture. Supervisory/peripheral faults can temporarily raise recording detail, and a CPU fault freezes the current history before the retained crash snapshot is recovered on the next boot.

## Mission and incident context

`laststate/mission.h` tracks bounded mission, dive, node and vehicle-mode context plus phase and depth. Incidents use 128-bit identifiers so events from several computers can be correlated without relying on the compact per-device 32-bit event number. `laststate/time_sync.h` anchors monotonic time to an external UTC source (GNSS/PTP/NTP/RTC/host) with an uncertainty field.

`laststate/fingerprint.h` derives stable event/crash fingerprints from fault context and build identity for fleet-side clustering. Fingerprints are diagnostics, not cryptographic signatures.

## Environment and health supervision

`laststate/environment.h` records pressure, depth, internal temperature, humidity, vibration RMS, water ingress and pressure-sensor status. `laststate/supervisor.h` evaluates configured thresholds for watchdog/deadline staleness, power/brownout, battery, temperature, heap, spool occupancy, boot loops, leak, vibration and environment-sensor faults. Alarm transitions are recorded; the application decides the safe-state response.

`laststate/trace.h` supplies bounded task/IRQ/mutex/DMA/state/link breadcrumbs. Peripheral diagnostic helpers also feed the retained recorder.

## Provisioning and OTA boundaries

`laststate/provisioning.h` models activation, key rotation, revocation and decommissioning. For hardware-backed keys, use secure decommissioning: Latch first persists a decommissioning state, requests destruction through the secure-element callback and marks the identity decommissioned only after destruction succeeds. Attestation delegates signing to the selected secure element.

`laststate/ota.h` is an orchestration boundary, not a bootloader. The product supplies callbacks to authenticate an image, stage the inactive slot, request boot, confirm or roll back. The persistent update state enforces a monotonic confirmed-version floor and keeps the pending image fingerprint/key ID for recovery and evidence.

## Fault injection and qualification

`laststate/fault_injection.h` defines named fail points. The implementation is wired through Flash programming, spool header/payload/commit, send/ACK and transport boundaries so hosted tests can model power loss or I/O failure between durability phases. Product HIL must repeat the equivalent failures on the selected board/Flash/bootloader.

## Fleet-side workflow

The reference collector namespaces data by device, stores replay/high-sequence state in SQLite and clusters crash fingerprints. `latch_fleet_report.py` compares builds and can apply conservative canary promotion guardrails based on candidate sample size, new crash signatures and crash-device-rate delta. `latch_symbols.py` resolves addresses against an exact build symbol manifest. `latch_support_bundle.py` creates a deterministic evidence ZIP and excludes likely private keys, credentials and environment-secret files unless explicitly overridden.

These tools are reference operational components. A commercial fleet backend still needs product-specific authentication/authorization, tenant isolation, backup/retention, monitoring, incident response and independently reviewed key management.
