#include "../core/internal.h"
#include "laststate/security.h"
typedef struct {
    uint32_t state[8];
    uint64_t bits;
    uint8_t block[64];
    size_t used;
} sha256_context_t;
static uint32_t rotr(uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32u - bits));
}
static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static void transform(sha256_context_t *context, const uint8_t block[64]) {
    static const uint32_t constants[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
        0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
        0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
        0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
        0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
        0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
        0xc67178f2u};
    uint32_t words[64];
    for (unsigned i = 0; i < 16; i++)
        words[i] = read_be32(block + i * 4);
    for (unsigned i = 16; i < 64; i++) {
        uint32_t s0 = rotr(words[i - 15], 7) ^ rotr(words[i - 15], 18) ^ (words[i - 15] >> 3);
        uint32_t s1 = rotr(words[i - 2], 17) ^ rotr(words[i - 2], 19) ^ (words[i - 2] >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }
    uint32_t a = context->state[0], b = context->state[1], c = context->state[2],
             d = context->state[3], e = context->state[4], f = context->state[5],
             g = context->state[6], h = context->state[7];
    for (unsigned i = 0; i < 64; i++) {
        uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25), choice = (e & f) ^ ((~e) & g),
                 temp1 = h + s1 + choice + constants[i] + words[i],
                 s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22),
                 majority = (a & b) ^ (a & c) ^ (b & c), temp2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
}
static void init(sha256_context_t *context) {
    static const uint32_t initial[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    ls_memcpy(context->state, initial, sizeof initial);
    context->bits = 0;
    context->used = 0;
}
static void update(sha256_context_t *context, const uint8_t *data, size_t length) {
    context->bits += (uint64_t)length * 8u;
    while (length) {
        size_t take = 64u - context->used;
        if (take > length)
            take = length;
        ls_memcpy(context->block + context->used, data, take);
        context->used += take;
        data += take;
        length -= take;
        if (context->used == 64u) {
            transform(context, context->block);
            context->used = 0;
        }
    }
}
static void finish(sha256_context_t *context, uint8_t output[32]) {
    context->block[context->used++] = 0x80u;
    if (context->used > 56u) {
        while (context->used < 64u)
            context->block[context->used++] = 0;
        transform(context, context->block);
        context->used = 0;
    }
    while (context->used < 56u)
        context->block[context->used++] = 0;
    for (unsigned i = 0; i < 8; i++)
        context->block[56 + i] = (uint8_t)(context->bits >> (56u - i * 8u));
    transform(context, context->block);
    for (unsigned i = 0; i < 8; i++) {
        output[i * 4] = (uint8_t)(context->state[i] >> 24);
        output[i * 4 + 1] = (uint8_t)(context->state[i] >> 16);
        output[i * 4 + 2] = (uint8_t)(context->state[i] >> 8);
        output[i * 4 + 3] = (uint8_t)context->state[i];
    }
    ls_memset(context, 0, sizeof *context);
}
void ls_hmac_sha256(const uint8_t *key, size_t key_length, const uint8_t *data, size_t length,
                    uint8_t output[32]) {
    uint8_t normalized[64], inner_hash[32], inner_pad[64], outer_pad[64];
    ls_memset(normalized, 0, sizeof normalized);
    if (key_length > 64u) {
        sha256_context_t hash;
        init(&hash);
        update(&hash, key, key_length);
        finish(&hash, normalized);
    } else if (key && key_length)
        ls_memcpy(normalized, key, key_length);
    for (unsigned i = 0; i < 64; i++) {
        inner_pad[i] = normalized[i] ^ 0x36u;
        outer_pad[i] = normalized[i] ^ 0x5cu;
    }
    sha256_context_t inner;
    init(&inner);
    update(&inner, inner_pad, 64);
    update(&inner, data, length);
    finish(&inner, inner_hash);
    sha256_context_t outer;
    init(&outer);
    update(&outer, outer_pad, 64);
    update(&outer, inner_hash, 32);
    finish(&outer, output);
    ls_memset(normalized, 0, sizeof normalized);
    ls_memset(inner_hash, 0, sizeof inner_hash);
}
ls_result_t ls_hkdf_sha256(const uint8_t *salt, size_t salt_length, const uint8_t *ikm,
                           size_t ikm_length, const uint8_t *info, size_t info_length,
                           uint8_t *output, size_t output_length) {
    uint8_t zero_salt[32] = {0}, prk[32], previous[32], message[32 + LS_HKDF_MAX_INFO_SIZE + 1];
    size_t produced = 0, previous_length = 0;
    uint8_t counter = 1;
    if ((!salt && salt_length) || (!ikm && ikm_length) || (!info && info_length) ||
        (!output && output_length) || info_length > LS_HKDF_MAX_INFO_SIZE ||
        output_length > 255u * 32u)
        return LS_EINVAL;
    ls_hmac_sha256(salt_length ? salt : zero_salt, salt_length ? salt_length : sizeof zero_salt,
                   ikm, ikm_length, prk);
    while (produced < output_length) {
        size_t message_length = 0;
        if (previous_length) {
            ls_memcpy(message, previous, previous_length);
            message_length = previous_length;
        }
        if (info_length) {
            ls_memcpy(message + message_length, info, info_length);
            message_length += info_length;
        }
        message[message_length++] = counter++;
        ls_hmac_sha256(prk, sizeof prk, message, message_length, previous);
        previous_length = sizeof previous;
        size_t take = output_length - produced;
        if (take > sizeof previous)
            take = sizeof previous;
        ls_memcpy(output + produced, previous, take);
        produced += take;
    }
    ls_secure_zero(prk, sizeof prk);
    ls_secure_zero(previous, sizeof previous);
    ls_secure_zero(message, sizeof message);
    return LS_OK;
}
void ls_secure_zero(void *data, size_t length) {
    volatile uint8_t *p = (volatile uint8_t *)data;
    if (!p)
        return;
    while (length--)
        *p++ = 0;
}
bool ls_constant_time_equal(const uint8_t *left, const uint8_t *right, size_t length) {
    uint8_t difference = 0;
    if ((!left || !right) && length)
        return false;
    for (size_t i = 0; i < length; i++)
        difference |= (uint8_t)(left[i] ^ right[i]);
    return difference == 0;
}
ls_result_t ls_security_set_key(const uint8_t *key, size_t length) {
    if (!key || length != LS_SECURITY_KEY_SIZE)
        return LS_EINVAL;
    ls_enter_critical();
    ls_secure_zero(ls_runtime.security_key, sizeof ls_runtime.security_key);
    ls_memcpy(ls_runtime.security_key, key, length);
    ls_runtime.security_key_length = length;
    ls_secure_zero(ls_runtime.last_envelope_nonce, sizeof ls_runtime.last_envelope_nonce);
    ls_runtime.has_last_envelope_nonce = false;
    ls_leave_critical();
    return LS_OK;
}
void ls_security_clear_key(void) {
    ls_enter_critical();
    ls_secure_zero(ls_runtime.security_key, sizeof ls_runtime.security_key);
    ls_runtime.security_key_length = 0;
    ls_secure_zero(ls_runtime.last_envelope_nonce, sizeof ls_runtime.last_envelope_nonce);
    ls_runtime.has_last_envelope_nonce = false;
    ls_leave_critical();
}
bool ls_security_enabled(void) {
    return ls_runtime.security_key_length != 0;
}
ls_result_t ls_security_set_policy(const ls_security_policy_t *policy) {
    if (!policy ||
        (policy->algorithm != LS_SECURITY_HMAC_SHA256 &&
         policy->algorithm != LS_SECURITY_XCHACHA20_POLY1305) ||
        policy->key_id == 0 ||
        (policy->algorithm == LS_SECURITY_HMAC_SHA256 && !policy->allow_legacy_hmac))
        return LS_EINVAL;
    ls_enter_critical();
    ls_runtime.security_policy = *policy;
    ls_leave_critical();
    return LS_OK;
}
ls_security_policy_t ls_security_get_policy(void) {
    return ls_runtime.security_policy;
}
void ls_security_set_random_provider(ls_crypto_random_fn random, void *context) {
    ls_enter_critical();
    ls_runtime.crypto_random = random;
    ls_runtime.crypto_random_context = context;
    ls_leave_critical();
}
ls_result_t ls_security_random(uint8_t *output, size_t length) {
    if ((!output && length) || !ls_runtime.crypto_random)
        return LS_ENOTSUP;
    ls_result_t result = ls_runtime.crypto_random(ls_runtime.crypto_random_context, output, length);
    if (result != LS_OK && output)
        ls_secure_zero(output, length);
    return result;
}
bool ls_replay_accept(uint32_t sequence) {
    ls_envelope_replay_t replay = {
        .highest_sequence = ls_runtime.replay_highest,
        .seen_sequences = ls_runtime.replay_bitmap,
        .initialized = ls_runtime.replay_bitmap != 0u,
    };
    bool accepted = ls_envelope_replay_accept(&replay, sequence);
    ls_runtime.replay_highest = replay.highest_sequence;
    ls_runtime.replay_bitmap = replay.seen_sequences;
    return accepted;
}
ls_result_t ls_envelope_verify_auth(const uint8_t *data, size_t length) {
    ls_envelope_info_t info;
    ls_result_t result = ls_envelope_validate(data, length, &info);
    if (result != LS_OK)
        return result;
    if (!(info.flags & LS_ENVELOPE_AUTHENTICATED) || !ls_runtime.security_key_length)
        return LS_EAUTH;
    if (info.flags & LS_ENVELOPE_AEAD) {
        size_t plaintext_length = 0;
        return ls_envelope_decrypt_payload(data, length, 0, 0, &plaintext_length);
    }
    if (!ls_runtime.security_policy.allow_legacy_hmac)
        return LS_EAUTH;
    uint8_t expected[LS_HMAC_SHA256_SIZE];
    ls_hmac_sha256(ls_runtime.security_key, ls_runtime.security_key_length, data,
                   length - LS_HMAC_SHA256_SIZE, expected);
    bool valid =
        ls_constant_time_equal(expected, data + length - LS_HMAC_SHA256_SIZE, sizeof expected);
    ls_secure_zero(expected, sizeof expected);
    return valid ? LS_OK : LS_EAUTH;
}
