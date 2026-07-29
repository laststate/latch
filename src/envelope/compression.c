#include "laststate/compression.h"
ls_result_t ls_varint_u32_encode(uint32_t value, uint8_t *output, size_t capacity,
                                 size_t *written) {
    if (!output || !written)
        return LS_EINVAL;
    size_t at = 0;
    do {
        if (at >= capacity)
            return LS_ENOSPACE;
        uint8_t byte = (uint8_t)(value & 0x7fu);
        value >>= 7;
        if (value)
            byte |= 0x80u;
        output[at++] = byte;
    } while (value);
    *written = at;
    return LS_OK;
}
ls_result_t ls_varint_u32_decode(const uint8_t *input, size_t length, uint32_t *value,
                                 size_t *consumed) {
    if (!input || !value || !consumed)
        return LS_EINVAL;
    uint32_t result = 0;
    for (size_t i = 0; i < length && i < 5; i++) {
        uint8_t byte = input[i];
        if (i == 4 && (byte & 0xf0u))
            return LS_EOVERFLOW;
        result |= (uint32_t)(byte & 0x7fu) << (i * 7u);
        if (!(byte & 0x80u)) {
            *value = result;
            *consumed = i + 1;
            return LS_OK;
        }
    }
    return length >= 5 ? LS_ECORRUPT : LS_EAGAIN;
}
ls_result_t ls_rle_compress(const uint8_t *input, size_t length, uint8_t *output, size_t capacity,
                            size_t *written) {
    if ((!input && length) || !output || !written)
        return LS_EINVAL;
    size_t in = 0, out = 0;
    while (in < length) {
        size_t run = 1;
        while (in + run < length && input[in + run] == input[in] && run < 128)
            run++;
        if (run >= 3) {
            if (capacity - out < 2)
                return LS_ENOSPACE;
            output[out++] = (uint8_t)(0x80u | (run - 1u));
            output[out++] = input[in];
            in += run;
            continue;
        }
        size_t literal = in;
        in += run;
        while (in < length) {
            run = 1;
            while (in + run < length && input[in + run] == input[in] && run < 128)
                run++;
            if (run >= 3 || in - literal + run > 128)
                break;
            in += run;
        }
        size_t literal_length = in - literal;
        if (capacity - out < literal_length + 1)
            return LS_ENOSPACE;
        output[out++] = (uint8_t)(literal_length - 1u);
        for (size_t i = 0; i < literal_length; i++)
            output[out++] = input[literal + i];
    }
    *written = out;
    return LS_OK;
}
ls_result_t ls_rle_decompress(const uint8_t *input, size_t length, uint8_t *output, size_t capacity,
                              size_t *written) {
    if ((!input && length) || !output || !written)
        return LS_EINVAL;
    size_t in = 0, out = 0;
    while (in < length) {
        uint8_t control = input[in++];
        size_t count = (control & 0x7fu) + 1u;
        if (control & 0x80u) {
            if (in >= length)
                return LS_ECORRUPT;
            if (count > capacity - out)
                return LS_ENOSPACE;
            uint8_t value = input[in++];
            for (size_t i = 0; i < count; i++)
                output[out++] = value;
        } else {
            if (count > length - in)
                return LS_ECORRUPT;
            if (count > capacity - out)
                return LS_ENOSPACE;
            for (size_t i = 0; i < count; i++)
                output[out++] = input[in++];
        }
    }
    *written = out;
    return LS_OK;
}
static uint32_t zigzag_encode(int32_t value) {
    uint32_t bits = (uint32_t)value;
    return (bits << 1) ^ (uint32_t) - (value < 0);
}
static int32_t zigzag_decode(uint32_t value) {
    return (int32_t)((value >> 1) ^ (uint32_t) - (int32_t)(value & 1u));
}
ls_result_t ls_delta_u32_encode(const uint32_t *values, size_t count, uint8_t *output,
                                size_t capacity, size_t *written) {
    if ((!values && count) || !output || !written)
        return LS_EINVAL;
    size_t at = 0;
    uint32_t previous = 0;
    for (size_t i = 0; i < count; i++) {
        int32_t delta = (int32_t)(values[i] - previous);
        uint8_t encoded[5];
        size_t length = 0;
        ls_result_t result =
            ls_varint_u32_encode(zigzag_encode(delta), encoded, sizeof encoded, &length);
        if (result != LS_OK)
            return result;
        if (length > capacity - at)
            return LS_ENOSPACE;
        for (size_t b = 0; b < length; b++)
            output[at++] = encoded[b];
        previous = values[i];
    }
    *written = at;
    return LS_OK;
}
ls_result_t ls_delta_u32_decode(const uint8_t *input, size_t length, uint32_t *values,
                                size_t capacity, size_t *count) {
    if ((!input && length) || !values || !count)
        return LS_EINVAL;
    size_t at = 0, items = 0;
    uint32_t previous = 0;
    while (at < length) {
        if (items >= capacity)
            return LS_ENOSPACE;
        uint32_t encoded;
        size_t consumed;
        ls_result_t result = ls_varint_u32_decode(input + at, length - at, &encoded, &consumed);
        if (result != LS_OK)
            return result;
        previous += (uint32_t)zigzag_decode(encoded);
        values[items++] = previous;
        at += consumed;
    }
    *count = items;
    return LS_OK;
}
