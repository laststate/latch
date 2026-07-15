#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "fuzz check failed: %s:%d\n", #x, __LINE__);                           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint8_t retained[50000], valid[LS_MAX_EVENT_SIZE];
static size_t valid_length;
static uint32_t random_state = 0x12345678u;
static uint32_t random_value(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
static bool yes(void *x) {
    (void)x;
    return true;
}
static size_t mtu(void *x) {
    (void)x;
    return LS_MAX_EVENT_SIZE;
}
static ls_result_t send_data(void *x, const uint8_t *data, size_t length) {
    (void)x;
    memcpy(valid, data, length);
    valid_length = length;
    return LS_OK;
}
static ls_result_t visitor(void *x, uint16_t type, const uint8_t *value, uint16_t length) {
    (void)x;
    (void)type;
    (void)value;
    (void)length;
    return LS_OK;
}
static ls_result_t random_bytes(void *context, uint8_t *output, size_t length) {
    (void)context;
    for (size_t i = 0; i < length; i++)
        output[i] = (uint8_t)random_value();
    return LS_OK;
}
int main(void) {
    ls_identity_t identity = {
        .project_id = "fuzz", .device_id = "one", .firmware_build_id = "fuzz0001"};
    ls_config_t config = {.identity = &identity};
    ls_memory_storage_t memory = {retained, sizeof retained};
    ls_storage_backend_t storage = {.name = "memory",
                                    .context = &memory,
                                    .capacity = sizeof retained,
                                    .read = ls_memory_storage_read,
                                    .write = ls_memory_storage_write,
                                    .erase = ls_memory_storage_erase};
    ls_transport_backend_t transport = {
        .name = "sink", .priority = 1, .available = yes, .send = send_data, .max_payload = mtu};
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);
    uint8_t key[32];
    for (unsigned i = 0; i < 32; i++)
        key[i] = (uint8_t)i;
    ls_security_set_random_provider(random_bytes, 0);
    CHECK(ls_security_set_key(key, sizeof key) == LS_OK);
    ls_capture_message("fuzz seed", LS_SEVERITY_ERROR);
    CHECK(ls_flush() == LS_OK);
    CHECK(ls_envelope_verify_auth(valid, valid_length) == LS_OK);
    for (size_t index = 0; index < valid_length; index++) {
        uint8_t mutated[LS_MAX_EVENT_SIZE];
        memcpy(mutated, valid, valid_length);
        mutated[index] ^= 1u;
        ls_result_t structural = ls_envelope_validate(mutated, valid_length, 0);
        if (structural == LS_OK)
            CHECK(ls_envelope_verify_auth(mutated, valid_length) == LS_EAUTH);
    }
    for (unsigned iteration = 0; iteration < 20000; iteration++) {
        uint8_t bytes[512];
        size_t length = random_value() % sizeof bytes;
        for (size_t i = 0; i < length; i++)
            bytes[i] = (uint8_t)random_value();
        ls_result_t result = ls_envelope_validate(bytes, length, 0);
        if (result == LS_OK) {
            ls_result_t visited = ls_envelope_visit(bytes, length, visitor, 0);
            CHECK(visited == LS_OK || visited == LS_EAUTH);
        }
    }
    return 0;
}
