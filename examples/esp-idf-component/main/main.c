// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// examples/esp-idf-component/main/main.c
//
// Latch runtime source. Part of the heap-free, deterministic,
// embedded failure-capture runtime that ships fault state over LEP
// to the Relay.
//
// Heap-free, bounded, deterministic.

#include <inttypes.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "esp_idf.h"
#include "laststate/latch.h"

#define LATCH_COMPONENT_STORAGE_BYTES 40000u

static const char *const tag = "latch-component";
static uint8_t storage_bytes[LATCH_COMPONENT_STORAGE_BYTES];

static uint32_t timestamp_ms(void *context) {
    (void)context;
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static bool log_available(void *context) {
    (void)context;
    return true;
}

static ls_result_t log_send(void *context, const uint8_t *data, size_t length) {
    (void)context;
    ls_envelope_info_t envelope;
    if (ls_envelope_validate(data, length, &envelope) != LS_OK) {
        return LS_ECORRUPT;
    }
    ESP_LOGI(tag, "delivered LEP v%u event=%08" PRIx32 " bytes=%u", (unsigned)envelope.version,
             envelope.event_id, (unsigned)length);
    return LS_OK;
}

static size_t log_maximum(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

void app_main(void) {
    static const ls_identity_t identity = {
        .project_id = "esp-idf-component-consumer",
        .device_id = "component-example",
        .product = "Latch Component Manager example",
        .firmware_version = LS_VERSION_STRING,
        .firmware_build_id = "component-example-0001",
        .architecture = "xtensa",
        .rtos = "freertos",
    };
    ls_memory_storage_t memory = {storage_bytes, sizeof storage_bytes};
    ls_storage_backend_t storage = {
        .name = "ram-only",
        .context = &memory,
        .capacity = sizeof storage_bytes,
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    ls_transport_backend_t log_transport = {
        .name = "esp-log",
        .priority = 1u,
        .available = log_available,
        .send = log_send,
        .max_payload = log_maximum,
    };
    ls_config_t config = {
        .identity = &identity,
        .architecture = LS_ARCH_XTENSA,
        .rtos = "freertos",
        .timestamp_ms = timestamp_ms,
        .reset_info = ls_esp_idf_reset_info,
    };

    if (sizeof storage_bytes < ls_storage_required_size()) {
        ESP_LOGE(tag, "RAM backend is smaller than the active Latch spool profile");
        return;
    }
    if (ls_init(&config) != LS_OK) {
        ESP_LOGE(tag, "Latch initialization failed");
        return;
    }
    ls_storage_register(&storage);
    ls_transport_register(&log_transport);
    if (ls_boot() != LS_OK) {
        ESP_LOGE(tag, "Latch boot recovery failed");
        return;
    }

    ls_breadcrumb("component_manager_ready");
    ls_capture_message("explicit Component Manager capture", LS_SEVERITY_ERROR);
    if (ls_flush() != LS_OK) {
        ESP_LOGE(tag, "Latch flush failed");
        return;
    }
    ESP_LOGI(tag, "Latch Component Manager integration passed");
}
