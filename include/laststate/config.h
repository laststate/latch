// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/config.h
//
// Build-time configuration for Latch. Every knob may be overridden by
// the build system; defaults target a heap-free, deterministic
// embedded runtime.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_CONFIG_H
#define LASTSTATE_CONFIG_H

/* All settings may be overridden by the build system. */
/*
 * AVR builds select a conservative cooperative baseline automatically. Other
 * small C targets may opt in with -DLS_CONSTRAINED_PROFILE=1, but that
 * definition must reach every Latch C translation unit (not merely a C++
 * sketch). This profile never installs an architecture fault handler.
 */
#ifndef LS_CONSTRAINED_PROFILE
#if defined(__AVR__)
#define LS_CONSTRAINED_PROFILE 1
#else
#define LS_CONSTRAINED_PROFILE 0
#endif
#endif

#if LS_CONSTRAINED_PROFILE
#ifndef LS_ENABLE_BREADCRUMBS
#define LS_ENABLE_BREADCRUMBS 0
#endif
#ifndef LS_ENABLE_METRICS
#define LS_ENABLE_METRICS 0
#endif
#ifndef LS_ENABLE_LOGS
#define LS_ENABLE_LOGS 0
#endif
#ifndef LS_ENABLE_POWER_SAMPLES
#define LS_ENABLE_POWER_SAMPLES 0
#endif
#ifndef LS_ENABLE_PERFORMANCE
#define LS_ENABLE_PERFORMANCE 0
#endif
#ifndef LS_ENABLE_DUMPS
#define LS_ENABLE_DUMPS 0
#endif
#ifndef LS_ENABLE_ASSERTS
#define LS_ENABLE_ASSERTS 0
#endif
#ifndef LS_ENABLE_STACK_SNAPSHOT
#define LS_ENABLE_STACK_SNAPSHOT 0
#endif
#ifndef LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER
#define LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER 0
#endif

#ifndef LS_MIN_CRYPTO_ASSURANCE
#define LS_MIN_CRYPTO_ASSURANCE 3
#endif

#ifndef LS_STORE_STRINGS
#define LS_STORE_STRINGS 0
#endif
#ifndef LS_COMPILED_MIN_LEVEL
#define LS_COMPILED_MIN_LEVEL 2
#endif
#ifndef LS_BREADCRUMB_CAPACITY
#define LS_BREADCRUMB_CAPACITY 1u
#endif
#ifndef LS_METRIC_CAPACITY
#define LS_METRIC_CAPACITY 1u
#endif
#ifndef LS_POWER_SAMPLE_CAPACITY
#define LS_POWER_SAMPLE_CAPACITY 1u
#endif
#ifndef LS_HEALTH_CAPACITY
#define LS_HEALTH_CAPACITY 1u
#endif
#ifndef LS_SPAN_CAPACITY
#define LS_SPAN_CAPACITY 1u
#endif
#ifndef LS_MAX_TRANSPORTS
#define LS_MAX_TRANSPORTS 1u
#endif
#ifndef LS_MAX_DUMP_REGIONS
#define LS_MAX_DUMP_REGIONS 1u
#endif
#ifndef LS_MAX_REDACTIONS
#define LS_MAX_REDACTIONS 1u
#endif
#ifndef LS_DUMP_REGION_MAX_BYTES
#define LS_DUMP_REGION_MAX_BYTES 1u
#endif
#ifndef LS_STACK_SNAPSHOT_MAX
#define LS_STACK_SNAPSHOT_MAX 1u
#endif
#ifndef LS_MAX_EVENT_SIZE
#define LS_MAX_EVENT_SIZE 512u
#endif
#ifndef LS_SPOOL_MAX_RECORDS
#define LS_SPOOL_MAX_RECORDS 1u
#endif
#ifndef LS_SPOOL_RESERVED_CRITICAL
#define LS_SPOOL_RESERVED_CRITICAL 0u
#endif
#ifndef LS_SPOOL_RESERVED_EMERGENCY
#define LS_SPOOL_RESERVED_EMERGENCY 0u
#endif
#ifndef LS_POLICY_CAPACITY
#define LS_POLICY_CAPACITY 1u
#endif
#ifndef LS_DEDUP_CAPACITY
#define LS_DEDUP_CAPACITY 1u
#endif
#ifndef LS_BLACKBOX_CAPACITY
#define LS_BLACKBOX_CAPACITY 4u
#endif
#ifndef LS_BLACKBOX_ENVELOPE_MAX
#define LS_BLACKBOX_ENVELOPE_MAX 2u
#endif
#ifndef LS_SELFTEST_CAPACITY
#define LS_SELFTEST_CAPACITY 2u
#endif
#ifndef LS_FAULT_INJECTION_CAPACITY
#define LS_FAULT_INJECTION_CAPACITY 2u
#endif
#endif
#ifndef LS_ENABLE_BREADCRUMBS
#define LS_ENABLE_BREADCRUMBS 1
#endif
#ifndef LS_ENABLE_METRICS
#define LS_ENABLE_METRICS 1
#endif
#ifndef LS_ENABLE_LOGS
#define LS_ENABLE_LOGS 1
#endif
#ifndef LS_ENABLE_POWER_SAMPLES
#define LS_ENABLE_POWER_SAMPLES 1
#endif
#ifndef LS_ENABLE_PERFORMANCE
#define LS_ENABLE_PERFORMANCE 1
#endif
#ifndef LS_ENABLE_DUMPS
#define LS_ENABLE_DUMPS 1
#endif
#ifndef LS_ENABLE_ASSERTS
#define LS_ENABLE_ASSERTS 1
#endif
#ifndef LS_ENABLE_STACK_SNAPSHOT
#define LS_ENABLE_STACK_SNAPSHOT 1
#endif
#ifndef LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER
#define LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER 0
#endif

#ifndef LS_MIN_CRYPTO_ASSURANCE
#define LS_MIN_CRYPTO_ASSURANCE 3
#endif

#ifndef LS_STORE_STRINGS
#define LS_STORE_STRINGS 1
#endif
#ifndef LS_COMPILED_MIN_LEVEL
#define LS_COMPILED_MIN_LEVEL 0
#endif
/* RV64 retained register state is intentionally opt-in: it reserves a
 * bounded .noinit sidecar in addition to the portable minimal snapshot. */
#ifndef LS_ENABLE_WIDE_CONTEXT
#define LS_ENABLE_WIDE_CONTEXT 0
#endif

#ifndef LS_BREADCRUMB_CAPACITY
#define LS_BREADCRUMB_CAPACITY 32u
#endif
#ifndef LS_BREADCRUMB_MESSAGE_MAX
#define LS_BREADCRUMB_MESSAGE_MAX 48u
#endif
#ifndef LS_BREADCRUMB_CATEGORY_MAX
#define LS_BREADCRUMB_CATEGORY_MAX 16u
#endif
#ifndef LS_BREADCRUMB_KV_MAX
#define LS_BREADCRUMB_KV_MAX 4u
#endif
#ifndef LS_METRIC_CAPACITY
#define LS_METRIC_CAPACITY 16u
#endif
#ifndef LS_METRIC_NAME_MAX
#define LS_METRIC_NAME_MAX 32u
#endif
#ifndef LS_METRIC_WINDOW_SIZE
#define LS_METRIC_WINDOW_SIZE 8u
#endif
#ifndef LS_POWER_SAMPLE_CAPACITY
#define LS_POWER_SAMPLE_CAPACITY 16u
#endif
#ifndef LS_HEALTH_CAPACITY
#define LS_HEALTH_CAPACITY 16u
#endif
#ifndef LS_SPAN_CAPACITY
#define LS_SPAN_CAPACITY 8u
#endif
#ifndef LS_LOG_ARG_MAX
#define LS_LOG_ARG_MAX 4u
#endif
#ifndef LS_MAX_TRANSPORTS
#define LS_MAX_TRANSPORTS 6u
#endif
#ifndef LS_MAX_DUMP_REGIONS
#define LS_MAX_DUMP_REGIONS 8u
#endif
#ifndef LS_MAX_REDACTIONS
#define LS_MAX_REDACTIONS 8u
#endif
#ifndef LS_DUMP_REGION_MAX_BYTES
#define LS_DUMP_REGION_MAX_BYTES 512u
#endif
#ifndef LS_STACK_SNAPSHOT_MAX
#define LS_STACK_SNAPSHOT_MAX 256u
#endif
#ifndef LS_MAX_EVENT_SIZE
#define LS_MAX_EVENT_SIZE 4096u
#endif
#ifndef LS_SPOOL_MAX_RECORDS
#define LS_SPOOL_MAX_RECORDS 8u
#endif
#ifndef LS_SPOOL_RESERVED_CRITICAL
#define LS_SPOOL_RESERVED_CRITICAL 2u
#endif
#ifndef LS_SPOOL_RESERVED_EMERGENCY
#define LS_SPOOL_RESERVED_EMERGENCY 1u
#endif
#if LS_SPOOL_RESERVED_CRITICAL > LS_SPOOL_MAX_RECORDS
#error "LS_SPOOL_RESERVED_CRITICAL cannot exceed LS_SPOOL_MAX_RECORDS"
#endif
#if LS_SPOOL_RESERVED_EMERGENCY > LS_SPOOL_RESERVED_CRITICAL
#error "LS_SPOOL_RESERVED_EMERGENCY cannot exceed LS_SPOOL_RESERVED_CRITICAL"
#endif
#ifndef LS_EMERGENCY_STACK_SIZE
#define LS_EMERGENCY_STACK_SIZE 768u
#endif
#ifndef LS_BOOT_LOOP_THRESHOLD
#define LS_BOOT_LOOP_THRESHOLD 3u
#endif
#ifndef LS_BOOT_LOOP_WINDOW_MS
#define LS_BOOT_LOOP_WINDOW_MS 60000u
#endif
#ifndef LS_BUILD_ID_MAX
#define LS_BUILD_ID_MAX 64u
#endif
#ifndef LS_POLICY_CAPACITY
#define LS_POLICY_CAPACITY 16u
#endif
#ifndef LS_DEDUP_CAPACITY
#define LS_DEDUP_CAPACITY 16u
#endif
#ifndef LS_DEDUP_WINDOW_MS
#define LS_DEDUP_WINDOW_MS 60000u
#endif
#ifndef LS_BLACKBOX_CAPACITY
#define LS_BLACKBOX_CAPACITY 64u
#endif
#ifndef LS_BLACKBOX_ENVELOPE_MAX
#define LS_BLACKBOX_ENVELOPE_MAX 24u
#endif
#ifndef LS_SELFTEST_CAPACITY
#define LS_SELFTEST_CAPACITY 16u
#endif
#ifndef LS_FAULT_INJECTION_CAPACITY
#define LS_FAULT_INJECTION_CAPACITY 16u
#endif
#ifndef LS_MISSION_ID_MAX
#define LS_MISSION_ID_MAX 32u
#endif
#ifndef LS_DIVE_ID_MAX
#define LS_DIVE_ID_MAX 24u
#endif
#ifndef LS_NODE_ID_MAX
#define LS_NODE_ID_MAX 24u
#endif
#ifndef LS_VEHICLE_MODE_MAX
#define LS_VEHICLE_MODE_MAX 20u
#endif

#if LS_BLACKBOX_ENVELOPE_MAX > LS_BLACKBOX_CAPACITY
#error "LS_BLACKBOX_ENVELOPE_MAX cannot exceed LS_BLACKBOX_CAPACITY"
#endif

#define LS_PROTOCOL_VERSION 1u
#define LS_FORMAT_VERSION 2u
#define LS_MAGIC 0x5054534Cu
#define LS_STORAGE_MAGIC 0x4C53534Cu
#endif
