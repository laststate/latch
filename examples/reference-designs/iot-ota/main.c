#include <stdio.h>

#include "laststate/latch.h"

static uint8_t spool_bytes[50000];
static unsigned durable_acks;

static bool collector_available(void *context) {
    (void)context;
    return true;
}

static size_t collector_mtu(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static ls_result_t collector_send(void *context, const uint8_t *data, size_t length) {
    (void)context;
    if (ls_envelope_validate(data, length, NULL) != LS_OK)
        return LS_ECORRUPT;
    durable_acks++;
    return LS_OK;
}

int main(void) {
    static const ls_identity_t identity = {
        .project_id = "reference-iot-ota",
        .device_id = "sensor-001",
        .product = "OTA sensor reference",
        .firmware_version = "2.0.0-rc1",
        .firmware_build_id = "ota-reference-0001",
    };
    ls_memory_storage_t memory = {spool_bytes, sizeof(spool_bytes)};
    ls_storage_backend_t storage = {
        .name = "replace-with-atomic-flash",
        .context = &memory,
        .capacity = sizeof(spool_bytes),
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    ls_transport_backend_t collector = {
        .name = "durable-ack-collector",
        .priority = 1,
        .available = collector_available,
        .send = collector_send,
        .max_payload = collector_mtu,
        .capabilities = LS_TRANSPORT_DURABLE_ACK,
    };
    ls_config_t config = {.identity = &identity};

    if (ls_init(&config) != LS_OK || sizeof(spool_bytes) < ls_storage_required_size())
        return 1;
    ls_storage_register(&storage);
    ls_transport_register(&collector);
    if (ls_boot() != LS_OK)
        return 2;

    ls_release_mark_pending("2.0.0");
    ls_breadcrumb("ota:download-complete");
    ls_breadcrumb("ota:signature-verified");
    ls_reset_mark_expected(true);

    /* The bootloader reports that the trial image did not become healthy. */
    ls_release_mark_rollback();
    ls_capture_message("OTA trial rolled back before health confirmation", LS_SEVERITY_ERROR);
    if (ls_flush() != LS_OK || durable_acks != 1u)
        return 3;

    puts("OTA rollback evidence received with a durable ACK");
    return 0;
}
