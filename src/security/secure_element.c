// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// src/security/secure_element.c
//
// Secure-element adapter glue. Forwards every key, AEAD, and
// random call to the port when a hardware element is present;
// returns ENOTSUP otherwise.
//
// Heap-free, bounded, deterministic.

#include "laststate/secure_element.h"
ls_result_t ls_secure_element_validate(const ls_secure_element_t *element) {
    return element && element->random && element->sign_sha256 && element->read_certificate
               ? LS_OK
               : LS_EINVAL;
}
ls_result_t ls_secure_element_random(void *context, uint8_t *output, size_t length) {
    ls_secure_element_t *element = (ls_secure_element_t *)context;
    if (!element || !element->random || (!output && length))
        return LS_EINVAL;
    return element->random(element->context, output, length);
}
ls_result_t ls_secure_element_sign(const ls_secure_element_t *element, const uint8_t digest[32],
                                   uint8_t signature[64]) {
    if (!element || !element->sign_sha256 || !digest || !signature)
        return LS_EINVAL;
    return element->sign_sha256(element->context, digest, signature);
}
ls_result_t ls_secure_element_certificate(const ls_secure_element_t *element, uint8_t *output,
                                          size_t capacity, size_t *length) {
    if (!element || !element->read_certificate || !output || !capacity || !length)
        return LS_EINVAL;
    return element->read_certificate(element->context, output, capacity, length);
}
ls_result_t ls_secure_element_derive(const ls_secure_element_t *element, uint32_t key_id,
                                     const uint8_t *context_data, size_t context_length,
                                     uint8_t output[32]) {
    if (!element || !element->derive_key || (!context_data && context_length) || !output || !key_id)
        return LS_EINVAL;
    return element->derive_key(element->context, key_id, context_data, context_length, output);
}

ls_result_t ls_secure_element_destroy_key(const ls_secure_element_t *element, uint32_t key_id) {
    if (!element || !key_id)
        return LS_EINVAL;
    if (!element->destroy_key)
        return LS_ENOTSUP;
    return element->destroy_key(element->context, key_id);
}
