#ifndef LASTSTATE_COMPRESSION_H
#define LASTSTATE_COMPRESSION_H
#include <stddef.h>
#include <stdint.h>
#include "event.h"
ls_result_t ls_varint_u32_encode(uint32_t value, uint8_t *output, size_t capacity, size_t *written);
ls_result_t ls_varint_u32_decode(const uint8_t *input, size_t length, uint32_t *value,
                                 size_t *consumed);
ls_result_t ls_rle_compress(const uint8_t *input, size_t length, uint8_t *output, size_t capacity,
                            size_t *written);
ls_result_t ls_rle_decompress(const uint8_t *input, size_t length, uint8_t *output, size_t capacity,
                              size_t *written);
ls_result_t ls_delta_u32_encode(const uint32_t *values, size_t count, uint8_t *output,
                                size_t capacity, size_t *written);
ls_result_t ls_delta_u32_decode(const uint8_t *input, size_t length, uint32_t *values,
                                size_t capacity, size_t *count);
#endif
