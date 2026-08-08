#include <Arduino.h>

extern "C" {
#include <laststate/latch.h>
}

/* This sketch deliberately keeps Latch cooperative and RAM-only. The AVR
 * profile disables optional observability and uses bounded 512-byte envelopes;
 * it is not an automatic fault handler or durable crash-recovery example. */
static uint8_t storage_bytes[1024];
static ls_memory_storage_t memory = {storage_bytes, sizeof(storage_bytes)};
static ls_storage_backend_t storage = {};
static ls_transport_backend_t serial_transport = {};

static bool serial_available(void *context) {
    (void)context;
    return true;
}

static size_t serial_maximum(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static ls_result_t serial_send(void *context, const uint8_t *data, size_t length) {
    ls_envelope_info_t info;
    (void)context;
    if (ls_envelope_validate(data, length, &info) != LS_OK)
        return LS_ECORRUPT;
    Serial.print(F("Latch event bytes="));
    Serial.println((unsigned)length);
    return LS_OK;
}

static void halt_with_error(ls_result_t result) {
    Serial.print(F("Latch failed: "));
    Serial.println((int)result);
    for (;;) {
        delay(1000);
    }
}

void setup() {
    static ls_identity_t identity = {};
    ls_config_t config = {};
    ls_result_t result;

    Serial.begin(115200);
    identity.project_id = "arduino-avr";
    identity.device_id = "mega2560";
    identity.firmware_build_id = "cooperative";
    storage.name = "ram-only";
    storage.context = &memory;
    storage.capacity = sizeof(storage_bytes);
    storage.read = ls_memory_storage_read;
    storage.write = ls_memory_storage_write;
    storage.erase = ls_memory_storage_erase;
    serial_transport.name = "serial";
    serial_transport.priority = 1u;
    serial_transport.available = serial_available;
    serial_transport.send = serial_send;
    serial_transport.max_payload = serial_maximum;
    config.identity = &identity;
    config.rtos = "arduino-avr";
    result = ls_init(&config);
    if (result != LS_OK || sizeof(storage_bytes) < ls_storage_required_size())
        halt_with_error(result == LS_OK ? LS_ENOSPACE : result);
    ls_storage_register(&storage);
    ls_transport_register(&serial_transport);
    result = ls_boot();
    if (result != LS_OK)
        halt_with_error(result);
    ls_capture_message("cooperative AVR event", LS_SEVERITY_ERROR);
    result = ls_flush();
    if (result != LS_OK)
        halt_with_error(result);
    Serial.println(F("Latch AVR cooperative capture complete"));
}

void loop() {
    delay(1000);
}
