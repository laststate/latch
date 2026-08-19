# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/add_laststate_headers.py
#
# Header hygiene script. Prepends canonical LastState header to
# source files. Idempotent; skips files with existing SPDX.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""
One-shot hygiene helper: prepend the canonical LastState file header to source
files across the repo. Idempotent: skips files that already begin with the
SPDX identifier. Designed for the latch-hygiene pass described in the session
log.

This is a script, not a tool. Run it directly and then delete it (or keep it
under tools/ if the project wants a repeatable header-bootstrap).
"""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]

# Custom one-paragraph purpose statement per file. Anything not listed falls
# back to the GENERIC template. Keep entries short; the header is for humans
# scanning unfamiliar source.
PURPOSES: dict[str, str] = {
    # ---- include/laststate/ public headers ----
    "include/laststate/anomaly.h":
        "Anomaly detection API for Latch — threshold-based monitoring of\n"
        "// battery, temperature, voltage, and current. Zero overhead when\n"
        "// disabled (LS_ENABLE_ANOMALY=OFF in config.h).",
    "include/laststate/assert.h":
        "Assertion and fault-handling policy for Latch. Selects how an\n"
        "// failed assertion is reported at runtime (continue, reset, halt,\n"
        "// breakpoint, or custom callback).",
    "include/laststate/blackbox.h":
        "Retained flight/mission recorder. Captures bounded recent events\n"
        "// across power cycles using .noinit memory; diagnostic only and\n"
        "// must never be used as a safety-control state store.",
    "include/laststate/boot.h":
        "Boot, reset-reason, and safe-mode API for Latch. Marks expected\n"
        "// resets, records successful boots, surfaces reboot-loop detection,\n"
        "// and gates safe-mode entry on crash history.",
    "include/laststate/breadcrumb.h":
        "Structured breadcrumbs — typed key/value events appended to the\n"
        "// next LEP envelope to reconstruct activity around a fault. Bounded\n"
        "// ring with drop-oldest, drop-newest, and keep-errors policies.",
    "include/laststate/build_id.h":
        "Build identity API. Exposes the compile-time build ID, git SHA,\n"
        "// and a validator used to confirm a serialized record was produced\n"
        "// by the expected firmware revision.",
    "include/laststate/capture.h":
        "Crash-capture API for Latch. Records fault context, CPU registers,\n"
        "// and dump regions into a minimal retained snapshot that survives\n"
        "// a CPU reset, ready for serialization on next boot.",
    "include/laststate/compression.h":
        "LEP stream compression API. Heap-free LZ-style codec used to shrink\n"
        "// envelopes and dump regions before authenticated storage or\n"
        "// transport.",
    "include/laststate/config.h":
        "Build-time configuration for Latch. Every knob may be overridden by\n"
        "// the build system; defaults target a heap-free, deterministic\n"
        "// embedded runtime.",
    "include/laststate/dna.h":
        "DNA (Device Fingerprint) capture API. Extracts hardware-unique\n"
        "// runtime characteristics (MCU ID, boot-time variance, flash wear,\n"
        "// bootloader signature) for clone and batch analysis.",
    "include/laststate/envelope.h":
        "LEP envelope API. Wraps a captured fault with identity, build,\n"
        "// severity, and flags; this is the unit that gets authenticated,\n"
        "// stored, and shipped to the Relay.",
    "include/laststate/environment.h":
        "Runtime environment sampling API (battery, temperature, supply\n"
        "// voltage). Feeds health, anomaly, and DNA modules with the\n"
        "// on-device telemetry they consume.",
    "include/laststate/event.h":
        "Core Latch types — result codes, priorities, severities, event\n"
        "// kinds, architecture IDs, fault kinds, reset reasons, and the\n"
        "// register/dump layouts that travel inside LEP frames.",
    "include/laststate/fault_injection.h":
        "Fault-injection hooks used by host tests to exercise the crash\n"
        "// capture path deterministically without a real hardware fault.\n"
        "// Compiled out in release unless LS_ENABLE_FAULT_INJECTION is on.",
    "include/laststate/fingerprint.h":
        "Device fingerprint hashing. Combines MCU ID, build ID, and any\n"
        "// DNA samples into a stable identifier the Relay can use to\n"
        "// de-duplicate records from the same physical device.",
    "include/laststate/flash_storage.h":
        "Flash-backed storage API for Latch. Wraps the wear-level and\n"
        "// mirror layers behind a single put/get/copy interface used by\n"
        "// both secure and non-secure spool paths.",
    "include/laststate/health.h":
        "Device health API. Aggregates environment samples, anomaly events,\n"
        "// and boot counters into a single health snapshot the LEP envelope\n"
        "// can carry alongside a fault.",
    "include/laststate/identity.h":
        "Device identity API. Owns the long-lived device ID, key handles,\n"
        "// and certificate material used by secure storage and transport.\n"
        "// Tied to provisioning.",
    "include/laststate/latch.h":
        "Umbrella public header. Includes every stable LastState entry\n"
        "// point so a one-line #include gives a downstream project the\n"
        "// full public surface.",
    "include/laststate/log.h":
        "Diagnostic log API. Append-only level/message records that are\n"
        "// bundled into the next envelope when a fault occurs; never used\n"
        "// for control flow.",
    "include/laststate/memory.h":
        "Memory-region and noinit helpers. Lets a port mark the retained\n"
        "// region Latch uses across resets and expose it to the capture\n"
        "// path without leaking layout to the public surface.",
    "include/laststate/metrics.h":
        "Runtime metrics API. Counters, gauges, and histograms published\n"
        "// by the application and shipped with the next envelope on demand\n"
        "// or on fault.",
    "include/laststate/mission.h":
        "Mission / lifetime context API. Lets the device label the current\n"
        "// run (firmware slot, campaign, experiment ID) so an off-line\n"
        "// operator can correlate records to a deployment.",
    "include/laststate/network_transport.h":
        "Network transport adapter contract. The integration point between\n"
        "// Latch's authenticated spool and a port-supplied TLS-capable\n"
        "// socket; the port implements it, Latch owns retry policy.",
    "include/laststate/noinit.h":
        "Cross-toolchain noinit attribute macro. The single source of\n"
        "// truth for declaring .noinit storage so the capture path works\n"
        "// the same way on every supported compiler.",
    "include/laststate/ota.h":
        "Firmware-update / OTA API. Tracks pending releases, confirms\n"
        "// successful boot on the new image, and triggers rollback when\n"
        "// boot_loop detection fires.",
    "include/laststate/performance.h":
        "Performance counters API. Cycle and timing measurements that feed\n"
        "// health and anomaly; safe to call from fault context but only\n"
        "// with portable back-ends.",
    "include/laststate/peripheral.h":
        "Peripheral fault hooks. Lets a port translate peripheral errors\n"
        "// (bus, sensor, radio) into Latch events without Latch having to\n"
        "// know any vendor-specific driver.",
    "include/laststate/policy.h":
        "Runtime policy API. Centralizes retention, sampling, and\n"
        "// transport behavior knobs that a deployment can adjust without\n"
        "// recompiling the runtime.",
    "include/laststate/provisioning.h":
        "Provisioning API. Issues the initial device identity, key handles,\n"
        "// and Relay endpoint configuration that every other layer depends\n"
        "// on. One-shot at manufacturing time.",
    "include/laststate/secure_element.h":
        "Secure-element adapter contract. The port supplies a small\n"
        "// interface for key injection, AEAD, and true-random; Latch never\n"
        "// embeds a fallback PRNG for cryptographic use.",
    "include/laststate/secure_storage.h":
        "Authenticated, replay-protected storage API. Wraps the spool\n"
        "// path with AEAD, versioned nonces, and atomic power-loss-safe\n"
        "// commits so a partial write never replaces a good record.",
    "include/laststate/security.h":
        "Security primitives façade. Re-exports the cryptography entry\n"
        "// points used by Latch so a port can satisfy them either via the\n"
        "// built-in software provider or a secure element.",
    "include/laststate/selftest.h":
        "Power-on self-test API. A short, deterministic suite that runs\n"
        "// before Latch is marked healthy; its result is part of every\n"
        "// envelope's identity block.",
    "include/laststate/spool.h":
        "Bounded spool API. Holds serialized LEP envelopes awaiting\n"
        "// transport, with a drop policy that keeps errors and drops the\n"
        "// oldest healthy entries when space is tight.",
    "include/laststate/storage.h":
        "Non-secure storage API. Used by blackbox and health for diagnostic\n"
        "// data that does not need authentication; shares the wear-level\n"
        "// layer with the secure path.",
    "include/laststate/storage_sim.h":
        "Host simulator storage adapter. Lets the unit tests exercise the\n"
        "// real spool/storage code paths on a deterministic in-memory\n"
        "// backing without touching flash.",
    "include/laststate/stream_transport.h":
        "Stream transport adapter contract. The byte-oriented counterpart\n"
        "// to network_transport; Latch owns framing, AEAD, and retry, the\n"
        "// port owns read/write.",
    "include/laststate/supervisor.h":
        "Supervisor / watchdog API. Owns the watchdog feed, lockup\n"
        "// detection, and the reset reason reported on the next boot.\n"
        "// Designed to be ticked from a low-priority task.",
    "include/laststate/time_sync.h":
        "Time synchronization API. Lets a port feed monotonic and wall\n"
        "// clock time into Latch without exposing the chosen back-end;\n"
        "// timestamps travel inside every envelope.",
    "include/laststate/trace.h":
        "Lightweight execution trace API. A bounded event ring used to\n"
        "// reconstruct the path leading to a fault without the cost of a\n"
        "// full debugger trace.",
    "include/laststate/transport.h":
        "Transport façade. Re-exports the stream and network adapters\n"
        "// under one header so callers depend on a single interface that\n"
        "// the port chooses to satisfy.",
    "include/laststate/update.h":
        "Update / changelog API. A small, audited log of release metadata\n"
        "// and rollbacks that Latch carries in the envelope identity block\n"
        "// to correlate records to firmware.",
    "include/laststate/version.h":
        "Latch version constants. The single source of truth for the LEP\n"
        "// protocol version, the Latch runtime version, and the\n"
        "// compatibility matrix a port must satisfy.",

    # ---- src/laststate/ shims ----
    # These are 1-line include shims and share the same purpose as their
    # public header counterpart.
    **{
        f"src/laststate/{name}.h": purpose
        for name, purpose in {
            "anomaly": "anomaly detection",
            "assert": "assertion and fault-handling policy",
            "blackbox": "the retained flight recorder",
            "boot": "boot, reset-reason, and safe-mode",
            "breadcrumb": "structured breadcrumbs",
            "build_id": "build identity",
            "capture": "crash capture",
            "compression": "LEP stream compression",
            "config": "build-time configuration",
            "dna": "device DNA fingerprint capture",
            "envelope": "LEP envelope framing",
            "environment": "runtime environment sampling",
            "event": "core Latch types and enums",
            "fault_injection": "test fault-injection hooks",
            "fingerprint": "device fingerprint hashing",
            "flash_storage": "flash-backed storage",
            "health": "device health aggregation",
            "identity": "device identity and key handles",
            "latch": "the umbrella public header",
            "log": "diagnostic logging",
            "memory": "memory-region and noinit helpers",
            "metrics": "runtime metrics",
            "mission": "mission / lifetime context",
            "network_transport": "the network transport adapter",
            "noinit": "the cross-toolchain noinit macro",
            "ota": "firmware update / OTA",
            "performance": "performance counters",
            "peripheral": "peripheral fault hooks",
            "policy": "runtime policy",
            "provisioning": "provisioning",
            "secure_element": "the secure-element adapter",
            "secure_storage": "authenticated secure storage",
            "security": "security primitive façade",
            "selftest": "power-on self-test",
            "spool": "the bounded LEP spool",
            "storage": "non-secure storage",
            "storage_sim": "the host simulator storage adapter",
            "stream_transport": "the stream transport adapter",
            "supervisor": "the supervisor / watchdog",
            "time_sync": "time synchronization",
            "trace": "execution trace",
            "transport": "the transport façade",
            "update": "update / changelog",
            "version": "Latch version constants",
        }.items()
    },

    # ---- src/capture/ ----
    "src/capture/anomaly.c":
        "Anomaly detection implementation. Threshold checks for battery,\n"
        "// temperature, voltage, and current; emits a structured breadcrumb\n"
        "// and sets the ANOMALY flag on the next envelope when tripped.",
    "src/capture/capture.c":
        "Crash capture implementation. Serializes the fault context, CPU\n"
        "// registers, and dump regions into the minimal retained snapshot\n"
        "// that survives a CPU reset.",
    "src/capture/dna.c":
        "DNA capture implementation. Reads MCU ID, boot-time samples, and\n"
        "// flash wear through the port-supplied hooks; computes the boot\n"
        "// time mean and standard deviation.",

    # ---- src/core/ ----
    "src/core/assert.c":
        "Assertion policy implementation. Routes a failed assertion to the\n"
        "// configured policy — continue, reset, halt, breakpoint, or the\n"
        "// port's callback — without ever calling into malloc.",
    "src/core/blackbox.c":
        "Blackbox / flight recorder implementation. Bounded ring in .noinit\n"
        "// memory with drop-on-overflow, freeze/thaw for fault windows,\n"
        "// and an anomaly-aware profile switch.",
    "src/core/boot.c":
        "Boot, reset-reason, and safe-mode implementation. Detects the\n"
        "// reset cause, decides whether the previous reset was expected,\n"
        "// and arms safe-mode when the boot loop threshold trips.",
    "src/core/build_id.c":
        "Build identity implementation. Surfaces the compile-time build ID\n"
        "// and git SHA, and validates an incoming string against the\n"
        "// expected value before the rest of the runtime trusts it.",
    "src/core/environment.c":
        "Environment sampling implementation. Stores the most recent\n"
        "// battery, temperature, and voltage readings in a small static\n"
        "// struct that health and anomaly read without locking.",
    "src/core/fault_injection.c":
        "Fault-injection implementation. Bounded table of synthetic fault\n"
        "// triggers consulted by host tests; compiled out in release\n"
        "// unless LS_ENABLE_FAULT_INJECTION is set.",
    "src/core/fingerprint.c":
        "Fingerprint hashing implementation. Mixes MCU ID, build ID, and\n"
        "// DNA samples into a stable identifier used by the Relay to\n"
        "// de-duplicate records from the same physical device.",
    "src/core/health.c":
        "Device health aggregation. Combines environment samples, recent\n"
        "// anomaly events, and the boot counter into the health block that\n"
        "// travels inside the next envelope.",
    "src/core/internal.h":
        "Internal runtime header shared by src/. Not installed; declares\n"
        "// the runtime singleton, internal macros, and prototypes used\n"
        "// across capture, envelope, storage, and transport.",
    "src/core/log.c":
        "Diagnostic log implementation. Append-only level/message records\n"
        "// that are bundled into the next envelope when a fault occurs;\n"
        "// never used for control flow.",
    "src/core/memory.c":
        "Memory-region helpers. Owns the .noinit bookkeeping and exposes\n"
        "// region metadata to the capture and storage layers without\n"
        "// leaking the layout to the public surface.",
    "src/core/mission.c":
        "Mission / lifetime context implementation. Holds the current run\n"
        "// label, firmware slot, and campaign identifier so the envelope\n"
        "// identity block can carry deployment context.",
    "src/core/ota.c":
        "OTA implementation. Tracks the pending release, confirms the\n"
        "// new image on successful boot, and triggers rollback when boot\n"
        "// loop detection fires.",
    "src/core/performance.c":
        "Performance counters implementation. Cycle and timing\n"
        "// measurements backed by the port-supplied counter hooks; safe to\n"
        "// call from fault context.",
    "src/core/peripheral.c":
        "Peripheral fault hooks implementation. Translates port-reported\n"
        "// bus, sensor, and radio errors into Latch events without any\n"
        "// vendor-specific knowledge in the runtime itself.",
    "src/core/policy.c":
        "Runtime policy implementation. Centralizes retention, sampling,\n"
        "// and transport knobs so a deployment can change behavior at\n"
        "// boot without recompiling the runtime.",
    "src/core/provisioning.c":
        "Provisioning implementation. Issues the initial device identity\n"
        "// and key handles; one-shot at manufacturing time and idempotent\n"
        "// across re-provisioning attempts.",
    "src/core/runtime.c":
        "Runtime singleton and ls_init(). Owns the global ls_runtime_t,\n"
        "// validates the supplied config, and wires every subsystem up\n"
        "// before the first user-visible API call returns.",
    "src/core/selftest.c":
        "Power-on self-test implementation. Deterministic checks of the\n"
        "// crypto provider, storage layer, and identity block that run\n"
        "// before ls_init() marks the runtime healthy.",
    "src/core/supervisor.c":
        "Supervisor / watchdog implementation. Owns the watchdog feed,\n"
        "// lockup detection, and the reset reason recorded on the next\n"
        "// boot. Designed to be ticked from a low-priority task.",
    "src/core/time_sync.c":
        "Time synchronization implementation. Pulls monotonic and wall\n"
        "// clock time from the port-supplied hooks and exposes them to\n"
        "// every layer that stamps a LEP record.",
    "src/core/trace.c":
        "Execution trace implementation. Bounded event ring used to\n"
        "// reconstruct the path leading to a fault without the cost of a\n"
        "// full debugger trace.",
    "src/core/util.c":
        "Small portable utilities shared across src/. CRC32, bounded\n"
        "// string helpers, and integer-safe math — all heap-free and\n"
        "// safe to call from fault context.",

    # ---- src/envelope/ ----
    "src/envelope/compression.c":
        "Compression codec implementation. Heap-free LZ-style encode and\n"
        "// decode used to shrink envelopes and dump regions before\n"
        "// authenticated storage or transport.",
    "src/envelope/lep.c":
        "LEP envelope codec implementation. Serializes and parses every\n"
        "// field of the LEP v1 wire format; the authoritative reference\n"
        "// for what a record looks like on the wire.",

    # ---- src/metrics/ ----
    "src/metrics/breadcrumbs.c":
        "Breadcrumb ring implementation. Bounded, lock-free single-producer\n"
        "// ring with drop-oldest, drop-newest, and keep-errors policies;\n"
        "// appended to the next envelope on demand.",
    "src/metrics/metrics.c":
        "Runtime metrics implementation. Counters, gauges, and histograms\n"
        "// published by the application; serialized into the next envelope\n"
        "// on demand or on fault.",

    # ---- src/security/ ----
    "src/security/aead.c":
        "Authenticated encryption with associated data. Wires the chosen\n"
        "// AEAD construction (ChaCha20-Poly1305 by default) into the\n"
        "// secure storage and transport paths.",
    "src/security/chacha20.c":
        "ChaCha20 stream cipher implementation. Constant-time and\n"
        "// allocation-free; reused by AEAD and by the secure-element\n"
        "// adapter when no hardware primitive is available.",
    "src/security/crypto_internal.h":
        "Internal cryptography header shared by src/security/. Not\n"
        "// installed; declares the in-tree primitives and the provider\n"
        "// dispatch table used at runtime.",
    "src/security/poly1305.c":
        "Poly1305 MAC implementation. Constant-time and allocation-free;\n"
        "// reused by AEAD and by the secure-element adapter when no\n"
        "// hardware primitive is available.",
    "src/security/provider.c":
        "Cryptography provider dispatch. Selects between the in-tree\n"
        "// software provider and a port-supplied secure element at\n"
        "// ls_init(); never falls back to a weaker PRNG.",
    "src/security/secure_element.c":
        "Secure-element adapter glue. Forwards every key, AEAD, and\n"
        "// random call to the port when a hardware element is present;\n"
        "// returns ENOTSUP otherwise.",
    "src/security/sha256.c":
        "SHA-256 implementation. Constant-time, allocation-free, used by\n"
        "// the AEAD construction and by the build-id / fingerprint paths.",

    # ---- src/spool/ ----
    "src/spool/spool.c":
        "Bounded LEP spool implementation. Holds authenticated envelopes\n"
        "// awaiting transport, drops healthy entries first under pressure,\n"
        "// and survives a partial commit without exposing plaintext.",

    # ---- src/storage/ ----
    "src/storage/flash_mirror.c":
        "Flash-backed mirror storage. Two-bank write/erase so a power\n"
        "// loss can never leave a partially committed record visible to\n"
        "// the reader.",
    "src/storage/memory_storage.c":
        "In-memory storage adapter. The backing store used by host tests\n"
        "// to exercise the real storage code paths without touching flash.",
    "src/storage/secure_storage.c":
        "Authenticated secure storage implementation. AEAD-protected,\n"
        "// versioned, and power-loss-safe; rejects a record whose nonce\n"
        "// or MAC fails validation.",
    "src/storage/storage_sim.c":
        "Host simulator storage adapter. Deterministic in-memory backing\n"
        "// for unit tests; never linked into a release build.",
    "src/storage/wear_level.c":
        "Wear-leveling implementation. Spreads writes across the flash\n"
        "// region and exposes the put/get/copy primitives the spool and\n"
        "// secure storage layers build on.",

    # ---- src/transport/ ----
    "src/transport/network.c":
        "Network transport adapter glue. Drives the port-supplied TLS\n"
        "// socket through Latch's authenticated spool; owns retry, back-off,\n"
        "// and framing.",
    "src/transport/stream.c":
        "Stream transport adapter glue. Byte-oriented counterpart to the\n"
        "// network path; the same framing, AEAD, and retry policy, just\n"
        "// over a port-supplied read/write interface.",
    "src/transport/transport.c":
        "Transport façade implementation. Selects between the stream and\n"
        "// network adapters based on the configured policy and surfaces a\n"
        "// single API to the rest of the runtime.",

    # ---- tests/ ----
    "tests/test_latch.c":
        "Main Latch integration test. Exercises the full runtime init,\n"
        "// capture, storage, and transport path end-to-end.",
    "tests/test_capture.c":
        "Crash capture unit tests. Validates snapshot assembly, register\n"
        "// dump, and region encoding across fault scenarios.",
    "tests/test_envelope_errors.c":
        "LEP envelope negative tests. Malformed, truncated, and oversized\n"
        "// frames must be rejected without panic.",
    "tests/test_envelope_fuzz.c":
        "Fuzz harness for the LEP parser. Drives the decoder with corpus\n"
        "// inputs to catch edge cases.",
    "tests/test_spool_edges.c":
        "Spool boundary tests. Full/empty transitions, wrap-around, and\n"
        "// priority drop policy under pressure.",
    "tests/test_spool_priority.c":
        "Spool priority tests. Errors are retained; healthy entries drop\n"
        "// first when capacity is exceeded.",
    "tests/test_spool_stress.c":
        "Spool stress test. Rapid enqueue/dequeue cycles with concurrent\n"
        "// transport drain to expose races.",
    "tests/test_storage_bounds.c":
        "Storage bounds tests. Write/read at region edges, power-loss\n"
        "// simulation, and wear-level distribution.",
    "tests/test_secure_storage.c":
        "Authenticated storage tests. AEAD seal/unseal, nonce replay\n"
        "// protection, and power-loss atomicity.",
    "tests/test_secure_power_loss.c":
        "Power-loss atomicity tests for secure storage. Validates that\n"
        "// partial commits never expose plaintext or corrupt state.",
    "tests/test_crypto_vectors.c":
        "Cryptographic test vectors. Validates ChaCha20, Poly1305, and\n"
        "// AEAD against known-answer tests.",
    "tests/test_crypto_provider.c":
        "Provider dispatch tests. Switches between software and secure-\n"
        "// element backends at runtime.",
    "tests/test_crypto_negative.c":
        "Negative crypto tests. Invalid keys, tags, nonces, and lengths\n"
        "// must fail cleanly.",
    "tests/test_runtime_edges.c":
        "Runtime edge-case tests. Double init, null config, version\n"
        "// mismatch, and subsystem coupling.",
    "tests/test_property.c":
        "Property-based tests. Randomized sequences of API calls checked\n"
        "// against invariants.",
    "tests/test_replay_property.c":
        "Replay property tests. Serialized envelopes round-trip through\n"
        "// storage and transport without data loss.",
    "tests/test_transport.c":
        "Transport adapter tests. Stream and network backends exercise\n"
        "// framing, retry, and back-off.",
    "tests/test_transport_policy.c":
        "Transport policy tests. Retry limits, drop thresholds, and\n"
        "// adapter selection.",
    "tests/test_ota.c":
        "OTA tests. Pending release tracking, boot confirmation, and\n"
        "// rollback on boot-loop detection.",
    "tests/test_update.c":
        "Update metadata tests. Changelog encoding and identity block\n"
        "// correlation.",
    "tests/test_boot.c":
        "Boot and reset-reason tests. Expected/unexpected reset detection\n"
        "// and safe-mode gating.",
    "tests/test_supervisor.c":
        "Supervisor/watchdog tests. Feed timing, lockup detection, and\n"
        "// reset reason recording.",
    "tests/test_environment.c":
        "Environment sampling tests. Battery, temperature, voltage\n"
        "// readers feed health and anomaly.",
    "tests/test_anomaly.c":
        "Anomaly detection tests. Threshold trips emit breadcrumbs and\n"
        "// set the ANOMALY flag.",
    "tests/test_health.c":
        "Health aggregation tests. Combines environment, anomaly, and\n"
        "// boot counter into the envelope block.",
    "tests/test_metrics.c":
        "Runtime metrics tests. Counters, gauges, histograms serialize\n"
        "// into envelopes on demand.",
    "tests/test_breadcrumbs.c":
        "Breadcrumb ring tests. Drop policies and envelope append.",
    "tests/test_trace.c":
        "Execution trace tests. Bounded event ring reconstructs fault\n"
        "// path.",
    "tests/test_blackbox.c":
        "Blackbox/flight recorder tests. .noinit ring survives reset,\n"
        "// freeze/thaw on fault.",
    "tests/test_selftest.c":
        "Power-on self-test tests. Crypto, storage, identity validation\n"
        "// before runtime is marked healthy.",
    "tests/test_fault_injection_pipeline.c":
        "Fault-injection pipeline tests. Synthetic triggers exercise the\n"
        "// capture path deterministically.",
    "tests/test_memory_capture.c":
        "Memory capture tests. Dump region selection, encoding, and\n"
        "// retention across reset.",
    "tests/test_power_loss.c":
        "Power-loss simulation tests. Flash mirror and secure storage\n"
        "// atomicity under simulated power cuts.",
    "tests/test_flash.c":
        "Flash mirror tests. Two-bank write/erase, wear-level integration.",
    "tests/test_wear_level.c":
        "Wear-leveling tests. Write distribution across flash region.",
    "tests/test_file_backend.c":
        "File-backed storage tests. Host simulator backend for CI.",
    "tests/test_secure_element.c":
        "Secure element adapter tests. Key injection, AEAD, random\n"
        "// forwarding to port.",
    "tests/test_libsodium_provider.c":
        "libsodium provider interop tests. Validates Sodium-backed\n"
        "// crypto against in-tree implementation.",
    "tests/test_sodium_interop.c":
        "Sodium interop tests. Cross-impl vector compatibility.",
    "tests/test_defensive_paths.c":
        "Defensive path tests. Null pointers, bounds, version checks\n"
        "// in public APIs.",
    "tests/test_vendor_reset.c":
        "Vendor-specific reset tests. STM32, ESP32, Zephyr port hooks.",
    "tests/test_stm32_reset.c":
        "STM32 reset reason tests. Maps vendor codes to Latch enums.",
    "tests/test_riscv64_fault.c":
        "RISC-V 64-bit fault capture tests. Register layout and dump.",
    "tests/test_xtensa_fault.c":
        "Xtensa fault capture tests. Register layout and dump.",
    "tests/test_port_adapters.c":
        "Port adapter tests. Validates port-supplied hooks conform to\n"
        "// contracts.",
    "tests/test_profile.c":
        "Profile tests. Memory footprint, cycle counts, stack usage.",
    "tests/test_footprint.py":
        "Footprint measurement script. Parses map/elf for RAM/flash use.",
    "tests/test_fleet_tools.py":
        "Fleet tool tests. Collector, reporter, bundle validation.",
    "tests/test_latch_collector.py":
        "Collector tests. Device enrollment and record ingestion.",
    "tests/test_latch_dump.py":
        "Dump tool tests. Record parsing and pretty-print.",
    "tests/test_release_qualification.py":
        "Release qualification tests. HIL matrix validation.",
    "tests/test_release_metadata.py":
        "Release metadata tests. SBOM, version, changelog integrity.",
    "tests/test_embedded_packaging.py":
        "Embedded packaging tests. CPack, archive layout, manifest.",
    "tests/test_safety_c.py":
        "Safety-C checklist tests. MISRA, CERT, coding standard rules.",

    # ---- examples/ ----
    "examples/zephyr-first-capture/src/main.c":
        "Zephyr first-capture example. Minimal app demonstrating\n"
        "// ls_init(), fault trigger, and envelope retrieval.",
    "examples/esp32-first-crash/main/main.c":
        "ESP32 first-crash example. FreeRTOS port with crash capture\n"
        "// and LEP envelope output.",
    "examples/reference-designs/low-power-deep-sleep/main.c":
        "Low-power deep-sleep reference. Demonstrates supervisor tick\n"
        "// and wake-on-fault.",

    # ---- fuzz/ ----
    "fuzz/fuzz_lep.c":
        "LEP fuzz target. Feeds corpus data into the envelope parser.",
    "fuzz/fuzz_stream.c":
        "Stream transport fuzz target. Exercises framing and AEAD.\n"
        "// Corpus driven.",
    "fuzz/fuzz_compression.c":
        "Compression codec fuzz target. Encode/decode round-trips with\n"
        "// malformed inputs.",
    "fuzz/fuzz_aead.c":
        "AEAD fuzz target. ChaCha20-Poly1305 seal/unseal with corpus\n"
        "// inputs.",

    # ---- benchmarks/ ----
    "benchmarks/benchmark.c":
        "Microbenchmarks. Cycle counts for capture, envelope, crypto,\n"
        "// storage, and transport hot paths.",

    # ---- tools/ ----
    "tools/add_laststate_headers.py":
        "Header hygiene script. Prepends canonical LastState header to\n"
        "// source files. Idempotent; skips files with existing SPDX.",
    "tools/check_hil_matrix.py":
        "HIL qualification matrix validator. Ensures claims match the\n"
        "// hardware-compatibility.md table.",
    "tools/check_release_qualification.py":
        "Release qualification checker. Validates 23/23 matrix, coverage,\n"
        "// and artifact integrity before tag.",
    "tools/check_release_metadata.py":
        "Release metadata validator. SBOM, version, changelog, and\n"
        "// provenance checks.",
    "tools/check_public_api.py":
        "Public API surface checker. Detects accidental exports and\n"
        "// ABI breaks.",
    "tools/check_project_readiness.py":
        "Project readiness gate. CI green, coverage, docs, and release\n"
        "// artifacts all present.",
    "tools/check_gcov_coverage.py":
        "GCOV coverage parser. Enforces 90%+ line/branch thresholds.",
    "tools/check_docs.py":
        "Documentation checker. Broken links, stale examples, and\n"
        "// required section presence.",
    "tools/check_size.py":
        "Binary size checker. Flash/RAM footprint against budgets.",
    "tools/check_safety_c.py":
        "Safety-C coding standard checker. MISRA/CERT rule enforcement.",
    "tools/check_test_vector.py":
        "Test vector validator. Known-answer tests for crypto and LEP.",
    "tools/generate_sbom.py":
        "SBOM generator. SPDX/JSON output for supply-chain transparency.",
    "tools/generate_public_api_manifest.py":
        "Public API manifest generator. Exports header surface for\n"
        "// downstream consumers.",
    "tools/minify_sources.py":
        "Source minifier. Strips comments and whitespace for footprint\n"
        "// analysis; never used in release builds.",
    "tools/test_minify_sources.py":
        "Minifier test. Round-trip validation of stripped sources.",
    "tools/seed_fuzz_corpora.py":
        "Fuzz corpus seeder. Generates initial seeds from valid\n"
        "// envelopes and edge cases.",
    "tools/run_clang_tidy.py":
        "clang-tidy runner. Config-driven static analysis for CI.",
    "tools/mutation_smoke.py":
        "Mutation testing smoke. Quick sanity on mutant survival rate.",
    "tools/mutation_campaign.py":
        "Mutation testing campaign. Full mutant generation and kill\n"
        "// rate reporting.",
    "tools/measure_footprint.py":
        "Footprint measurement. Parses ELF/map for RAM/flash breakdown.",
    "tools/latch_symbols.py":
        "Symbol extractor. Public/private classification for ABI\n"
        "// tracking.",
    "tools/latch_support_bundle.py":
        "Support bundle generator. Collects logs, config, and state for\n"
        "// field debugging.",
    "tools/latch_fleet_report.py":
        "Fleet report generator. Aggregates device health, versions,\n"
        "// and anomaly trends.",
    "tools/latch_collector.py":
        "Collector CLI. Device enrollment and record ingestion.",
    "tools/latch_dump.c":
        "Dump tool. Parses and pretty-prints LEP envelopes from\n"
        "// storage or capture.",
    "tools/test_cpack_archive.py":
        "CPack archive test. Validates package layout and contents.",

    # ---- arch/ ----
    "arch/xtensa/xtensa.c":
        "Xtensa architecture port. Register save/restore, fault frame\n"
        "// layout, and port-specific hooks.",
    "arch/xtensa/xtensa.h":
        "Xtensa architecture port header. Register definitions and\n"
        "// fault frame layout for Xtensa cores.",

    # ---- trace/ (TypeScript/Python) ----
    "trace/web/src/api.ts":
        "Trace web API client. TypeScript wrapper for Relay REST\n"
        "// endpoints.",
    "trace/web/src/lep/codec.ts":
        "LEP codec for web. Encodes/decodes envelopes in TypeScript\n"
        "// for dashboard display.",
    "trace/web/src/views/index.ts":
        "Trace web view components. React views for fault timeline,\n"
        "// device fleet, and envelope inspector.",
    "trace/web/src/soundboard.ts":
        "Soundboard utility. Audio feedback for dashboard events.",
    "trace/web/src/notifications.ts":
        "Notification system. Toast/snackbar manager for web UI.",
    "trace/web/src/nav.ts":
        "Navigation helper. Route management for SPA.",
    "trace/web/src/helpers.test.ts":
        "Web helper unit tests.",
    "trace/web/src/lep/codec.test.ts":
        "LEP codec tests. Encode/decode round-trips and edge cases.",
    "trace/web/src/App.test.ts":
        "App component tests. Integration smoke for dashboard.",
    "trace/web/e2e/smoke.spec.ts":
        "Playwright smoke tests. Critical user flows.",
    "trace/web/e2e/product.spec.ts":
        "Playwright product tests. Full feature coverage.",
    "trace/sdk/python/trace_client.py":
        "Python Trace SDK. Client library for Relay API.",
    "trace/sdk/js/trace.js":
        "JavaScript Trace SDK. Browser/Node client for Relay API.",

    # ---- protocol/ ----
    "protocol/conformance/runner/run.py":
        "Protocol conformance runner. Validates implementations against\n"
        "// the LEP spec test vectors.",
}


import re

GENERIC_PURPOSE = (
    "Latch runtime source. Part of the heap-free, deterministic,\n"
    "embedded failure-capture runtime that ships fault state over LEP\n"
    "to the Relay."
)

PY_COPYRIGHT_LINE = "# Copyright 2024-2026 LastState Contributors"
CPP_COPYRIGHT_LINE = "// Copyright 2024-2026 LastState Contributors"

HEADER_PATTERN = re.compile(
    r"^(?:#|//|\*|\s)*SPDX-License-Identifier:.*?\bHeap-free, bounded, deterministic\.\s*\n*",
    re.DOTALL
)


def comment_style(path: Path) -> str:
    """Return the comment prefix for the given file extension."""
    suffix = path.suffix.lower()
    if suffix in (".py", ".sh", ".yml", ".yaml", ".toml", ".cmake"):
        return "#"
    return "//"


def format_purpose(raw: str, prefix: str) -> str:
    lines = []
    for line in raw.splitlines():
        line_clean = line.strip()
        if line_clean.startswith("//"):
            line_clean = line_clean[2:].strip()
        elif line_clean.startswith("#"):
            line_clean = line_clean[1:].strip()
        lines.append(f"{prefix} {line_clean}" if line_clean else prefix)
    return "\n".join(lines)


def strip_existing_header(text: str) -> str:
    match = HEADER_PATTERN.match(text)
    if match:
        return text[match.end():]
    return text


def build_header(rel_path: str, path: Path) -> str:
    """Build the canonical header for the given relative path."""
    purpose_raw = PURPOSES.get(rel_path, GENERIC_PURPOSE)
    prefix = comment_style(path)
    purpose = format_purpose(purpose_raw, prefix)
    copyright_line = f"{prefix} Copyright 2024-2026 LastState Contributors"
    spdx = f"{prefix} SPDX-License-Identifier: Apache-2.0"
    return (
        f"{spdx}\n"
        f"{copyright_line}\n"
        f"{prefix} {rel_path}\n"
        f"{prefix}\n"
        f"{purpose}\n"
        f"{prefix}\n"
        f"{prefix} Heap-free, bounded, deterministic.\n"
        "\n"
    )


def process_file(path: Path) -> bool:
    """Prepend/update the header. Returns True if modified."""
    rel = path.relative_to(REPO_ROOT).as_posix()
    if rel.startswith("src/laststate/"):
        return False
    text = path.read_text(encoding="utf-8")
    body = strip_existing_header(text)
    new_header = build_header(rel, path)
    new_text = new_header + body
    if new_text != text:
        path.write_text(new_text, encoding="utf-8")
        return True
    return False


def main() -> int:
    targets: list[Path] = []
    scan_dirs = {
        "include/laststate": ("*.c", "*.h"),
        "src": ("*.c", "*.h"),
        "tests": ("*.c", "*.h", "*.py"),
        "examples": ("*.c", "*.h"),
        "fuzz": ("*.c", "*.h"),
        "benchmarks": ("*.c", "*.h"),
        "tools": ("*.c", "*.h", "*.py"),
        "arch": ("*.c", "*.h", "*.S", "*.s"),
        "trace": ("*.ts", "*.js", "*.py"),
        "protocol": ("*.py",),
    }
    for sub, exts in scan_dirs.items():
        root = REPO_ROOT / sub
        if not root.is_dir():
            print(f"skip missing: {root}", file=sys.stderr)
            continue
        for ext in exts:
            targets.extend(sorted(root.rglob(ext)))

    modified = 0
    for path in targets:
        if process_file(path):
            modified += 1

    print(f"scanned {len(targets)} files, modified {modified}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
