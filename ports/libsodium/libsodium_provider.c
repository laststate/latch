#include "libsodium_provider.h"
#include <sodium.h>
#include <string.h>

static int sodium_ready(void) {
    return sodium_init() < 0 ? -1 : 0;
}

static ls_result_t sodium_hmac(const uint8_t *key, size_t key_len, const uint8_t *a, size_t a_len,
                               const uint8_t *b, size_t b_len, const uint8_t *c, size_t c_len,
                               uint8_t out[crypto_auth_hmacsha256_BYTES]) {
    crypto_auth_hmacsha256_state state;
    if (crypto_auth_hmacsha256_init(&state, key, key_len) != 0)
        return LS_EIO;
    if (a_len && crypto_auth_hmacsha256_update(&state, a, (unsigned long long)a_len) != 0)
        return LS_EIO;
    if (b_len && crypto_auth_hmacsha256_update(&state, b, (unsigned long long)b_len) != 0)
        return LS_EIO;
    if (c_len && crypto_auth_hmacsha256_update(&state, c, (unsigned long long)c_len) != 0)
        return LS_EIO;
    return crypto_auth_hmacsha256_final(&state, out) == 0 ? LS_OK : LS_EIO;
}

static ls_result_t sodium_hkdf(void *ctx, const uint8_t *salt, size_t salt_len, const uint8_t *ikm,
                               size_t ikm_len, const uint8_t *info, size_t info_len, uint8_t *out,
                               size_t out_len) {
    (void)ctx;
    if (sodium_ready() != 0)
        return LS_EIO;
    if ((!salt && salt_len) || (!ikm && ikm_len) || (!info && info_len) || (!out && out_len) ||
        out_len > 255u * crypto_auth_hmacsha256_BYTES)
        return LS_EINVAL;
    uint8_t zero_salt[crypto_auth_hmacsha256_BYTES] = {0};
    uint8_t prk[crypto_auth_hmacsha256_BYTES], t[crypto_auth_hmacsha256_BYTES];
    const uint8_t *salt_key = salt_len ? salt : zero_salt;
    size_t salt_key_len = salt_len ? salt_len : sizeof zero_salt;
    ls_result_t r = sodium_hmac(salt_key, salt_key_len, ikm, ikm_len, NULL, 0u, NULL, 0u, prk);
    size_t written = 0u, t_len = 0u;
    uint8_t counter = 1u;
    while (r == LS_OK && written < out_len) {
        r = sodium_hmac(prk, sizeof prk, t, t_len, info, info_len, &counter, 1u, t);
        if (r != LS_OK)
            break;
        size_t take = out_len - written;
        if (take > sizeof t)
            take = sizeof t;
        memcpy(out + written, t, take);
        written += take;
        t_len = sizeof t;
        counter++;
    }
    sodium_memzero(prk, sizeof prk);
    sodium_memzero(t, sizeof t);
    sodium_memzero(zero_salt, sizeof zero_salt);
    return r;
}

static ls_result_t sodium_encrypt(void *ctx, const uint8_t key[32], const uint8_t nonce[24],
                                  const uint8_t *aad, size_t aad_len, const uint8_t *plaintext,
                                  uint8_t *ciphertext, size_t len, uint8_t tag[16]) {
    (void)ctx;
    if (sodium_ready() != 0)
        return LS_EIO;
    unsigned long long maclen = 0;
    return crypto_aead_xchacha20poly1305_ietf_encrypt_detached(
               ciphertext, tag, &maclen, plaintext, (unsigned long long)len, aad,
               (unsigned long long)aad_len, NULL, nonce, key) == 0 &&
                   maclen == 16u
               ? LS_OK
               : LS_EIO;
}
static ls_result_t sodium_decrypt(void *ctx, const uint8_t key[32], const uint8_t nonce[24],
                                  const uint8_t *aad, size_t aad_len, const uint8_t *ciphertext,
                                  uint8_t *plaintext, size_t len, const uint8_t tag[16]) {
    (void)ctx;
    if (sodium_ready() != 0)
        return LS_EIO;
    return crypto_aead_xchacha20poly1305_ietf_decrypt_detached(
               plaintext, NULL, ciphertext, (unsigned long long)len, tag, aad,
               (unsigned long long)aad_len, nonce, key) == 0
               ? LS_OK
               : LS_EAUTH;
}
ls_crypto_provider_t ls_libsodium_crypto_provider(void) {
    ls_crypto_provider_t p = {
        .name = "libsodium",
        .version = sodium_version_string(),
        .audit_reference =
            "https://libsodium.gitbook.io/doc/roadmap (third-party security audit: DONE)",
        .assurance = LS_CRYPTO_ASSURANCE_EXTERNAL_AUDITED,
        .context = NULL,
        .hkdf_sha256 = sodium_hkdf,
        .xchacha20_poly1305_encrypt = sodium_encrypt,
        .xchacha20_poly1305_decrypt = sodium_decrypt};
    return p;
}
