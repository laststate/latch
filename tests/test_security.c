#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "security check failed: %s:%d\\n", #expression, __LINE__);             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint8_t retained[50000];
static uint8_t sent[LS_MAX_EVENT_SIZE];
static size_t sent_length;

static int equal_hex(const uint8_t *data, const char *hex, size_t length) {
    static const char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < length; ++index) {
        if (digits[data[index] >> 4u] != hex[index * 2u] ||
            digits[data[index] & 15u] != hex[index * 2u + 1u]) {
            return 0;
        }
    }
    return 1;
}

static bool available(void *context) {
    (void)context;
    return true;
}

static size_t mtu(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    (void)context;
    if (length > sizeof(sent)) {
        return LS_ENOSPACE;
    }
    memcpy(sent, data, length);
    sent_length = length;
    return LS_OK;
}

static ls_result_t random_bytes(void *context, uint8_t *output, size_t length) {
    uint32_t *state = (uint32_t *)context;
    for (size_t index = 0; index < length; ++index) {
        *state = *state * 1664525u + 1013904223u;
        output[index] = (uint8_t)(*state >> 24u);
    }
    return LS_OK;
}

static ls_result_t count_tlv(void *context, uint16_t type, const uint8_t *value, uint16_t length) {
    (void)type;
    (void)value;
    (void)length;
    ++*(unsigned *)context;
    return LS_OK;
}

static int capture_envelope(const char *message, uint8_t *output, size_t *output_length) {
    ls_capture_message(message, LS_SEVERITY_ERROR);
    if (ls_flush() != LS_OK || sent_length == 0u || sent_length > LS_MAX_EVENT_SIZE) {
        return 0;
    }
    memcpy(output, sent, sent_length);
    *output_length = sent_length;
    return 1;
}

int main(void) {
    uint8_t key[20];
    uint8_t digest[32];
    memset(key, 0x0b, sizeof(key));
    ls_hmac_sha256(key, sizeof(key), (const uint8_t *)"Hi There", 8u, digest);
    CHECK(equal_hex(digest, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
                    sizeof(digest)));

    uint8_t chacha_key[32];
    uint8_t nonce[12] = {0, 0, 0, 9, 0, 0, 0, 0x4a, 0, 0, 0, 0};
    uint8_t stream[64];
    for (unsigned index = 0; index < sizeof(chacha_key); ++index) {
        chacha_key[index] = (uint8_t)index;
    }
    ls_chacha20_xor(chacha_key, nonce, 1u, NULL, stream, sizeof(stream));
    CHECK(equal_hex(stream,
                    "10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4"
                    "ed2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e",
                    sizeof(stream)));
    CHECK(ls_replay_accept(10u));
    CHECK(!ls_replay_accept(10u));
    CHECK(ls_replay_accept(9u));
    CHECK(!ls_replay_accept(9u));
    CHECK(ls_replay_accept(100u));
    CHECK(!ls_replay_accept(1u));

    ls_identity_t identity = {
        .project_id = "security",
        .device_id = "one",
        .firmware_build_id = "security01",
    };
    ls_config_t config = {.identity = &identity};
    ls_memory_storage_t memory = {retained, sizeof(retained)};
    ls_storage_backend_t storage = {
        .name = "memory",
        .context = &memory,
        .capacity = sizeof(retained),
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    ls_transport_backend_t transport = {
        .name = "sink",
        .priority = 1,
        .available = available,
        .send = send_data,
        .max_payload = mtu,
    };
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);

    uint32_t random_state = 1u;
    ls_security_set_random_provider(random_bytes, &random_state);
    CHECK(ls_security_set_key(chacha_key, sizeof(chacha_key)) == LS_OK);

    uint8_t envelope[LS_MAX_EVENT_SIZE];
    size_t envelope_length = 0u;
    CHECK(capture_envelope("authenticated", envelope, &envelope_length));
    CHECK(envelope_length > LS_AEAD_TAG_SIZE);
    CHECK(ls_envelope_verify_auth(envelope, envelope_length) == LS_OK);

    uint8_t plaintext[LS_MAX_EVENT_SIZE];
    size_t plaintext_length = 0u;
    CHECK(ls_envelope_decrypt_payload(envelope, envelope_length, plaintext, sizeof(plaintext),
                                      &plaintext_length) == LS_OK);
    CHECK(plaintext_length > 0u);
    CHECK(ls_envelope_visit(envelope, envelope_length, count_tlv, NULL) == LS_EAUTH);
    unsigned tlvs = 0u;
    memset(plaintext, 0xa5, sizeof(plaintext));
    CHECK(ls_envelope_visit_secure(envelope, envelope_length, plaintext, sizeof(plaintext),
                                   count_tlv, &tlvs) == LS_OK);
    CHECK(tlvs > 0u);
    for (size_t index = 0; index < plaintext_length; ++index) {
        CHECK(plaintext[index] == 0u);
    }

    ls_envelope_replay_t tamper_window;
    ls_envelope_replay_reset(&tamper_window);
    uint8_t tampered[LS_MAX_EVENT_SIZE];
    memcpy(tampered, envelope, envelope_length);
    tampered[envelope_length - 1u] ^= 1u;
    tlvs = 0u;
    CHECK(ls_envelope_visit_secure_replay(tampered, envelope_length, &tamper_window, plaintext,
                                          sizeof(plaintext), count_tlv, &tlvs) == LS_EAUTH);
    CHECK(tlvs == 0u);
    CHECK(ls_envelope_visit_secure_replay(envelope, envelope_length, &tamper_window, plaintext,
                                          sizeof(plaintext), count_tlv, &tlvs) == LS_OK);
    CHECK(tlvs > 0u);
    CHECK(ls_envelope_visit_secure_replay(envelope, envelope_length, &tamper_window, plaintext,
                                          sizeof(plaintext), count_tlv, &tlvs) == LS_EAUTH);

    uint8_t older[LS_MAX_EVENT_SIZE];
    uint8_t newer[LS_MAX_EVENT_SIZE];
    size_t older_length = 0u;
    size_t newer_length = 0u;
    ls_envelope_info_t older_info;
    ls_envelope_info_t newer_info;
    CHECK(capture_envelope("replay-older", older, &older_length));
    CHECK(capture_envelope("replay-newer", newer, &newer_length));
    CHECK(ls_envelope_validate(older, older_length, &older_info) == LS_OK);
    CHECK(ls_envelope_validate(newer, newer_length, &newer_info) == LS_OK);
    CHECK(newer_info.sequence == older_info.sequence + 1u);

    ls_envelope_replay_t out_of_order_window = {0};
    tlvs = 0u;
    CHECK(ls_envelope_visit_secure_replay(newer, newer_length, &out_of_order_window, plaintext,
                                          sizeof(plaintext), count_tlv, &tlvs) == LS_OK);
    CHECK(ls_envelope_visit_secure_replay(older, older_length, &out_of_order_window, plaintext,
                                          sizeof(plaintext), count_tlv, &tlvs) == LS_OK);
    CHECK(ls_envelope_visit_secure_replay(older, older_length, &out_of_order_window, plaintext,
                                          sizeof(plaintext), count_tlv, &tlvs) == LS_EAUTH);

    uint8_t expired[LS_MAX_EVENT_SIZE];
    uint8_t advancing[LS_MAX_EVENT_SIZE];
    size_t expired_length = 0u;
    size_t advancing_length = 0u;
    ls_envelope_info_t expired_info;
    ls_envelope_info_t advancing_info;
    CHECK(capture_envelope("replay-expired", expired, &expired_length));
    CHECK(ls_envelope_validate(expired, expired_length, &expired_info) == LS_OK);

    ls_envelope_replay_t expiry_window = {0};
    for (unsigned index = 0; index < LS_ENVELOPE_REPLAY_WINDOW_SIZE + 1u; ++index) {
        char message[32];
        int written = snprintf(message, sizeof(message), "replay-advance-%u", index);
        CHECK(written > 0 && (size_t)written < sizeof(message));
        CHECK(capture_envelope(message, advancing, &advancing_length));
        CHECK(ls_envelope_visit_secure_replay(advancing, advancing_length, &expiry_window,
                                              plaintext, sizeof(plaintext), count_tlv,
                                              &tlvs) == LS_OK);
    }
    CHECK(ls_envelope_validate(advancing, advancing_length, &advancing_info) == LS_OK);
    CHECK((uint32_t)(advancing_info.sequence - expired_info.sequence) >
          LS_ENVELOPE_REPLAY_WINDOW_SIZE);
    CHECK(ls_envelope_visit_secure_replay(expired, expired_length, &expiry_window, plaintext,
                                          sizeof(plaintext), count_tlv, &tlvs) == LS_EAUTH);

    ls_security_policy_t hmac_policy = {
        .algorithm = LS_SECURITY_HMAC_SHA256,
        .key_id = 1u,
        .reject_plaintext = true,
        .allow_legacy_hmac = true,
    };
    CHECK(ls_security_set_policy(&hmac_policy) == LS_OK);
    uint8_t hmac_envelope[LS_MAX_EVENT_SIZE];
    size_t hmac_length = 0u;
    CHECK(capture_envelope("hmac-replay", hmac_envelope, &hmac_length));
    ls_envelope_replay_t hmac_window = {0};
    tlvs = 0u;
    CHECK(ls_envelope_visit_secure_replay(hmac_envelope, hmac_length, &hmac_window, NULL, 0u,
                                          count_tlv, &tlvs) == LS_OK);
    CHECK(tlvs > 0u);
    CHECK(ls_envelope_visit_secure_replay(hmac_envelope, hmac_length, &hmac_window, NULL, 0u,
                                          count_tlv, &tlvs) == LS_EAUTH);

    memcpy(tampered, hmac_envelope, hmac_length);
    tampered[hmac_length - 1u] ^= 1u;
    ls_envelope_replay_t hmac_tamper_window = {0};
    CHECK(ls_envelope_visit_secure_replay(tampered, hmac_length, &hmac_tamper_window, NULL, 0u,
                                          count_tlv, &tlvs) == LS_EAUTH);
    CHECK(ls_envelope_visit_secure_replay(hmac_envelope, hmac_length, &hmac_tamper_window, NULL, 0u,
                                          count_tlv, &tlvs) == LS_OK);

    CHECK(ls_envelope_visit_secure_replay(envelope, envelope_length, NULL, plaintext,
                                          sizeof(plaintext), count_tlv, &tlvs) == LS_EINVAL);
    ls_security_clear_key();
    CHECK(!ls_security_enabled());
    return 0;
}
