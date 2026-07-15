#include <stddef.h>
#include <stdint.h>
#include "laststate/latch.h"
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint8_t output[4096];
    size_t written;
    (void)ls_rle_decompress(data, size, output, sizeof output, &written);
    uint32_t values[512];
    size_t count;
    (void)ls_delta_u32_decode(data, size, values, 512, &count);
    return 0;
}
