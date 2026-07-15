#include <stdio.h>
#include <string.h>
#include "laststate/compression.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "property check failed: %s:%d\n", #x, __LINE__);                       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint32_t state = 0x12345678u;
static uint32_t random_u32(void) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
int main(void) {
    static const uint32_t boundaries[] = {0,     1,     127,         128,        129,
                                          16383, 16384, 0x0fffffffu, 0xffffffffu};
    uint8_t encoded[2048], decoded[1024];
    size_t written, consumed, decoded_length;
    for (size_t i = 0; i < sizeof boundaries / sizeof boundaries[0]; i++) {
        uint32_t value = 0;
        CHECK(ls_varint_u32_encode(boundaries[i], encoded, sizeof encoded, &written) == LS_OK);
        CHECK(ls_varint_u32_decode(encoded, written, &value, &consumed) == LS_OK);
        CHECK(value == boundaries[i] && consumed == written);
    }
    for (unsigned iteration = 0; iteration < 2000; iteration++) {
        size_t length = random_u32() % sizeof decoded;
        for (size_t i = 0; i < length; i++)
            decoded[i] = (uint8_t)random_u32();
        CHECK(ls_rle_compress(decoded, length, encoded, sizeof encoded, &written) == LS_OK);
        uint8_t roundtrip[1024];
        CHECK(ls_rle_decompress(encoded, written, roundtrip, sizeof roundtrip, &decoded_length) ==
              LS_OK);
        CHECK(decoded_length == length && !memcmp(decoded, roundtrip, length));
    }
    for (unsigned iteration = 0; iteration < 1000; iteration++) {
        uint32_t values[128], roundtrip[128], value = random_u32();
        size_t count = 1u + random_u32() % 128u;
        for (size_t i = 0; i < count; i++) {
            value += (int32_t)(random_u32() % 2001u) - 1000;
            values[i] = value;
        }
        CHECK(ls_delta_u32_encode(values, count, encoded, sizeof encoded, &written) == LS_OK);
        size_t result_count = 0;
        CHECK(ls_delta_u32_decode(encoded, written, roundtrip, 128, &result_count) == LS_OK);
        CHECK(result_count == count && !memcmp(values, roundtrip, count * sizeof *values));
    }
    return 0;
}
