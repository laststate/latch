#include "../core/internal.h"
#include "laststate/security.h"

static uint32_t load32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void store32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}
static uint32_t rotate(uint32_t value, unsigned count) {
    return (value << count) | (value >> (32u - count));
}
#define QUARTER(a, b, c, d)                                                                        \
    do {                                                                                           \
        x[a] += x[b];                                                                              \
        x[d] ^= x[a];                                                                              \
        x[d] = rotate(x[d], 16);                                                                   \
        x[c] += x[d];                                                                              \
        x[b] ^= x[c];                                                                              \
        x[b] = rotate(x[b], 12);                                                                   \
        x[a] += x[b];                                                                              \
        x[d] ^= x[a];                                                                              \
        x[d] = rotate(x[d], 8);                                                                    \
        x[c] += x[d];                                                                              \
        x[b] ^= x[c];                                                                              \
        x[b] = rotate(x[b], 7);                                                                    \
    } while (0)
static void rounds(uint32_t x[16]) {
    for (unsigned round = 0; round < 10; round++) {
        QUARTER(0, 4, 8, 12);
        QUARTER(1, 5, 9, 13);
        QUARTER(2, 6, 10, 14);
        QUARTER(3, 7, 11, 15);
        QUARTER(0, 5, 10, 15);
        QUARTER(1, 6, 11, 12);
        QUARTER(2, 7, 8, 13);
        QUARTER(3, 4, 9, 14);
    }
}
static void block(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter,
                  uint8_t output[64]) {
    static const uint32_t constants[4] = {0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u};
    uint32_t state[16], x[16];
    for (unsigned i = 0; i < 4; i++)
        state[i] = constants[i];
    for (unsigned i = 0; i < 8; i++)
        state[4 + i] = load32(key + i * 4);
    state[12] = counter;
    state[13] = load32(nonce);
    state[14] = load32(nonce + 4);
    state[15] = load32(nonce + 8);
    for (unsigned i = 0; i < 16; i++)
        x[i] = state[i];
    rounds(x);
    for (unsigned i = 0; i < 16; i++)
        store32(output + i * 4, x[i] + state[i]);
    ls_secure_zero(x, sizeof x);
    ls_secure_zero(state, sizeof state);
}
void ls_chacha20_xor(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter,
                     const uint8_t *input, uint8_t *output, size_t length) {
    while (length) {
        uint8_t stream[64];
        block(key, nonce, counter++, stream);
        size_t take = length > 64u ? 64u : length;
        for (size_t i = 0; i < take; i++)
            output[i] = (input ? input[i] : 0u) ^ stream[i];
        if (input)
            input += take;
        output += take;
        length -= take;
        ls_secure_zero(stream, sizeof stream);
    }
}
void ls_hchacha20(const uint8_t key[32], const uint8_t nonce[16], uint8_t subkey[32]) {
    static const uint32_t constants[4] = {0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u};
    uint32_t x[16];
    for (unsigned i = 0; i < 4; i++)
        x[i] = constants[i];
    for (unsigned i = 0; i < 8; i++)
        x[4 + i] = load32(key + i * 4);
    for (unsigned i = 0; i < 4; i++)
        x[12 + i] = load32(nonce + i * 4);
    rounds(x);
    store32(subkey, x[0]);
    store32(subkey + 4, x[1]);
    store32(subkey + 8, x[2]);
    store32(subkey + 12, x[3]);
    store32(subkey + 16, x[12]);
    store32(subkey + 20, x[13]);
    store32(subkey + 24, x[14]);
    store32(subkey + 28, x[15]);
    ls_secure_zero(x, sizeof x);
}
#undef QUARTER
