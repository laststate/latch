// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// tests/test_libsodium_provider.c
//
// libsodium provider interop tests. Validates Sodium-backed
// crypto against in-tree implementation.
//
// Heap-free, bounded, deterministic.

#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#include "libsodium_provider.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "libsodium provider failed: %s:%d\n", #x, __LINE__);                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    ls_crypto_provider_t provider = ls_libsodium_crypto_provider();
    CHECK(provider.assurance == LS_CRYPTO_ASSURANCE_EXTERNAL_AUDITED);
    CHECK(provider.audit_reference && provider.audit_reference[0]);
    CHECK(ls_security_set_crypto_provider(&provider) == LS_OK);
    CHECK(strcmp(ls_security_crypto_provider_name(), "libsodium") == 0);
    CHECK(ls_security_crypto_provider_ready());
    ls_security_clear_crypto_provider();
    return 0;
}
