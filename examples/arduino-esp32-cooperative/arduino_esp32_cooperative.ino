#include <Arduino.h>

extern "C" {
#include <laststate/latch.h>
}

/*
 * This deliberately uses volatile RAM and an explicit error capture. It is a
 * packaging and integration example, not an ESP32 panic handler or a
 * crash-survives-reboot demonstration.
 */
static uint8_t storage_bytes[40000];

static uint32_t timestamp_ms(void *context) {
    (void)context;
    return (uint32_t)millis();
}

static bool serial_available(void *context) {
    (void)context;
    return true;
}

static ls_result_t serial_send(void *context, const uint8_t *data, size_t length) {
    (void)context;
    ls_envelope_info_t envelope;
    if (ls_envelope_validate(data, length, &envelope) != LS_OK) {
        return LS_ECORRUPT;
    }
    Serial.printf("Latch delivered LEP v%u event=%08lx bytes=%u\n", (unsigned)envelope.version,
                  (unsigned long)envelope.event_id, (unsigned)length);
    return LS_OK;
}

static size_t serial_maximum(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static void fail(const char *step, ls_result_t result) {
    Serial.printf("Latch %s failed: %d\n", step, (int)result);
    for (;;) {
        delay(1000);
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);

    static const ls_identity_t identity = {
        "arduino-esp32-cooperative",
        "esp32-board",
        "Latch Arduino example",
        NULL,
        NULL,
        NULL,
        LS_VERSION_STRING,
        "arduino-example-0001",
        NULL,
        NULL,
        NULL,
        "xtensa",
        "arduino",
        NULL,
        NULL,
    };
    ls_memory_storage_t memory = {storage_bytes, sizeof storage_bytes};
    ls_storage_backend_t storage = {};
    storage.name = "ram-only";
    storage.context = &memory;
    storage.capacity = sizeof storage_bytes;
    storage.read = ls_memory_storage_read;
    storage.write = ls_memory_storage_write;
    storage.erase = ls_memory_storage_erase;

    ls_transport_backend_t serial = {};
    serial.name = "serial";
    serial.priority = 1u;
    serial.available = serial_available;
    serial.send = serial_send;
    serial.max_payload = serial_maximum;

    ls_config_t config = {};
    config.identity = &identity;
    config.architecture = LS_ARCH_XTENSA;
    config.rtos = "arduino";
    config.timestamp_ms = timestamp_ms;

    ls_result_t result = ls_init(&config);
    if (result != LS_OK) {
        fail("initialization", result);
    }
    if (sizeof storage_bytes < ls_storage_required_size()) {
        Serial.println("Latch RAM backend is smaller than the active spool profile");
        for (;;) {
            delay(1000);
        }
    }

    ls_storage_register(&storage);
    ls_transport_register(&serial);
    result = ls_boot();
    if (result != LS_OK) {
        fail("boot", result);
    }

    ls_breadcrumb("arduino_setup_complete");
    ls_metric_u32("free_heap_hint", (uint32_t)ESP.getFreeHeap());
    ls_capture_message("explicit Arduino error capture", LS_SEVERITY_ERROR);
    result = ls_flush();
    if (result != LS_OK) {
        fail("flush", result);
    }
    Serial.println("Latch cooperative capture complete");
}

void loop() {
    delay(1000);
}
