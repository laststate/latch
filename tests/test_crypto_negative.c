#include <stdio.h>
#include <string.h>
#include "laststate/security.h"
static ls_result_t fail_random(void *context, uint8_t *output, size_t length) {
    (void)context;
    if (output) memset(output, 0xA5, length);
    return LS_EIO;
}

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "crypto negative failed: %s:%d\n", #x, __LINE__);                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    uint8_t key[32] = {0}, nonce[24] = {0}, aad[7] = {1, 2, 3, 4, 5, 6, 7}, plain[65], cipher[65],
            opened[65], tag[16];
    for (unsigned i = 0; i < sizeof plain; i++)
        plain[i] = (uint8_t)i;
    static const size_t lengths[] = {0, 1, 15, 16, 17, 31, 32, 63, 64, 65};
    for (size_t n = 0; n < sizeof lengths / sizeof lengths[0]; n++) {
        size_t length = lengths[n];
        nonce[23] = (uint8_t)n;
        CHECK(ls_xchacha20_poly1305_encrypt(key, nonce, aad, sizeof aad, plain, cipher, length,
                                            tag) == LS_OK);
        memset(opened, 0x5a, sizeof opened);
        CHECK(ls_xchacha20_poly1305_decrypt(key, nonce, aad, sizeof aad, cipher, opened, length,
                                            tag) == LS_OK);
        CHECK(!memcmp(opened, plain, length));
        tag[15] ^= 1u;
        memset(opened, 0x5a, sizeof opened);
        CHECK(ls_xchacha20_poly1305_decrypt(key, nonce, aad, sizeof aad, cipher, opened, length,
                                            tag) == LS_EAUTH);
        for (size_t i = 0; i < sizeof opened; i++)
            CHECK(opened[i] == 0x5a);
        tag[15] ^= 1u;
    }
    nonce[23] = 0;
    CHECK(ls_xchacha20_poly1305_encrypt(key, nonce, aad, sizeof aad, plain, cipher, sizeof cipher,
                                        tag) == LS_OK);
    cipher[32] ^= 0x80u;
    CHECK(ls_xchacha20_poly1305_decrypt(key, nonce, aad, sizeof aad, cipher, opened, sizeof opened,
                                        tag) == LS_EAUTH);
    cipher[32] ^= 0x80u;
    aad[0] ^= 1u;
    CHECK(ls_xchacha20_poly1305_decrypt(key, nonce, aad, sizeof aad, cipher, opened, sizeof opened,
                                        tag) == LS_EAUTH);
    aad[0] ^= 1u;
    CHECK(ls_xchacha20_poly1305_decrypt(key, nonce, aad, sizeof aad, cipher, opened, sizeof opened,
                                        tag) == LS_OK);
    CHECK(!memcmp(opened, plain, sizeof plain));

    /* Defensive API validation and the counter-space limit must fail before
       touching caller buffers. */
    uint8_t nonce12[12] = {0};
    uint8_t one = 0;
    CHECK(ls_chacha20_poly1305_encrypt(NULL, nonce12, NULL, 0, NULL, NULL, 0, tag) == LS_EINVAL);
    CHECK(ls_chacha20_poly1305_encrypt(key, NULL, NULL, 0, NULL, NULL, 0, tag) == LS_EINVAL);
    CHECK(ls_chacha20_poly1305_encrypt(key, nonce12, NULL, 0, NULL, NULL, 0, NULL) == LS_EINVAL);
    CHECK(ls_chacha20_poly1305_encrypt(key, nonce12, NULL, 1, &one, &one, 1, tag) == LS_EINVAL);
    CHECK(ls_chacha20_poly1305_encrypt(key, nonce12, NULL, 0, NULL, &one, 1, tag) == LS_EINVAL);
    CHECK(ls_chacha20_poly1305_encrypt(key, nonce12, NULL, 0, &one, NULL, 1, tag) == LS_EINVAL);
    CHECK(ls_chacha20_poly1305_encrypt(key, nonce12, NULL, 0, NULL, NULL, 0, tag) == LS_OK);
    CHECK(ls_chacha20_poly1305_decrypt(NULL, nonce12, NULL, 0, NULL, NULL, 0, tag) == LS_EINVAL);
    CHECK(ls_chacha20_poly1305_decrypt(key, NULL, NULL, 0, NULL, NULL, 0, tag) == LS_EINVAL);
    CHECK(ls_chacha20_poly1305_decrypt(key, nonce12, NULL, 0, NULL, NULL, 0, NULL) == LS_EINVAL);
    CHECK(ls_chacha20_poly1305_decrypt(key, nonce12, NULL, 1, &one, &one, 1, tag) == LS_EINVAL);
    CHECK(ls_chacha20_poly1305_decrypt(key, nonce12, NULL, 0, NULL, &one, 1, tag) == LS_EINVAL);
    CHECK(ls_xchacha20_poly1305_encrypt(NULL, nonce, NULL, 0, NULL, NULL, 0, tag) == LS_EINVAL);
    CHECK(ls_xchacha20_poly1305_encrypt(key, NULL, NULL, 0, NULL, NULL, 0, tag) == LS_EINVAL);
    CHECK(ls_xchacha20_poly1305_decrypt(NULL, nonce, NULL, 0, NULL, NULL, 0, tag) == LS_EINVAL);
    CHECK(ls_xchacha20_poly1305_decrypt(key, NULL, NULL, 0, NULL, NULL, 0, tag) == LS_EINVAL);
#if SIZE_MAX > UINT32_MAX
    size_t too_large = (size_t)UINT32_MAX * 64u + 1u;
    CHECK(ls_chacha20_poly1305_encrypt(key, nonce12, NULL, 0, &one, &one, too_large, tag) == LS_EOVERFLOW);
    CHECK(ls_chacha20_poly1305_decrypt(key, nonce12, NULL, 0, &one, &one, too_large, tag) == LS_EOVERFLOW);
#endif

    uint8_t long_key[100];
    uint8_t long_data[60];
    uint8_t digest[32];
    memset(long_key, 0x42, sizeof long_key);
    memset(long_data, 0x24, sizeof long_data);
    ls_hmac_sha256(long_key, sizeof long_key, long_data, sizeof long_data, digest);
    ls_hmac_sha256(NULL, 0, NULL, 0, digest);
    CHECK(ls_hkdf_sha256(NULL, 1, key, sizeof key, NULL, 0, opened, 1) == LS_EINVAL);
    CHECK(ls_hkdf_sha256(NULL, 0, NULL, 1, NULL, 0, opened, 1) == LS_EINVAL);
    CHECK(ls_hkdf_sha256(NULL, 0, key, sizeof key, NULL, 1, opened, 1) == LS_EINVAL);
    CHECK(ls_hkdf_sha256(NULL, 0, key, sizeof key, NULL, 0, NULL, 1) == LS_EINVAL);
    CHECK(ls_hkdf_sha256(NULL, 0, key, sizeof key, long_key, LS_HKDF_MAX_INFO_SIZE + 1u, opened, 1) == LS_EINVAL);
    CHECK(ls_hkdf_sha256(NULL, 0, key, sizeof key, NULL, 0, opened, 255u * 32u + 1u) == LS_EINVAL);
    CHECK(ls_hkdf_sha256(NULL, 0, key, sizeof key, NULL, 0, opened, 0) == LS_OK);
    ls_secure_zero(NULL, 8);
    CHECK(ls_constant_time_equal(NULL, NULL, 0));
    CHECK(!ls_constant_time_equal(NULL, &one, 1));
    CHECK(ls_constant_time_equal(&one, &one, 1));
    uint8_t two = 2; CHECK(!ls_constant_time_equal(&one, &two, 1));
    CHECK(ls_security_set_key(NULL, LS_SECURITY_KEY_SIZE) == LS_EINVAL);
    ls_security_policy_t policy = {0};
    CHECK(ls_security_set_policy(NULL) == LS_EINVAL);
    policy.algorithm = (ls_security_algorithm_t)99; policy.key_id = 1;
    CHECK(ls_security_set_policy(&policy) == LS_EINVAL);
    policy.algorithm = LS_SECURITY_XCHACHA20_POLY1305; policy.key_id = 0;
    CHECK(ls_security_set_policy(&policy) == LS_EINVAL);
    policy.algorithm = LS_SECURITY_HMAC_SHA256; policy.key_id = 1; policy.allow_legacy_hmac = false;
    CHECK(ls_security_set_policy(&policy) == LS_EINVAL);
    policy.allow_legacy_hmac = true; CHECK(ls_security_set_policy(&policy) == LS_OK);
    ls_security_set_random_provider(NULL, NULL);
    CHECK(ls_security_random(opened, 1) == LS_ENOTSUP);
    CHECK(ls_security_random(NULL, 1) == LS_ENOTSUP);
    ls_security_set_random_provider(fail_random, NULL);
    memset(opened, 0x5A, sizeof opened);
    CHECK(ls_security_random(opened, sizeof opened) == LS_EIO);
    for (size_t i = 0; i < sizeof opened; ++i) CHECK(opened[i] == 0);
    CHECK(ls_security_random(NULL, 0) == LS_EIO);
    CHECK(ls_security_set_key(key, 16) == LS_EINVAL);
    CHECK(ls_hkdf_sha256(0, 0, key, sizeof key, 0, 0, opened, 33) == LS_OK);
    return 0;
}
