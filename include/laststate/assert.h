// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/assert.h
//
// Assertion and fault-handling policy for Latch. Selects how an
// failed assertion is reported at runtime (continue, reset, halt,
// breakpoint, or custom callback).
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_ASSERT_H
#define LASTSTATE_ASSERT_H

#include "event.h"
typedef enum {
    LS_ASSERT_CONTINUE,
    LS_ASSERT_RESET,
    LS_ASSERT_HALT,
    LS_ASSERT_BREAKPOINT,
    LS_ASSERT_CALLBACK
} ls_assert_policy_t;
typedef void (*ls_assert_callback_t)(const char *expression, const char *file, int line);
void ls_assert_set_policy(ls_assert_policy_t policy, ls_assert_callback_t callback);
void ls_assert_failed(const char *expression, const char *file, int line, const char *message);
#endif
