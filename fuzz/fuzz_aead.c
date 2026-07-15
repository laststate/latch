#include <stddef.h>
#include <stdint.h>
#include "laststate/security.h"
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 56u)
        return 0;
    const uint8_t *key = data, *nonce = data + 32;
    size_t aad_length = (size_t)(data[55] & 31u);
    if (56u + aad_length > size)
        return 0;
    const uint8_t *aad = data + 56, *plaintext = aad + aad_length;
    size_t length = size - 56u - aad_length;
    if (length > 4096u)
        return 0;
    uint8_t ciphertext[4096], opened[4096], tag[16];
    if (ls_xchacha20_poly1305_encrypt(key, nonce, aad, aad_length, plaintext, ciphertext, length,
                                      tag) != LS_OK)
        __builtin_trap();
    if (ls_xchacha20_poly1305_decrypt(key, nonce, aad, aad_length, ciphertext, opened, length,
                                      tag) != LS_OK)
        __builtin_trap();
    for (size_t i = 0; i < length; i++)
        if (opened[i] != plaintext[i])
            __builtin_trap();
    if (length) {
        ciphertext[length / 2] ^= 1u;
        if (ls_xchacha20_poly1305_decrypt(key, nonce, aad, aad_length, ciphertext, opened, length,
                                          tag) != LS_EAUTH)
            __builtin_trap();
    }
    return 0;
}
