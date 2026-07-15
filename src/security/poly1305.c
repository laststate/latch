#include "../core/internal.h"
#include "crypto_internal.h"
#include "laststate/security.h"

static uint32_t load32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void store32_le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void blocks(ls_poly1305_context_t *context, const uint8_t *message, size_t bytes) {
    const uint32_t hibit = context->final ? 0u : (1u << 24);
    const uint32_t r0 = context->r[0], r1 = context->r[1], r2 = context->r[2], r3 = context->r[3],
                   r4 = context->r[4];
    const uint32_t s1 = r1 * 5u, s2 = r2 * 5u, s3 = r3 * 5u, s4 = r4 * 5u;
    uint32_t h0 = context->h[0], h1 = context->h[1], h2 = context->h[2], h3 = context->h[3],
             h4 = context->h[4];
    while (bytes >= 16u) {
        uint64_t d0, d1, d2, d3, d4;
        uint32_t carry;
        h0 += (load32_le(message)) & 0x3ffffffu;
        h1 += (load32_le(message + 3) >> 2) & 0x3ffffffu;
        h2 += (load32_le(message + 6) >> 4) & 0x3ffffffu;
        h3 += (load32_le(message + 9) >> 6) & 0x3ffffffu;
        h4 += (load32_le(message + 12) >> 8) | hibit;
        d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 +
             (uint64_t)h4 * s1;
        d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 +
             (uint64_t)h4 * s2;
        d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 +
             (uint64_t)h4 * s3;
        d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 +
             (uint64_t)h4 * s4;
        d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 +
             (uint64_t)h4 * r0;
        carry = (uint32_t)(d0 >> 26);
        h0 = (uint32_t)d0 & 0x3ffffffu;
        d1 += carry;
        carry = (uint32_t)(d1 >> 26);
        h1 = (uint32_t)d1 & 0x3ffffffu;
        d2 += carry;
        carry = (uint32_t)(d2 >> 26);
        h2 = (uint32_t)d2 & 0x3ffffffu;
        d3 += carry;
        carry = (uint32_t)(d3 >> 26);
        h3 = (uint32_t)d3 & 0x3ffffffu;
        d4 += carry;
        carry = (uint32_t)(d4 >> 26);
        h4 = (uint32_t)d4 & 0x3ffffffu;
        h0 += carry * 5u;
        carry = h0 >> 26;
        h0 &= 0x3ffffffu;
        h1 += carry;
        message += 16;
        bytes -= 16;
    }
    context->h[0] = h0;
    context->h[1] = h1;
    context->h[2] = h2;
    context->h[3] = h3;
    context->h[4] = h4;
}

void ls_poly1305_init(ls_poly1305_context_t *context, const uint8_t key[32]) {
    context->r[0] = load32_le(key) & 0x3ffffffu;
    context->r[1] = (load32_le(key + 3) >> 2) & 0x3ffff03u;
    context->r[2] = (load32_le(key + 6) >> 4) & 0x3ffc0ffu;
    context->r[3] = (load32_le(key + 9) >> 6) & 0x3f03fffu;
    context->r[4] = (load32_le(key + 12) >> 8) & 0x00fffffu;
    ls_memset(context->h, 0, sizeof context->h);
    context->pad[0] = load32_le(key + 16);
    context->pad[1] = load32_le(key + 20);
    context->pad[2] = load32_le(key + 24);
    context->pad[3] = load32_le(key + 28);
    context->leftover = 0;
    context->final = 0;
}

void ls_poly1305_update(ls_poly1305_context_t *context, const uint8_t *message, size_t length) {
    if (!length)
        return;
    if (context->leftover) {
        size_t want = 16u - context->leftover;
        if (want > length)
            want = length;
        ls_memcpy(context->buffer + context->leftover, message, want);
        message += want;
        length -= want;
        context->leftover += want;
        if (context->leftover < 16u)
            return;
        blocks(context, context->buffer, 16u);
        context->leftover = 0;
    }
    if (length >= 16u) {
        size_t want = length & ~(size_t)0xfu;
        blocks(context, message, want);
        message += want;
        length -= want;
    }
    if (length) {
        ls_memcpy(context->buffer, message, length);
        context->leftover = length;
    }
}

void ls_poly1305_finish(ls_poly1305_context_t *context, uint8_t tag[16]) {
    uint32_t h0, h1, h2, h3, h4, g0, g1, g2, g3, g4, carry, mask;
    uint64_t f;
    if (context->leftover) {
        context->buffer[context->leftover++] = 1u;
        while (context->leftover < 16u)
            context->buffer[context->leftover++] = 0;
        context->final = 1;
        blocks(context, context->buffer, 16u);
    }
    h0 = context->h[0];
    h1 = context->h[1];
    h2 = context->h[2];
    h3 = context->h[3];
    h4 = context->h[4];
    carry = h1 >> 26;
    h1 &= 0x3ffffffu;
    h2 += carry;
    carry = h2 >> 26;
    h2 &= 0x3ffffffu;
    h3 += carry;
    carry = h3 >> 26;
    h3 &= 0x3ffffffu;
    h4 += carry;
    carry = h4 >> 26;
    h4 &= 0x3ffffffu;
    h0 += carry * 5u;
    carry = h0 >> 26;
    h0 &= 0x3ffffffu;
    h1 += carry;
    g0 = h0 + 5u;
    carry = g0 >> 26;
    g0 &= 0x3ffffffu;
    g1 = h1 + carry;
    carry = g1 >> 26;
    g1 &= 0x3ffffffu;
    g2 = h2 + carry;
    carry = g2 >> 26;
    g2 &= 0x3ffffffu;
    g3 = h3 + carry;
    carry = g3 >> 26;
    g3 &= 0x3ffffffu;
    g4 = h4 + carry - (1u << 26);
    mask = (g4 >> 31) - 1u;
    g0 &= mask;
    g1 &= mask;
    g2 &= mask;
    g3 &= mask;
    g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;
    f = (uint64_t)(h0 | (h1 << 26)) + context->pad[0];
    store32_le(tag, (uint32_t)f);
    f = (uint64_t)((h1 >> 6) | (h2 << 20)) + context->pad[1] + (f >> 32);
    store32_le(tag + 4, (uint32_t)f);
    f = (uint64_t)((h2 >> 12) | (h3 << 14)) + context->pad[2] + (f >> 32);
    store32_le(tag + 8, (uint32_t)f);
    f = (uint64_t)((h3 >> 18) | (h4 << 8)) + context->pad[3] + (f >> 32);
    store32_le(tag + 12, (uint32_t)f);
    ls_secure_zero(context, sizeof *context);
}

void ls_poly1305_auth(const uint8_t key[32], const uint8_t *message, size_t length,
                      uint8_t tag[16]) {
    ls_poly1305_context_t context;
    ls_poly1305_init(&context, key);
    if (message && length)
        ls_poly1305_update(&context, message, length);
    ls_poly1305_finish(&context, tag);
}
