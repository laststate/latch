#include <stdio.h>
#include <string.h>
#include "laststate/security.h"
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
    CHECK(ls_security_set_key(key, 16) == LS_EINVAL);
    CHECK(ls_hkdf_sha256(0, 0, key, sizeof key, 0, 0, opened, 33) == LS_OK);
    return 0;
}
