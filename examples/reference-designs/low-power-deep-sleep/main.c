#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

static uint8_t persistent_spool[50000];
static int radio_online;
static unsigned delivered;

static bool radio_available(void *context) {
    (void)context;
    return radio_online != 0;
}

static size_t radio_mtu(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static ls_result_t radio_send(void *context, const uint8_t *data, size_t length) {
    (void)context;
    if (ls_envelope_validate(data, length, NULL) != LS_OK)
        return LS_ECORRUPT;
    delivered++;
    return LS_OK;
}

static int boot_cycle(void) {
    static const ls_identity_t identity = {
        .project_id = "reference-low-power",
        .device_id = "field-node-001",
        .product = "Deep-sleep sensor reference",
        .firmware_version = "1.0.0",
        .firmware_build_id = "low-power-reference-0001",
    };
    static ls_memory_storage_t memory;
    static ls_storage_backend_t storage;
    static ls_transport_backend_t radio;
    ls_config_t config = {.identity = &identity};
    memory = (ls_memory_storage_t){persistent_spool, sizeof(persistent_spool)};
    storage = (ls_storage_backend_t){
        .name = "replace-with-deep-sleep-persistent-flash",
        .context = &memory,
        .capacity = sizeof(persistent_spool),
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    radio = (ls_transport_backend_t){
        .name = "low-power-radio",
        .priority = 1,
        .available = radio_available,
        .send = radio_send,
        .max_payload = radio_mtu,
        .capabilities = LS_TRANSPORT_LOW_POWER | LS_TRANSPORT_DURABLE_ACK,
        .energy_cost = 10u,
    };
    if (ls_init(&config) != LS_OK)
        return 1;
    ls_storage_register(&storage);
    ls_transport_register(&radio);
    return ls_boot() == LS_OK ? 0 : 2;
}

int main(void) {
    memset(persistent_spool, 0xff, sizeof(persistent_spool));
    radio_online = 0;
    if (boot_cycle() != 0)
        return 1;
    ls_power_sample_t sample = {.timestamp_ms = 100u, .battery_mv = 2980u};
    ls_power_sample(&sample);
    ls_breadcrumb("sleep:radio-off");
    ls_capture_message("sensor sample deferred until next radio window", LS_SEVERITY_WARNING);
    if (ls_flush() != LS_EAGAIN || delivered != 0u)
        return 2;

    /* A real target now synchronizes flash and enters deep sleep. Static bytes
       model only the persistence boundary; they are not physical-flash HIL. */
    radio_online = 1;
    if (boot_cycle() != 0 || ls_flush() != LS_OK || delivered != 1u)
        return 3;
    puts("Deferred envelope survived a modeled deep-sleep cycle and was ACKed");
    return 0;
}
