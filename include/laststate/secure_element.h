// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// include/laststate/secure_element.h
//
// Secure-element adapter contract. The port supplies a small
// interface for key injection, AEAD, and true-random; Latch never
// embeds a fallback PRNG for cryptographic use.
//
// Heap-free, bounded, deterministic.

#ifndef LASTSTATE_SECURE_ELEMENT_H
#define LASTSTATE_SECURE_ELEMENT_H
#include <stddef.h>
#include <stdint.h>
#include "event.h"
typedef struct ls_secure_element {
    void *context;
    ls_result_t (*random)(void *context, uint8_t *output, size_t length);
    ls_result_t (*sign_sha256)(void *context, const uint8_t digest[32], uint8_t signature[64]);
    ls_result_t (*read_certificate)(void *context, uint8_t *output, size_t capacity,
                                    size_t *length);
    ls_result_t (*derive_key)(void *context, uint32_t key_id, const uint8_t *context_data,
                              size_t context_length, uint8_t output[32]);
    /* Optional irreversible product-specific destruction of a key slot/handle. */
    ls_result_t (*destroy_key)(void *context, uint32_t key_id);
} ls_secure_element_t;
ls_result_t ls_secure_element_validate(const ls_secure_element_t *element);
ls_result_t ls_secure_element_random(void *context, uint8_t *output, size_t length);
ls_result_t ls_secure_element_sign(const ls_secure_element_t *element, const uint8_t digest[32],
                                   uint8_t signature[64]);
ls_result_t ls_secure_element_certificate(const ls_secure_element_t *element, uint8_t *output,
                                          size_t capacity, size_t *length);
ls_result_t ls_secure_element_derive(const ls_secure_element_t *element, uint32_t key_id,
                                     const uint8_t *context_data, size_t context_length,
                                     uint8_t output[32]);
ls_result_t ls_secure_element_destroy_key(const ls_secure_element_t *element, uint32_t key_id);
#endif
