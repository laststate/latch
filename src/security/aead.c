#include "../core/internal.h"
#include "crypto_internal.h"
#include "laststate/security.h"

static void store64_le(uint8_t output[8], uint64_t value) {
    for (unsigned i = 0; i < 8; i++)
        output[i] = (uint8_t)(value >> (i * 8u));
}
static void poly_pad16(ls_poly1305_context_t *context, size_t length) {
    static const uint8_t zero[16] = {0};
    size_t remainder = length & 15u;
    if (remainder)
        ls_poly1305_update(context, zero, 16u - remainder);
}
static bool chacha20_length_overflows(size_t length) {
#if SIZE_MAX > UINT32_MAX
    return (uint64_t)length > UINT64_C(0xffffffff) * 64u;
#else
    (void)length;
    return false;
#endif
}
static void calculate_tag(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad,
                          size_t aad_length, const uint8_t *ciphertext, size_t length,
                          uint8_t tag[16]) {
    uint8_t first_block[64], lengths[16];
    ls_poly1305_context_t poly;
    ls_chacha20_xor(key, nonce, 0, 0, first_block, sizeof first_block);
    ls_poly1305_init(&poly, first_block);
    if (aad_length)
        ls_poly1305_update(&poly, aad, aad_length);
    poly_pad16(&poly, aad_length);
    if (length)
        ls_poly1305_update(&poly, ciphertext, length);
    poly_pad16(&poly, length);
    store64_le(lengths, (uint64_t)aad_length);
    store64_le(lengths + 8, (uint64_t)length);
    ls_poly1305_update(&poly, lengths, sizeof lengths);
    ls_poly1305_finish(&poly, tag);
    ls_secure_zero(first_block, sizeof first_block);
    ls_secure_zero(lengths, sizeof lengths);
}
ls_result_t ls_chacha20_poly1305_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                                         const uint8_t *aad, size_t aad_length,
                                         const uint8_t *plaintext, uint8_t *ciphertext,
                                         size_t length, uint8_t tag[16]) {
    if (!key || !nonce || !tag || (!aad && aad_length) || (!plaintext && length) ||
        (!ciphertext && length))
        return LS_EINVAL;
    if (chacha20_length_overflows(length))
        return LS_EOVERFLOW;
    if (length)
        ls_chacha20_xor(key, nonce, 1, plaintext, ciphertext, length);
    calculate_tag(key, nonce, aad, aad_length, ciphertext, length, tag);
    return LS_OK;
}
ls_result_t ls_chacha20_poly1305_decrypt(const uint8_t key[32], const uint8_t nonce[12],
                                         const uint8_t *aad, size_t aad_length,
                                         const uint8_t *ciphertext, uint8_t *plaintext,
                                         size_t length, const uint8_t tag[16]) {
    uint8_t expected[16];
    if (!key || !nonce || !tag || (!aad && aad_length) || (!ciphertext && length))
        return LS_EINVAL;
    if (chacha20_length_overflows(length))
        return LS_EOVERFLOW;
    calculate_tag(key, nonce, aad, aad_length, ciphertext, length, expected);
    bool valid = ls_constant_time_equal(expected, tag, sizeof expected);
    ls_secure_zero(expected, sizeof expected);
    if (!valid)
        return LS_EAUTH;
    if (plaintext && length)
        ls_chacha20_xor(key, nonce, 1, ciphertext, plaintext, length);
    return LS_OK;
}
static void xchacha_material(const uint8_t key[32], const uint8_t nonce[24], uint8_t subkey[32],
                             uint8_t subnonce[12]) {
    ls_hchacha20(key, nonce, subkey);
    ls_memset(subnonce, 0, 4);
    ls_memcpy(subnonce + 4, nonce + 16, 8);
}
ls_result_t ls_xchacha20_poly1305_encrypt(const uint8_t key[32], const uint8_t nonce[24],
                                          const uint8_t *aad, size_t aad_length,
                                          const uint8_t *plaintext, uint8_t *ciphertext,
                                          size_t length, uint8_t tag[16]) {
    uint8_t subkey[32], subnonce[12];
    if (!key || !nonce)
        return LS_EINVAL;
    xchacha_material(key, nonce, subkey, subnonce);
    ls_result_t result = ls_chacha20_poly1305_encrypt(subkey, subnonce, aad, aad_length, plaintext,
                                                      ciphertext, length, tag);
    ls_secure_zero(subkey, sizeof subkey);
    ls_secure_zero(subnonce, sizeof subnonce);
    return result;
}
ls_result_t ls_xchacha20_poly1305_decrypt(const uint8_t key[32], const uint8_t nonce[24],
                                          const uint8_t *aad, size_t aad_length,
                                          const uint8_t *ciphertext, uint8_t *plaintext,
                                          size_t length, const uint8_t tag[16]) {
    uint8_t subkey[32], subnonce[12];
    if (!key || !nonce)
        return LS_EINVAL;
    xchacha_material(key, nonce, subkey, subnonce);
    ls_result_t result = ls_chacha20_poly1305_decrypt(subkey, subnonce, aad, aad_length, ciphertext,
                                                      plaintext, length, tag);
    ls_secure_zero(subkey, sizeof subkey);
    ls_secure_zero(subnonce, sizeof subnonce);
    return result;
}
