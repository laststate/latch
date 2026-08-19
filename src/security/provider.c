// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// src/security/provider.c
//
// Cryptography provider dispatch. Selects between the in-tree
// software provider and a port-supplied secure element at
// ls_init(); never falls back to a weaker PRNG.
//
// Heap-free, bounded, deterministic.

#include "../core/internal.h"
#include "laststate/security.h"

static bool assurance_meets_policy(ls_crypto_assurance_t assurance) {
#if LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER
    return (int)assurance >= (int)LS_MIN_CRYPTO_ASSURANCE;
#else
    (void)assurance;
    return true;
#endif
}

static bool bytes_equal(const uint8_t *a, const uint8_t *b, size_t n) {
    uint8_t diff = 0u;
    for (size_t i = 0; i < n; ++i)
        diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0u;
}

static ls_result_t provider_known_answer_test(const ls_crypto_provider_t *provider) {
    static const uint8_t hkdf_expected[42] = {
        0x3a, 0x86, 0xcb, 0x5c, 0x29, 0xf9, 0x3d, 0x75, 0xc0, 0x47, 0x72, 0x15, 0x44, 0x3d,
        0x0f, 0x87, 0x50, 0x4b, 0x49, 0x3c, 0x92, 0x6f, 0xa3, 0x7c, 0xb0, 0xa3, 0x16, 0x7c,
        0xee, 0xbf, 0x28, 0xef, 0x0a, 0xe4, 0x39, 0x67, 0x1b, 0xc2, 0xe0, 0xe2, 0x4b, 0xa2};
    static const uint8_t ciphertext_expected[14] = {0xfd, 0xad, 0x62, 0x12, 0xf5, 0xa0, 0xee,
                                                    0xc7, 0x52, 0x28, 0x0b, 0xaf, 0xbe, 0x24};
    static const uint8_t tag_expected[16] = {0x1a, 0x9c, 0x63, 0xdf, 0xbc, 0xbe, 0xea, 0x1e,
                                             0x45, 0x8a, 0x47, 0x90, 0xd7, 0xb5, 0x9d, 0xf1};
    static const uint8_t aad[] = "Latch provider KAT";
    static const uint8_t plaintext[] = "commercial-auv";
    uint8_t salt[13], ikm[32], key[32], nonce[24];
    uint8_t hkdf[42], ciphertext[14], tag[16], recovered[14];
    for (size_t i = 0; i < sizeof salt; ++i)
        salt[i] = (uint8_t)i;
    for (size_t i = 0; i < sizeof ikm; ++i)
        ikm[i] = (uint8_t)(i + 32u);
    for (size_t i = 0; i < sizeof key; ++i)
        key[i] = (uint8_t)i;
    for (size_t i = 0; i < sizeof nonce; ++i)
        nonce[i] = (uint8_t)i;
    ls_result_t r =
        provider->hkdf_sha256(provider->context, salt, sizeof salt, ikm, sizeof ikm,
                              (const uint8_t *)"Latch HKDF KAT", 14u, hkdf, sizeof hkdf);
    if (r != LS_OK || !bytes_equal(hkdf, hkdf_expected, sizeof hkdf))
        return LS_EAUTH;
    r = provider->xchacha20_poly1305_encrypt(provider->context, key, nonce, aad, sizeof aad - 1u,
                                             plaintext, ciphertext, sizeof plaintext - 1u, tag);
    if (r != LS_OK || !bytes_equal(ciphertext, ciphertext_expected, sizeof ciphertext) ||
        !bytes_equal(tag, tag_expected, sizeof tag))
        return LS_EAUTH;
    r = provider->xchacha20_poly1305_decrypt(provider->context, key, nonce, aad, sizeof aad - 1u,
                                             ciphertext, recovered, sizeof recovered, tag);
    if (r != LS_OK || !bytes_equal(recovered, plaintext, sizeof recovered))
        return LS_EAUTH;
    tag[0] ^= 1u;
    r = provider->xchacha20_poly1305_decrypt(provider->context, key, nonce, aad, sizeof aad - 1u,
                                             ciphertext, recovered, sizeof recovered, tag);
    ls_secure_zero(hkdf, sizeof hkdf);
    ls_secure_zero(recovered, sizeof recovered);
    ls_secure_zero(ciphertext, sizeof ciphertext);
    ls_secure_zero(tag, sizeof tag);
    return r == LS_EAUTH ? LS_OK : LS_EAUTH;
}

ls_result_t ls_security_set_crypto_provider(const ls_crypto_provider_t *provider) {
    if (!provider || !provider->name || !provider->name[0] || !provider->version ||
        !provider->version[0] || !provider->audit_reference || !provider->audit_reference[0] ||
        !provider->hkdf_sha256 || !provider->xchacha20_poly1305_encrypt ||
        !provider->xchacha20_poly1305_decrypt)
        return LS_EINVAL;
    if (!assurance_meets_policy(provider->assurance))
        return LS_EAUTH;
    ls_result_t kat = provider_known_answer_test(provider);
    if (kat != LS_OK)
        return kat;
    ls_runtime.crypto_provider = *provider;
    ls_runtime.has_crypto_provider = true;
    return LS_OK;
}

void ls_security_clear_crypto_provider(void) {
    ls_secure_zero(&ls_runtime.crypto_provider, sizeof ls_runtime.crypto_provider);
    ls_runtime.has_crypto_provider = false;
}

const char *ls_security_crypto_provider_name(void) {
    return ls_runtime.has_crypto_provider ? ls_runtime.crypto_provider.name : "builtin-unqualified";
}

ls_crypto_assurance_t ls_security_crypto_provider_assurance(void) {
    return ls_runtime.has_crypto_provider ? ls_runtime.crypto_provider.assurance
                                          : LS_CRYPTO_ASSURANCE_TEST_ONLY;
}

const char *ls_security_crypto_provider_audit_reference(void) {
    return ls_runtime.has_crypto_provider ? ls_runtime.crypto_provider.audit_reference : "none";
}

bool ls_security_crypto_provider_ready(void) {
#if LS_REQUIRE_EXTERNAL_CRYPTO_PROVIDER
    return ls_runtime.has_crypto_provider &&
           assurance_meets_policy(ls_runtime.crypto_provider.assurance);
#else
    return true;
#endif
}

static ls_result_t require_provider_if_configured(void) {
    return ls_security_crypto_provider_ready() ? LS_OK : LS_EAUTH;
}

ls_result_t ls_crypto_hkdf_sha256(const uint8_t *salt, size_t salt_length, const uint8_t *ikm,
                                  size_t ikm_length, const uint8_t *info, size_t info_length,
                                  uint8_t *output, size_t output_length) {
    ls_result_t gate = require_provider_if_configured();
    if (gate != LS_OK)
        return gate;
    if (ls_runtime.has_crypto_provider)
        return ls_runtime.crypto_provider.hkdf_sha256(ls_runtime.crypto_provider.context, salt,
                                                      salt_length, ikm, ikm_length, info,
                                                      info_length, output, output_length);
    return ls_hkdf_sha256(salt, salt_length, ikm, ikm_length, info, info_length, output,
                          output_length);
}

ls_result_t ls_crypto_xchacha20_poly1305_encrypt(const uint8_t key[32], const uint8_t nonce[24],
                                                 const uint8_t *aad, size_t aad_length,
                                                 const uint8_t *plaintext, uint8_t *ciphertext,
                                                 size_t length, uint8_t tag[16]) {
    ls_result_t gate = require_provider_if_configured();
    if (gate != LS_OK)
        return gate;
    if (ls_runtime.has_crypto_provider)
        return ls_runtime.crypto_provider.xchacha20_poly1305_encrypt(
            ls_runtime.crypto_provider.context, key, nonce, aad, aad_length, plaintext, ciphertext,
            length, tag);
    return ls_xchacha20_poly1305_encrypt(key, nonce, aad, aad_length, plaintext, ciphertext, length,
                                         tag);
}

ls_result_t ls_crypto_xchacha20_poly1305_decrypt(const uint8_t key[32], const uint8_t nonce[24],
                                                 const uint8_t *aad, size_t aad_length,
                                                 const uint8_t *ciphertext, uint8_t *plaintext,
                                                 size_t length, const uint8_t tag[16]) {
    ls_result_t gate = require_provider_if_configured();
    if (gate != LS_OK)
        return gate;
    if (ls_runtime.has_crypto_provider)
        return ls_runtime.crypto_provider.xchacha20_poly1305_decrypt(
            ls_runtime.crypto_provider.context, key, nonce, aad, aad_length, ciphertext, plaintext,
            length, tag);
    return ls_xchacha20_poly1305_decrypt(key, nonce, aad, aad_length, ciphertext, plaintext, length,
                                         tag);
}
