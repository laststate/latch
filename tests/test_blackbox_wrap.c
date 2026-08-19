// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// tests/test_blackbox_wrap.c
//
// Latch runtime source. Part of the heap-free, deterministic,
// embedded failure-capture runtime that ships fault state over LEP
// to the Relay.
//
// Heap-free, bounded, deterministic.

#include <stdio.h>

#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "blackbox wrap check failed: %s:%d\n", #condition, __LINE__);          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    ls_blackbox_clear();
    for (int32_t value = 0; value < 4; ++value) {
        CHECK(ls_blackbox_record_values(LS_BLACKBOX_USER, 1u, 0u, value, 0, 0, 0) == LS_OK);
    }

    ls_blackbox_record_t records[4];
    size_t count = 0u;
    CHECK(ls_blackbox_copy(records, sizeof(records) / sizeof(records[0]), &count) == LS_OK);
    CHECK(count == sizeof(records) / sizeof(records[0]));
    for (size_t index = 0u; index < count; ++index) {
        CHECK(records[index].value[0] == (int32_t)index);
    }
    return 0;
}
