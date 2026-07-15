#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "policy check failed: %s:%d\n", #x, __LINE__);                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint8_t storage_bytes[50000];
static unsigned sends;
static uint32_t now;
static uint32_t clock_ms(void *x) {
    (void)x;
    return now;
}
static bool available(void *x) {
    (void)x;
    return true;
}
static size_t mtu(void *x) {
    (void)x;
    return LS_MAX_EVENT_SIZE;
}
static ls_result_t send_event(void *x, const uint8_t *data, size_t length) {
    (void)x;
    (void)data;
    (void)length;
    sends++;
    return LS_OK;
}
static int setup(void) {
    memset(storage_bytes, 0xff, sizeof storage_bytes);
    static ls_identity_t identity = {
        .project_id = "policy", .device_id = "one", .firmware_build_id = "policy001"};
    static ls_config_t config;
    config = (ls_config_t){.identity = &identity, .timestamp_ms = clock_ms};
    static ls_memory_storage_t memory;
    memory = (ls_memory_storage_t){storage_bytes, sizeof storage_bytes};
    static ls_storage_backend_t storage;
    storage = (ls_storage_backend_t){.name = "memory",
                                     .context = &memory,
                                     .capacity = sizeof storage_bytes,
                                     .read = ls_memory_storage_read,
                                     .write = ls_memory_storage_write,
                                     .erase = ls_memory_storage_erase};
    static ls_transport_backend_t transport;
    transport = (ls_transport_backend_t){.name = "sink",
                                         .priority = 1,
                                         .available = available,
                                         .send = send_event,
                                         .max_payload = mtu};
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);
    return 0;
}
int main(void) {
    CHECK(setup() == 0);
    CHECK(ls_sampling_set("half", 0.5f) == LS_OK);
    CHECK(ls_sampling_set("zero", -0.0f) == LS_OK);
    CHECK(ls_sampling_set("invalid", -0.1f) == LS_EINVAL);
    CHECK(ls_sampling_set("invalid", 1.1f) == LS_EINVAL);
    for (unsigned i = 0; i < 8; i++) {
        ls_error_t error = {"repeat", 7, LS_SEVERITY_ERROR, "same"};
        ls_capture_error(&error);
        now++;
    }
    CHECK(ls_dedup_count() == 1);
    ls_dedup_snapshot_t snapshot;
    CHECK(ls_dedup_get(0, &snapshot) == LS_OK && snapshot.count == 8);
    CHECK(ls_flush() == LS_OK);
    CHECK(sends == 4);
    CHECK(setup() == 0);
    sends = 0;
    CHECK(ls_sampling_set_permyriad("sample", 0) == LS_OK);
    for (int i = 0; i < 10; i++) {
        ls_error_t error = {"sample", i, LS_SEVERITY_WARNING, "unique"};
        ls_capture_error(&error);
    }
    CHECK(ls_flush() == LS_OK);
    CHECK(sends == 1);
    CHECK(setup() == 0);
    sends = 0;
    CHECK(ls_rate_limit_set("rate", 2, LS_PER_MINUTE) == LS_OK);
    for (int i = 0; i < 10; i++) {
        ls_error_t error = {"rate", i, LS_SEVERITY_WARNING, "unique"};
        ls_capture_error(&error);
    }
    CHECK(ls_flush() == LS_OK);
    CHECK(sends == 2);
    return 0;
}
