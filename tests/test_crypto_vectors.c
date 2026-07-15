#include <stdio.h>
#include <string.h>
#include "laststate/security.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "crypto vector failed: %s:%d\n", #x, __LINE__);                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int hex(const uint8_t *data, const char *expected, size_t length) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < length; i++)
        if (digits[data[i] >> 4] != expected[i * 2] || digits[data[i] & 15] != expected[i * 2 + 1])
            return 0;
    return 1;
}
int main(void) {
    uint8_t poly_key[32] = {0x85, 0xd6, 0xbe, 0x78, 0x57, 0x55, 0x6d, 0x33, 0x7f, 0x44, 0x52,
                            0xfe, 0x42, 0xd5, 0x06, 0xa8, 0x01, 0x03, 0x80, 0x8a, 0xfb, 0x0d,
                            0xb2, 0xfd, 0x4a, 0xbf, 0xf6, 0xaf, 0x41, 0x49, 0xf5, 0x1b},
            tag[16];
    ls_poly1305_auth(poly_key, (const uint8_t *)"Cryptographic Forum Research Group", 34, tag);
    CHECK(hex(tag, "a8061dc1305136c6c22b8baf0c0127a9", 16));
    uint8_t ikm[22], salt[13], info[10], okm[42];
    memset(ikm, 0x0b, sizeof ikm);
    for (unsigned i = 0; i < 13; i++)
        salt[i] = (uint8_t)i;
    for (unsigned i = 0; i < 10; i++)
        info[i] = (uint8_t)(0xf0u + i);
    CHECK(ls_hkdf_sha256(salt, sizeof salt, ikm, sizeof ikm, info, sizeof info, okm, sizeof okm) ==
          LS_OK);
    CHECK(hex(
        okm, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865",
        42));
    uint8_t key[32], nonce[12] = {0x07, 0, 0, 0, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47};
    for (unsigned i = 0; i < 32; i++)
        key[i] = (uint8_t)(0x80u + i);
    uint8_t aad[12] = {0x50, 0x51, 0x52, 0x53, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7};
    static const uint8_t plain[] = "Ladies and Gentlemen of the class of '99: If I could offer you "
                                   "only one tip for the future, sunscreen would be it.";
    uint8_t cipher[sizeof plain - 1], opened[sizeof plain - 1];
    CHECK(ls_chacha20_poly1305_encrypt(key, nonce, aad, sizeof aad, plain, cipher, sizeof cipher,
                                       tag) == LS_OK);
    CHECK(hex(cipher,
              "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fa"
              "fb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4"
              "fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116",
              sizeof cipher));
    CHECK(hex(tag, "1ae10b594f09e26a7e902ecbd0600691", 16));
    CHECK(ls_chacha20_poly1305_decrypt(key, nonce, aad, sizeof aad, cipher, opened, sizeof cipher,
                                       tag) == LS_OK);
    CHECK(!memcmp(opened, plain, sizeof cipher));
    uint8_t hnonce[16] = {0};
    for (unsigned i = 0; i < 16; i++)
        hnonce[i] = (uint8_t)i;
    for (unsigned i = 0; i < 32; i++)
        key[i] = (uint8_t)i;
    uint8_t subkey[32];
    ls_hchacha20(key, hnonce, subkey);
    CHECK(hex(subkey, "51e3ff45a895675c4b33b46c64f4a9ace110d34df6a2ceab486372bacbd3eff6", 32));
    return 0;
}
