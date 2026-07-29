#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "compression check failed: %s:%d\n", #x, __LINE__);                    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint32_t random_state = 1;
static uint32_t random_value(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
int main(void) {
    const uint32_t cases[] = {0, 1, 127, 128, 255, 16384, 0x0fffffffu, 0xffffffffu};
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        uint8_t encoded[8];
        size_t written, consumed;
        uint32_t decoded;
        CHECK(ls_varint_u32_encode(cases[i], encoded, sizeof encoded, &written) == LS_OK);
        CHECK(ls_varint_u32_decode(encoded, written, &decoded, &consumed) == LS_OK);
        CHECK(decoded == cases[i] && consumed == written);
    }
    uint8_t input[1024], compressed[2048], output[1024];
    for (unsigned iteration = 0; iteration < 200; iteration++) {
        size_t length = random_value() % sizeof input;
        for (size_t i = 0; i < length; i++)
            input[i] = (random_value() % 5) ? (uint8_t)random_value() : 0;
        size_t compressed_length, output_length;
        CHECK(ls_rle_compress(input, length, compressed, sizeof compressed, &compressed_length) ==
              LS_OK);
        CHECK(ls_rle_decompress(compressed, compressed_length, output, sizeof output,
                                &output_length) == LS_OK);
        CHECK(output_length == length && !memcmp(input, output, length));
    }
    uint32_t values[256], decoded[256];
    for (size_t i = 0; i < 256; i++)
        values[i] = 100000u + (uint32_t)i * 3u + (i % 7u);
    size_t encoded_length, count;
    CHECK(ls_delta_u32_encode(values, 256, compressed, sizeof compressed, &encoded_length) ==
          LS_OK);
    CHECK(ls_delta_u32_decode(compressed, encoded_length, decoded, 256, &count) == LS_OK);
    CHECK(count == 256 && !memcmp(values, decoded, sizeof values));
    const uint32_t wrapping_values[] = {0x70000000u, 0xe0000000u, 0x10000000u};
    CHECK(ls_delta_u32_encode(wrapping_values, sizeof wrapping_values / sizeof wrapping_values[0],
                              compressed, sizeof compressed, &encoded_length) == LS_OK);
    CHECK(ls_delta_u32_decode(compressed, encoded_length, decoded,
                              sizeof wrapping_values / sizeof wrapping_values[0], &count) == LS_OK);
    CHECK(count == sizeof wrapping_values / sizeof wrapping_values[0] &&
          !memcmp(wrapping_values, decoded, sizeof wrapping_values));
    CHECK(ls_rle_decompress((uint8_t[]){0x82}, 1, output, sizeof output, &count) == LS_ECORRUPT);
    return 0;
}
