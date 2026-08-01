#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#if defined(CONFIG_ARCH_POSIX)
#include <nsi_main.h>
#endif
#include "laststate/latch.h"

static uint8_t storage_bytes[50000];
static unsigned delivered;

static int finish(int status) {
#if defined(CONFIG_ARCH_POSIX)
    nsi_exit(status);
#endif
    return status;
}

static uint32_t timestamp_ms(void *context) {
    (void)context;
    return k_uptime_get_32();
}

static bool available(void *context) {
    (void)context;
    return true;
}

static size_t maximum(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static ls_result_t print_envelope(void *context, const uint8_t *data, size_t length) {
    (void)context;
    ls_envelope_info_t info;
    if (ls_envelope_validate(data, length, &info) != LS_OK) {
        return LS_ECORRUPT;
    }
    ++delivered;
    printk("Latch: delivered LEP v%u event=%08x bytes=%u\n", info.version, info.event_id,
           (unsigned)length);
    return LS_OK;
}

int main(void) {
    static const ls_identity_t identity = {
        .project_id = "zephyr-first-capture",
        .device_id = "native-sim",
        .firmware_version = "0.2.0",
        .firmware_build_id = "zephyr-example-0001",
        .rtos = "zephyr",
    };
    ls_memory_storage_t memory = {storage_bytes, sizeof storage_bytes};
    ls_storage_backend_t storage = {
        .name = "memory",
        .context = &memory,
        .capacity = sizeof storage_bytes,
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    ls_transport_backend_t console = {
        .name = "console",
        .priority = 1,
        .available = available,
        .send = print_envelope,
        .max_payload = maximum,
    };
    ls_config_t config = {.identity = &identity, .rtos = "zephyr", .timestamp_ms = timestamp_ms};
    if (ls_init(&config) != LS_OK)
        return finish(1);
    ls_storage_register(&storage);
    ls_transport_register(&console);
    if (ls_boot() != LS_OK)
        return finish(2);
    ls_breadcrumb("zephyr_boot_complete");
    ls_metric_u32("sample_counter", 1);
    ls_capture_message("first Zephyr event", LS_SEVERITY_INFO);
    if (ls_flush() != LS_OK || delivered != 1u)
        return finish(3);
    printk("Latch: PASS first capture\n");
    return finish(0);
}
