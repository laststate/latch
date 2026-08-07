#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#include "../src/core/internal.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "envelope errors failed: %s:%d\n", #x, __LINE__);                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint8_t retained[40000], captured[LS_MAX_EVENT_SIZE];
static size_t captured_length;
static bool available(void *x) {
    (void)x;
    return true;
}
static size_t mtu(void *x) {
    (void)x;
    return sizeof captured;
}
static ls_result_t send_data(void *x, const uint8_t *data, size_t length) {
    (void)x;
    memcpy(captured, data, length);
    captured_length = length;
    return LS_OK;
}
static ls_result_t accept_visit(void *c, uint16_t t, const uint8_t *v, uint16_t l) {
    (void)t; (void)v; (void)l;
    if (c) ++*(unsigned *)c;
    return LS_OK;
}
static void write_u32(uint8_t *p, uint32_t value) {
    p[0]=(uint8_t)value; p[1]=(uint8_t)(value>>8); p[2]=(uint8_t)(value>>16); p[3]=(uint8_t)(value>>24);
}
static ls_result_t stop_visit(void *c, uint16_t t, const uint8_t *v, uint16_t l) {
    (void)c;
    (void)t;
    (void)v;
    (void)l;
    return LS_EAGAIN;
}
static const uint8_t golden_vector[] = {0x4c, 0x53, 0x54, 0x50, 0x01, 0x02, 0x00, 0x00, 0x07,
                                        0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x06, 0x00,
                                        0x00, 0x00, 0x74, 0xdd, 0xc4, 0x89, 0x01, 0x00, 0x02,
                                        0x00, 0xaa, 0xbb, 0xea, 0x84, 0xcc, 0xd8};
static const uint8_t truncated_vector[] = {0x4c, 0x53, 0x54, 0x50, 0x01, 0x02, 0x00, 0x08, 0x07,
                                           0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x06, 0x00,
                                           0x00, 0x00, 0x19, 0x0e, 0xc7, 0xd3, 0x01, 0x00, 0x02,
                                           0x00, 0xaa, 0xbb, 0xea, 0x84, 0xcc, 0xd8};
static const uint8_t unknown_flag_vector[] = {0x4c, 0x53, 0x54, 0x50, 0x01, 0x02, 0x00, 0x80, 0x07,
                                              0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x06, 0x00,
                                              0x00, 0x00, 0x63, 0xf8, 0xae, 0x29, 0x01, 0x00, 0x02,
                                              0x00, 0xaa, 0xbb, 0xea, 0x84, 0xcc, 0xd8};
static const uint8_t malformed_tlv_vector[] = {0x4c, 0x53, 0x54, 0x50, 0x01, 0x02, 0x00, 0x00, 0x07,
                                               0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x06, 0x00,
                                               0x00, 0x00, 0x74, 0xdd, 0xc4, 0x89, 0x00, 0x00, 0x02,
                                               0x00, 0xaa, 0xbb, 0x4f, 0x57, 0x90, 0x13};
int main(void) {
    ls_envelope_info_t golden;
    CHECK(ls_envelope_validate(golden_vector, sizeof golden_vector, &golden) == LS_OK);
    CHECK(golden.version == LS_LEP_VERSION_1 && golden.sequence == 7 && golden.event_id == 9);
    CHECK(!ls_envelope_is_truncated(&golden));
    CHECK(ls_envelope_validate(truncated_vector, sizeof truncated_vector, &golden) == LS_OK &&
          ls_envelope_is_truncated(&golden));
    CHECK(!ls_envelope_is_truncated(NULL));
    ls_envelope_replay_reset(NULL);
    CHECK(!ls_envelope_replay_accept(NULL, 1u));
    ls_envelope_replay_t replay = {0};
    ls_envelope_replay_reset(&replay);
    CHECK(ls_envelope_replay_accept(&replay, 10u));
    CHECK(ls_envelope_replay_accept(&replay, 100u));
    CHECK(!ls_envelope_replay_accept(&replay, 10u));
    CHECK(ls_envelope_replay_accept(&replay, 99u));
    CHECK(ls_envelope_validate(unknown_flag_vector, sizeof unknown_flag_vector, 0) == LS_ECORRUPT);
    CHECK(ls_envelope_validate(malformed_tlv_vector, sizeof malformed_tlv_vector, 0) ==
          LS_ECORRUPT);
    ls_identity_t identity = {.firmware_build_id = "enverror"};
    ls_config_t config = {.identity = &identity};
    ls_memory_storage_t memory = {retained, sizeof retained};
    ls_storage_backend_t storage = {.context = &memory,
                                    .capacity = sizeof retained,
                                    .read = ls_memory_storage_read,
                                    .write = ls_memory_storage_write,
                                    .erase = ls_memory_storage_erase};
    ls_transport_backend_t transport = {
        .priority = 1, .available = available, .send = send_data, .max_payload = mtu};
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);
    ls_event_t encode_event = {.type=LS_EVENT_MESSAGE,.priority=LS_PRIORITY_WARNING,.timestamp_ms=1u,
                               .domain="encode",.severity=LS_SEVERITY_WARNING,.message="invalid",
                               .capture_level=LS_CAPTURE_METADATA};
    size_t invalid_length = 0u;
    uint8_t invalid_out[LS_MAX_EVENT_SIZE];
    CHECK(ls_envelope_encode(NULL, invalid_out, sizeof invalid_out, &invalid_length) == LS_EINVAL);
    CHECK(ls_envelope_encode(&encode_event, NULL, sizeof invalid_out, &invalid_length) == LS_EINVAL);
    CHECK(ls_envelope_encode(&encode_event, invalid_out, sizeof invalid_out, NULL) == LS_EINVAL);
    CHECK(ls_envelope_encode(&encode_event, invalid_out, LS_LEP_HEADER_SIZE, &invalid_length) == LS_EINVAL);
    ls_capture_message("x", LS_SEVERITY_ERROR);
    CHECK(ls_flush() == LS_OK && captured_length > 28);
    CHECK(ls_envelope_validate(0, 0, 0) == LS_EINVAL);
    CHECK(ls_envelope_validate(captured, 23, 0) == LS_ECORRUPT);
    CHECK(ls_envelope_visit(captured, captured_length, 0, 0) == LS_EINVAL);
    CHECK(ls_envelope_visit(captured, captured_length, stop_visit, 0) == LS_EAGAIN);
    uint8_t changed[LS_MAX_EVENT_SIZE];
    memcpy(changed, captured, captured_length);
    changed[0] ^= 1;
    CHECK(ls_envelope_validate(changed, captured_length, 0) == LS_ECORRUPT);
    memcpy(changed, captured, captured_length);
    changed[4] = 99;
    CHECK(ls_envelope_validate(changed, captured_length, 0) == LS_ECORRUPT);
    memcpy(changed, captured, captured_length);
    changed[16] = 0xff;
    CHECK(ls_envelope_validate(changed, captured_length, 0) == LS_ECORRUPT);
    memcpy(changed, captured, captured_length); changed[7] = LS_ENVELOPE_ENCRYPTED;
    CHECK(ls_envelope_validate(changed, captured_length, 0) == LS_ECORRUPT);
    memcpy(changed, captured, captured_length); changed[7] = LS_ENVELOPE_AEAD;
    CHECK(ls_envelope_validate(changed, captured_length, 0) == LS_ECORRUPT);
    memcpy(changed, captured, captured_length); changed[7] = LS_ENVELOPE_AUTHENTICATED;
    CHECK(ls_envelope_validate(changed, captured_length, 0) == LS_ECORRUPT);
    CHECK(ls_envelope_validate(captured, captured_length + 1u, 0) == LS_ECORRUPT);
    memcpy(changed, captured, captured_length); changed[8] ^= 1u;
    CHECK(ls_envelope_validate(changed, captured_length, 0) == LS_ECORRUPT);
    memcpy(changed, captured, captured_length); changed[LS_LEP_HEADER_SIZE + 4u] ^= 1u;
    CHECK(ls_envelope_validate(changed, captured_length, 0) == LS_ECORRUPT);

    /* Build bounded malformed plaintext payloads with otherwise valid CRCs. */
    uint8_t malformed[64] = {0};
    memcpy(malformed, captured, LS_LEP_HEADER_SIZE);
    malformed[7] = 0u;
    write_u32(malformed + 16u, 1u);
    malformed[LS_LEP_HEADER_SIZE] = 0x11u;
    write_u32(malformed + 20u, ls_crc32(malformed, 20u));
    write_u32(malformed + LS_LEP_HEADER_SIZE + 1u, ls_crc32(malformed + LS_LEP_HEADER_SIZE, 1u));
    CHECK(ls_envelope_validate(malformed, LS_LEP_HEADER_SIZE + 1u + 4u, 0) == LS_ECORRUPT);
    write_u32(malformed + 16u, 4u);
    malformed[LS_LEP_HEADER_SIZE+0]=1u; malformed[LS_LEP_HEADER_SIZE+1]=0u;
    malformed[LS_LEP_HEADER_SIZE+2]=5u; malformed[LS_LEP_HEADER_SIZE+3]=0u;
    write_u32(malformed + 20u, ls_crc32(malformed, 20u));
    write_u32(malformed + LS_LEP_HEADER_SIZE + 4u, ls_crc32(malformed + LS_LEP_HEADER_SIZE, 4u));
    CHECK(ls_envelope_validate(malformed, LS_LEP_HEADER_SIZE + 4u + 4u, 0) == LS_ECORRUPT);

    CHECK(ls_envelope_decrypt_payload(captured, captured_length, NULL, 0u, NULL) == LS_EINVAL);
    uint8_t invalid_public[LS_MAX_EVENT_SIZE];
    memcpy(invalid_public, captured, captured_length);
    invalid_public[0] ^= 1u;
    uint8_t public_workspace[LS_MAX_EVENT_SIZE];
    ls_envelope_replay_t invalid_replay = {0};
    CHECK(ls_envelope_decrypt_payload(invalid_public, captured_length, public_workspace, sizeof public_workspace, &invalid_length) == LS_ECORRUPT);
    CHECK(ls_envelope_visit(invalid_public, captured_length, accept_visit, NULL) == LS_ECORRUPT);
    CHECK(ls_envelope_visit_secure(invalid_public, captured_length, public_workspace, sizeof public_workspace, accept_visit, NULL) == LS_ECORRUPT);
    CHECK(ls_envelope_visit_secure_replay(invalid_public, captured_length, &invalid_replay, public_workspace, sizeof public_workspace, accept_visit, NULL) == LS_ECORRUPT);
    size_t plain_length = 0u;
    CHECK(ls_envelope_decrypt_payload(captured, captured_length, NULL, 0u, &plain_length) == LS_EAUTH);
    CHECK(ls_envelope_verify_auth(captured, captured_length) == LS_EAUTH);

    uint8_t key[LS_SECURITY_KEY_SIZE] = {0};
    CHECK(ls_security_set_key(key, sizeof key) == LS_OK);
    unsigned visits = 0u;
    CHECK(ls_envelope_visit_secure(captured, captured_length, NULL, 0u, accept_visit, &visits) == LS_EAUTH);
    ls_envelope_replay_t plaintext_replay = {0};
    CHECK(ls_envelope_visit_secure_replay(captured, captured_length, &plaintext_replay, NULL, 0u, accept_visit, &visits) == LS_EAUTH);
    ls_security_policy_t permissive = {.algorithm=LS_SECURITY_XCHACHA20_POLY1305,.key_id=1u,.reject_plaintext=false,.allow_legacy_hmac=false};
    CHECK(ls_security_set_policy(&permissive) == LS_OK);
    CHECK(ls_envelope_visit_secure(captured, captured_length, NULL, 0u, accept_visit, &visits) == LS_OK);
    CHECK(visits > 0u);
    ls_security_policy_t hmac = {.algorithm=LS_SECURITY_HMAC_SHA256,.key_id=1u,.reject_plaintext=true,.allow_legacy_hmac=true};
    CHECK(ls_security_set_policy(&hmac) == LS_OK);
    captured_length = 0u;
    ls_capture_message("hmac", LS_SEVERITY_ERROR);
    CHECK(ls_flush() == LS_OK && captured_length > LS_HMAC_SHA256_SIZE);
    CHECK(ls_envelope_verify_auth(captured, captured_length) == LS_OK);
    visits = 0u;
    CHECK(ls_envelope_visit_secure(captured, captured_length, NULL, 0u, accept_visit, &visits) == LS_OK);
    ls_envelope_replay_t hmac_replay = {0};
    CHECK(ls_envelope_visit_secure_replay(captured, captured_length, &hmac_replay, NULL, 0u, accept_visit, &visits) == LS_OK);
    CHECK(ls_envelope_visit_secure_replay(captured, captured_length, &hmac_replay, NULL, 0u, accept_visit, &visits) == LS_EAUTH);
    memcpy(changed, captured, captured_length); changed[captured_length-1u] ^= 1u;
    CHECK(ls_envelope_visit_secure(changed, captured_length, NULL, 0u, accept_visit, &visits) == LS_EAUTH);
    ls_security_clear_key();
    return 0;
}
