#include <sodium.h>
#include <stdio.h>
#include <string.h>
#include "laststate/security.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "sodium interop failed: %s:%d\n", #x, __LINE__);                       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    CHECK(sodium_init() >= 0);
    uint8_t key[32], nonce[24], aad[33], plain[257], ours[257], theirs[257], ours_tag[16],
        their_tag[16], opened[257];
    for (unsigned i = 0; i < 32; i++)
        key[i] = (uint8_t)(i * 3u + 1u);
    for (unsigned i = 0; i < 24; i++)
        nonce[i] = (uint8_t)(i * 5u + 7u);
    for (unsigned i = 0; i < 33; i++)
        aad[i] = (uint8_t)(i ^ 0xa5u);
    for (unsigned i = 0; i < 257; i++)
        plain[i] = (uint8_t)(i * 11u);
    static const size_t lengths[] = {0, 1, 15, 16, 17, 63, 64, 65, 255, 256, 257};
    for (size_t n = 0; n < sizeof lengths / sizeof lengths[0]; n++) {
        unsigned long long mac_length = 0;
        nonce[23] = (uint8_t)n;
        CHECK(ls_xchacha20_poly1305_encrypt(key, nonce, aad, sizeof aad, plain, ours, lengths[n],
                                            ours_tag) == LS_OK);
        CHECK(crypto_aead_xchacha20poly1305_ietf_encrypt_detached(
                  theirs, their_tag, &mac_length, plain, lengths[n], aad, sizeof aad, NULL, nonce,
                  key) == 0);
        CHECK(mac_length == 16);
        CHECK(!memcmp(ours, theirs, lengths[n]));
        CHECK(!memcmp(ours_tag, their_tag, 16));
        CHECK(ls_xchacha20_poly1305_decrypt(key, nonce, aad, sizeof aad, theirs, opened, lengths[n],
                                            their_tag) == LS_OK);
        CHECK(!memcmp(opened, plain, lengths[n]));
    }
    return 0;
}
