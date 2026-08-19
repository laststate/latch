// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// tests/package/linux.c
//
// Latch runtime source. Part of the heap-free, deterministic,
// embedded failure-capture runtime that ships fault state over LEP
// to the Relay.
//
// Heap-free, bounded, deterministic.

#include "file_backend.h"
#include "linux.h"

int main(void) {
    ls_file_storage_t storage = LS_FILE_STORAGE_INITIALIZER("/tmp/latch-store", 1u);

    return storage.fd != -1 || storage.initialized ||
                   LS_LINUX_SIGNAL_RECORD_REGISTER_CAPACITY != 32u
               ? 1
               : 0;
}
