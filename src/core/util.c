#include "internal.h"

/* MSVC emits this marker for any translation unit exposing a float API, even
   when the implementation performs no floating-point operation. Providing it
   here keeps /NODEFAULTLIB freestanding links self-contained. */
#if defined(_MSC_VER)
int _fltused;
#endif

void *ls_memcpy(void *destination, const void *source, size_t length) {
    uint8_t *dst = (uint8_t *)destination;
    const uint8_t *src = (const uint8_t *)source;
    while (length--) {
        *dst++ = *src++;
    }
    return destination;
}
void *ls_memset(void *destination, int value, size_t length) {
    uint8_t *dst = (uint8_t *)destination;
    while (length--) {
        *dst++ = (uint8_t)value;
    }
    return destination;
}
int ls_memcmp(const void *left, const void *right, size_t length) {
    const uint8_t *a = (const uint8_t *)left, *b = (const uint8_t *)right;
    while (length--) {
        if (*a != *b)
            return *a < *b ? -1 : 1;
        ++a;
        ++b;
    }
    return 0;
}
size_t ls_string_length(const char *text) {
    size_t length = 0;
    if (text)
        while (text[length])
            ++length;
    return length;
}
uint32_t ls_crc32(const void *data, size_t length) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xffffffffu;
    while (length--) {
        crc ^= *p++;
        for (unsigned i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t) - (int32_t)(crc & 1u));
    }
    return ~crc;
}
uint32_t ls_hash_string(const char *text) {
    uint32_t hash = 2166136261u;
    if (!text)
        return 0;
    while (*text) {
        hash ^= (uint8_t)*text++;
        hash *= 16777619u;
    }
    return hash;
}
void ls_copy_string(char *dst, size_t size, const char *src) {
    size_t at = 0;
    if (!dst || !size)
        return;
    if (src)
        while (at + 1 < size && src[at]) {
            dst[at] = src[at];
            ++at;
        }
    dst[at] = '\0';
}
ls_result_t ls_writer_write(ls_writer_t *writer, const void *data, size_t size) {
    if (!writer || (!data && size) || writer->length > writer->capacity ||
        size > writer->capacity - writer->length)
        return LS_ENOSPACE;
    ls_memcpy(writer->data + writer->length, data, size);
    writer->length += size;
    return LS_OK;
}
ls_result_t ls_writer_u8(ls_writer_t *writer, uint8_t value) {
    return ls_writer_write(writer, &value, 1);
}
ls_result_t ls_writer_u16(ls_writer_t *writer, uint16_t value) {
    uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    return ls_writer_write(writer, bytes, 2);
}
ls_result_t ls_writer_u32(ls_writer_t *writer, uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16),
                        (uint8_t)(value >> 24)};
    return ls_writer_write(writer, bytes, 4);
}
ls_result_t ls_writer_u64(ls_writer_t *writer, uint64_t value) {
    uint8_t bytes[8];
    for (unsigned i = 0; i < 8; i++)
        bytes[i] = (uint8_t)(value >> (i * 8));
    return ls_writer_write(writer, bytes, 8);
}
ls_result_t ls_writer_tlv(ls_writer_t *writer, uint16_t type, const void *value, uint16_t length) {
    ls_result_t result;
    if (!writer || writer->length > writer->capacity ||
        (size_t)length + 4u > writer->capacity - writer->length)
        return LS_ENOSPACE;
    if ((result = ls_writer_u16(writer, type)) != LS_OK ||
        (result = ls_writer_u16(writer, length)) != LS_OK)
        return result;
    return ls_writer_write(writer, value, length);
}
void ls_enter_critical(void) {
    if (ls_runtime.config.enter_critical)
        ls_runtime.config.enter_critical(ls_runtime.config.critical_context);
}
void ls_leave_critical(void) {
    if (ls_runtime.config.leave_critical)
        ls_runtime.config.leave_critical(ls_runtime.config.critical_context);
}
