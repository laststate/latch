#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
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
    CHECK(ls_envelope_verify_auth(captured, captured_length) == LS_EAUTH);
    return 0;
}
