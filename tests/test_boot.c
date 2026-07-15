#include <stdio.h>
#include "laststate/latch.h"
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "check failed: %s line %d\n", #condition, __LINE__);                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint8_t retained[40000];
static uint32_t time_ms;
static uint32_t clock_ms(void *unused) {
    (void)unused;
    return time_ms;
}
static bool offline(void *unused) {
    (void)unused;
    return false;
}
static ls_result_t sink(void *unused, const uint8_t *data, size_t length) {
    (void)unused;
    (void)data;
    (void)length;
    return LS_OK;
}
static size_t mtu(void *unused) {
    (void)unused;
    return LS_MAX_EVENT_SIZE;
}
static int configure(ls_identity_t *identity, ls_storage_backend_t *storage,
                     ls_transport_backend_t *transport) {
    ls_config_t config = {
        .identity = identity, .architecture = LS_ARCH_CORTEX_M, .timestamp_ms = clock_ms};
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(storage);
    ls_transport_register(transport);
    CHECK(ls_boot() == LS_OK);
    return 0;
}
int main(void) {
    ls_identity_t identity = {
        .project_id = "boot", .device_id = "loop", .firmware_build_id = "bootloop1"};
    ls_memory_storage_t memory = {retained, sizeof retained};
    ls_storage_backend_t storage = {.name = "retained",
                                    .context = &memory,
                                    .capacity = sizeof retained,
                                    .read = ls_memory_storage_read,
                                    .write = ls_memory_storage_write,
                                    .erase = ls_memory_storage_erase};
    ls_transport_backend_t transport = {
        .name = "offline", .priority = 1, .available = offline, .send = sink, .max_payload = mtu};

    ls_minimal_snapshot_clear();
    ls_capture_minimal_fault(0x0800f001u, 0x0800f101u, 0x20001000u, 0x20002000u, 0x82u, 0x40u,
                             LS_FAULT_HARD, 0xfffffff9u, 0x21000000u, 0u,
                             LS_MINIMAL_SNAPSHOT_FRAME_VALID, 0u, 1u);
    ls_config_t no_storage_config = {.identity = &identity, .architecture = LS_ARCH_CORTEX_M};
    CHECK(ls_init(&no_storage_config) == LS_OK);
    CHECK(ls_boot() == LS_OK);
    ls_minimal_snapshot_t snapshot;
    CHECK(ls_minimal_snapshot_read(&snapshot));
    CHECK(configure(&identity, &storage, &transport) == 0);
    CHECK(ls_previous_boot_crashed());
    CHECK(!ls_minimal_snapshot_read(&snapshot));

    for (unsigned crash = 0; crash < 3; crash++) {
        CHECK(configure(&identity, &storage, &transport) == 0);
        time_ms += 100;
        ls_arch_context_t context = {0};
        context.architecture = LS_ARCH_CORTEX_M;
        context.fault = LS_FAULT_HARD;
        context.pc = 0x08001000u + crash * 4;
        context.lr = 0x08002000u;
        context.cfsr = crash + 1;
        CHECK(ls_capture_cpu_context(&context) == LS_OK);
    }
    CHECK(configure(&identity, &storage, &transport) == 0);
    CHECK(ls_previous_boot_crashed());
    CHECK(ls_boot_loop_detected());
    CHECK(ls_safe_mode_requested());
    CHECK(ls_boot_count() == 5);
    ls_boot_mark_successful();
    return 0;
}
