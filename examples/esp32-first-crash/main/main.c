#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_idf.h"
#include "laststate/latch.h"

#define DEMO_PHASE_MAGIC 0x4c534445u
#define LOGICAL_STORAGE_SIZE 4096u

typedef struct {
    uint32_t magic;
    uint32_t phase;
} demo_rtc_state_t;

RTC_DATA_ATTR static demo_rtc_state_t rtc_state;

static ls_flash_mirror_t mirror;
static ls_storage_backend_t raw_flash;
static uint8_t flash_workspace[LOGICAL_STORAGE_SIZE];
static ls_stream_transport_t stream;
static uint8_t
    frame[LS_MAX_EVENT_SIZE + LS_STREAM_TRANSPORT_HEADER_SIZE + LS_STREAM_TRANSPORT_TRAILER_SIZE];
static size_t frame_used;
static nvs_handle_t acknowledgements;

static uint32_t timestamp_ms(void *context) {
    (void)context;
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static ls_result_t esp_result(esp_err_t result) {
    return result == ESP_OK ? LS_OK : LS_EIO;
}

static ls_result_t flash_read(void *context, size_t offset, void *destination, size_t length) {
    return esp_result(
        esp_partition_read((const esp_partition_t *)context, offset, destination, length));
}

static ls_result_t flash_write(void *context, size_t offset, const void *source, size_t length) {
    return esp_result(
        esp_partition_write((const esp_partition_t *)context, offset, source, length));
}

static ls_result_t flash_erase(void *context, size_t offset, size_t length) {
    return esp_result(esp_partition_erase_range((const esp_partition_t *)context, offset, length));
}

static ls_result_t flash_sync(void *context) {
    (void)context;
    return LS_OK;
}

static bool transport_available(void *context) {
    (void)context;
    return true;
}

static ls_result_t collect_frame_bytes(void *context, const uint8_t *data, size_t length) {
    (void)context;
    if (length > sizeof frame - frame_used) {
        frame_used = 0;
        return LS_ENOSPACE;
    }
    memcpy(frame + frame_used, data, length);
    frame_used += length;
    return LS_OK;
}

/* A zero-dependency collector used only by this first-run tutorial. It parses
   the public wire frame and commits the ACK event ID to NVS before returning
   success. Replace it with the public Python collector or your real transport
   without changing Latch's capture/storage code. */
static ls_result_t persist_ack(void *context, uint32_t expected_event_id, uint32_t timeout_ms) {
    (void)context;
    (void)timeout_ms;
    ls_stream_frame_t parsed;
    ls_result_t result = ls_stream_frame_parse(frame, frame_used, LS_MAX_EVENT_SIZE, &parsed);
    if (result != LS_OK) {
        frame_used = 0;
        return result;
    }
    ls_envelope_info_t info;
    result = ls_envelope_validate(parsed.envelope, parsed.envelope_length, &info);
    if (result != LS_OK || info.event_id != expected_event_id) {
        frame_used = 0;
        return LS_ECORRUPT;
    }
    uint32_t previous = 0;
    bool duplicate = nvs_get_u32(acknowledgements, "last_event", &previous) == ESP_OK &&
                     previous == expected_event_id;
    if (!duplicate && (nvs_set_u32(acknowledgements, "last_event", expected_event_id) != ESP_OK ||
                       nvs_commit(acknowledgements) != ESP_OK)) {
        frame_used = 0;
        return LS_EIO;
    }
    printf("5. ACK stored in NVS: event=%08x status=%s\n", (unsigned)expected_event_id,
           duplicate ? "duplicate" : "stored");
    frame_used = 0;
    return LS_OK;
}

static bool init_latch(void) {
    const esp_partition_t *partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, "latch");
    if (!partition) {
        puts("Latch demo: dedicated partition not found");
        return false;
    }
    raw_flash = (ls_storage_backend_t){
        .name = "esp-partition",
        .context = (void *)partition,
        .capacity = partition->size,
        .erase_size = 4096,
        .write_size = 1,
        .read = flash_read,
        .write = flash_write,
        .erase = flash_erase,
        .sync = flash_sync,
    };
    if (ls_flash_mirror_init(&mirror, &raw_flash, flash_workspace, sizeof flash_workspace) !=
        LS_OK) {
        puts("Latch demo: flash mirror initialization failed");
        return false;
    }
    static const ls_identity_t identity = {
        .project_id = "esp32-first-crash",
        .device_id = "tutorial-board",
        .product = "Latch ESP32 tutorial",
        .firmware_version = "0.2.0",
        .firmware_build_id = "esp32-first-crash-0001",
        .architecture = "xtensa",
        .rtos = "freertos",
    };
    ls_config_t config = {.identity = &identity,
                          .architecture = LS_ARCH_XTENSA,
                          .rtos = "freertos",
                          .timestamp_ms = timestamp_ms,
                          .reset_info = ls_esp_idf_reset_info};
    if (ls_init(&config) != LS_OK)
        return false;
    ls_capture_minimal_prepare();
    ls_storage_register(&mirror.backend);
    return ls_boot() == LS_OK;
}

static void register_loopback_collector(void) {
    stream = (ls_stream_transport_t){.write = collect_frame_bytes,
                                     .wait_ack = persist_ack,
                                     .maximum_envelope = LS_MAX_EVENT_SIZE,
                                     .ack_timeout_ms = 1000};
    ESP_ERROR_CHECK(ls_stream_transport_init(&stream) == LS_OK ? ESP_OK : ESP_FAIL);
    static ls_transport_backend_t transport;
    transport = (ls_transport_backend_t){
        .name = "tutorial-loopback",
        .priority = 100,
        .available = transport_available,
        .send = ls_stream_transport_send,
        .max_payload = ls_stream_transport_max_payload,
        .context = &stream,
        .capabilities = LS_TRANSPORT_STREAM | LS_TRANSPORT_DURABLE_ACK,
    };
    ls_transport_register(&transport);
}

static void first_boot(void) {
    ls_breadcrumb("esp32_booted");
    ls_metric_u32("supply_mv", 3300);
    ls_capture_message("saved immediately before intentional panic", LS_SEVERITY_ERROR);
    puts("1. Event recorded in the flash-backed Latch spool");
    rtc_state.phase = 1;
    puts("2. Intentional panic now (abort)");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(250));
    abort();
}

static void recovery_boot(void) {
    printf("3. Reboot detected: reset_reason=%u previous_crash=%u\n", (unsigned)esp_reset_reason(),
           ls_previous_boot_crashed() ? 1u : 0u);
    puts("4. Recovering the queued event and sending its LS frame");
    register_loopback_collector();
    ls_result_t result = ls_flush();
    if (result != LS_OK) {
        printf("Latch demo failed while flushing: %d\n", (int)result);
        return;
    }
    ls_boot_mark_successful();
    rtc_state.phase = 2;
    puts("PASS: crash -> reboot -> recovery -> durable ACK");
}

void app_main(void) {
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);
    ESP_ERROR_CHECK(nvs_open("latch_demo", NVS_READWRITE, &acknowledgements));

    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_PANIC || reason == ESP_RST_SW) {
        rtc_state.magic = DEMO_PHASE_MAGIC;
        rtc_state.phase = 1;
    } else if (rtc_state.magic != DEMO_PHASE_MAGIC) {
        rtc_state.magic = DEMO_PHASE_MAGIC;
        rtc_state.phase = 0;
    }

    puts("\n=== Latch ESP32 first crash ===");
    if (!init_latch()) {
        puts("Latch demo initialization failed");
        return;
    }
    if (rtc_state.phase == 0)
        first_boot();
    else if (rtc_state.phase == 1)
        recovery_boot();
    else
        puts("Demo already completed. Run `idf.py erase-flash` to repeat it.");
}
