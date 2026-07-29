#include <stdio.h>
#include "laststate/latch.h"

#define DEMO_ENVELOPE_PATH "latch-demo.lst"

static uint32_t ticks;
static size_t captured_bytes;

static uint32_t time_ms(void *unused) {
    (void)unused;
    return ticks;
}

static bool file_available(void *unused) {
    (void)unused;
    return true;
}

static ls_result_t file_send(void *unused, const uint8_t *bytes, size_t count) {
    (void)unused;
    FILE *output = fopen(DEMO_ENVELOPE_PATH, "ab");
    if (!output)
        return LS_EIO;
    size_t written = fwrite(bytes, 1, count, output);
    if (fclose(output) != 0 || written != count)
        return LS_EIO;
    captured_bytes += written;
    return LS_OK;
}

static size_t file_mtu(void *unused) {
    (void)unused;
    return LS_MAX_EVENT_SIZE;
}

int main(void) {
    if (remove(DEMO_ENVELOPE_PATH) != 0) {
        FILE *existing = fopen(DEMO_ENVELOPE_PATH, "rb");
        if (existing) {
            fclose(existing);
            fputs("could not replace " DEMO_ENVELOPE_PATH "\n", stderr);
            return 1;
        }
    }
    static uint8_t storage_bytes[40000];
    if (sizeof storage_bytes < ls_storage_required_size()) {
        fputs("demo storage is smaller than ls_storage_required_size()\n", stderr);
        return 1;
    }
    ls_memory_storage_t ram = {storage_bytes, sizeof storage_bytes};
    ls_identity_t id = {.project_id = "demo",
                        .device_id = "host-01",
                        .product = "Latch demo",
                        .firmware_version = "0.1.0",
                        .firmware_build_id = "demo-build-0001"};
    ls_config_t cfg = {.identity = &id, .timestamp_ms = time_ms};
    ls_storage_backend_t storage = {.name = "memory",
                                    .context = &ram,
                                    .capacity = sizeof storage_bytes,
                                    .read = ls_memory_storage_read,
                                    .write = ls_memory_storage_write,
                                    .erase = ls_memory_storage_erase};
    ls_transport_backend_t file = {.name = "file",
                                   .priority = 10,
                                   .available = file_available,
                                   .send = file_send,
                                   .max_payload = file_mtu};
    if (ls_init(&cfg) != LS_OK) {
        fputs("Latch initialization failed\n", stderr);
        return 1;
    }
    ls_storage_register(&storage);
    ls_transport_register(&file);
    if (ls_boot() != LS_OK) {
        fputs("Latch boot recovery failed\n", stderr);
        return 1;
    }
    ls_breadcrumb("boot_complete");
    ticks = 155;
    ls_metric_u32("battery_mv", 3264);
    ls_capture_message("sensor timeout", LS_SEVERITY_ERROR);
    if (ls_flush() != LS_OK) {
        fputs("Latch delivery failed\n", stderr);
        return 1;
    }
    printf("Captured %zu-byte LEP envelope in %s\n", captured_bytes, DEMO_ENVELOPE_PATH);
    return 0;
}
