#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "check failed: %s at line %d\n", #condition, __LINE__);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint32_t now_ms;
static uint8_t persisted[40000];
static uint8_t sent[LS_MAX_EVENT_SIZE];
static size_t sent_length;
static unsigned fragments;
static uint32_t clock_ms(void *unused) {
    (void)unused;
    return now_ms;
}
static bool online(void *unused) {
    (void)unused;
    return true;
}
static ls_result_t send_data(void *unused, const uint8_t *data, size_t length) {
    (void)unused;
    memcpy(sent, data, length);
    sent_length = length;
    return LS_OK;
}
static size_t large_mtu(void *unused) {
    (void)unused;
    return LS_MAX_EVENT_SIZE;
}
static size_t small_mtu(void *unused) {
    (void)unused;
    return 64;
}
static ls_result_t send_fragment(void *unused, uint32_t event_id, uint16_t index, uint16_t count,
                                 const uint8_t *data, size_t length, uint32_t crc) {
    (void)unused;
    (void)event_id;
    (void)index;
    (void)count;
    CHECK(data != 0);
    CHECK(length <= 64);
    CHECK(crc != 0);
    fragments++;
    return LS_OK;
}
typedef struct {
    unsigned fields, memory_fields;
    uint16_t memory_length;
} visitor_state_t;
static ls_result_t visit(void *context, uint16_t type, const uint8_t *value, uint16_t length) {
    visitor_state_t *state = (visitor_state_t *)context;
    CHECK(type > 0);
    CHECK(value != 0);
    CHECK(length > 0);
    state->fields++;
    if (type == LS_TLV_MEMORY) {
        state->memory_fields++;
        state->memory_length = length;
    }
    return LS_OK;
}
int main(void) {
    ls_identity_t identity = {.project_id = "test",
                              .device_id = "unit-1",
                              .product = "host",
                              .firmware_build_id = "abc12345",
                              .architecture = "host",
                              .rtos = "none",
                              .region = "lab",
                              .device_group = "ci"};
    ls_config_t config = {
        .identity = &identity, .architecture = LS_ARCH_UNKNOWN, .timestamp_ms = clock_ms};
    ls_memory_storage_t memory = {persisted, sizeof persisted};
    ls_storage_backend_t storage = {.name = "ram",
                                    .context = &memory,
                                    .capacity = sizeof persisted,
                                    .read = ls_memory_storage_read,
                                    .write = ls_memory_storage_write,
                                    .erase = ls_memory_storage_erase};
    ls_transport_backend_t transport = {.name = "loopback",
                                        .priority = 10,
                                        .available = online,
                                        .send = send_data,
                                        .max_payload = large_mtu,
                                        .capabilities = LS_TRANSPORT_ACK};
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);
    now_ms = 10;
    ls_breadcrumb_kv_t kv = {7, LS_VALUE_U32, {.u32 = 42}};
    ls_breadcrumb_t breadcrumb = {"boot", LS_SEVERITY_INFO, 12, "boot_complete", &kv, 1};
    ls_breadcrumb_event(&breadcrumb);
    ls_metric_i32("battery_mv", 3264);
    ls_metric_i32("battery_mv", 3200);
    ls_metric_snapshot_t metric;
    CHECK(ls_metric_get("battery_mv", &metric) == LS_OK);
    CHECK(metric.previous == 3264);
    CHECK(metric.average == 3232);
    ls_power_sample_t power = {
        .vdd_mv = 3300, .battery_mv = 3200, .current_ma = 120, .temperature_c = 41};
    ls_power_sample(&power);
    CHECK(ls_power_sample_count() == 1);
    ls_capture_message("hello", LS_SEVERITY_ERROR);
    CHECK(ls_flush() == LS_OK);
    CHECK(sent_length > LS_LEP_HEADER_SIZE);
    CHECK(sent[0] == 'L' && sent[1] == 'S' && sent[2] == 'T' && sent[3] == 'P');
    ls_envelope_info_t info;
    CHECK(ls_envelope_validate(sent, sent_length, &info) == LS_OK);
    visitor_state_t visited = {0};
    CHECK(ls_envelope_visit(sent, sent_length, visit, &visited) == LS_OK);
    CHECK(visited.fields >= 6);
    CHECK(ls_build_id_validate(ls_build_id()));
    uint8_t corrupted[LS_MAX_EVENT_SIZE];
    memcpy(corrupted, sent, sent_length);
    corrupted[sent_length - 1] ^= 0x80;
    CHECK(ls_envelope_validate(corrupted, sent_length, 0) == LS_ECORRUPT);
    uint8_t secret[32];
    memset(secret, 0x5a, sizeof secret);
    CHECK(ls_dump_region_register("secret", secret, sizeof secret,
                                  LS_DUMP_SAFE | LS_DUMP_SENSITIVE) == LS_OK);
    CHECK(ls_memory_redact(secret, sizeof secret, LS_REDACT_HASH) == LS_OK);
    CHECK(ls_capture_coredump(LS_CAPTURE_FULL) == LS_OK);
    CHECK(ls_flush() == LS_OK);
    visited = (visitor_state_t){0};
    CHECK(ls_envelope_visit(sent, sent_length, visit, &visited) == LS_OK);
    CHECK(visited.memory_fields == 1);
    CHECK(visited.memory_length < sizeof secret + 16);
    ls_transport_clear();
    transport.max_payload = small_mtu;
    transport.send_fragment = send_fragment;
    transport.capabilities = LS_TRANSPORT_ACK | LS_TRANSPORT_FRAGMENT;
    ls_transport_register(&transport);
    ls_capture_message("fragment", LS_SEVERITY_ERROR);
    CHECK(ls_flush() == LS_OK);
    CHECK(fragments > 1);
    ls_boot_mark_successful();
    CHECK(ls_boot_count() == 1);
    CHECK(!ls_boot_loop_detected());
    puts("latch tests passed");
    return 0;
}
