// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// src/security/crypto_internal.h
//
// Internal cryptography header shared by src/security/. Not
// installed; declares the in-tree primitives and the provider
// dispatch table used at runtime.
//
// Heap-free, bounded, deterministic.

#ifndef LS_CRYPTO_INTERNAL_H
#define LS_CRYPTO_INTERNAL_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t r[5], h[5], pad[4];
    uint8_t buffer[16];
    size_t leftover;
    uint8_t final;
} ls_poly1305_context_t;
void ls_poly1305_init(ls_poly1305_context_t *context, const uint8_t key[32]);
void ls_poly1305_update(ls_poly1305_context_t *context, const uint8_t *message, size_t length);
void ls_poly1305_finish(ls_poly1305_context_t *context, uint8_t tag[16]);
#endif
