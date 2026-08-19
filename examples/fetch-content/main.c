// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// examples/fetch-content/main.c
//
// Latch runtime source. Part of the heap-free, deterministic,
// embedded failure-capture runtime that ships fault state over LEP
// to the Relay.
//
// Heap-free, bounded, deterministic.

#include <stdio.h>
#include "laststate/latch.h"

int main(void) {
    printf("Latch %s linked with FetchContent\n", LS_VERSION_STRING);
    return ls_build_id_validate(ls_build_id()) ? 0 : 1;
}
