// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/fingerprint.h
//
// Device fingerprint hashing. Combines MCU ID, build ID, and any
// DNA samples into a stable identifier the Relay can use to
// de-duplicate records from the same physical device.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_FINGERPRINT_H
#define LASTSTATE_FINGERPRINT_H

#include <stdint.h>

#include "event.h"

/* Stable diagnostic fingerprint for crash clustering. It deliberately uses
 * addresses/register state plus the build identity and is not a cryptographic
 * identifier. */
uint32_t ls_crash_fingerprint(const ls_arch_context_t *context);
uint32_t ls_event_fingerprint(const char *domain, int32_t code, uint32_t detail);

#endif
