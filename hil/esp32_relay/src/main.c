#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_idf.h"
#include "laststate/latch.h"

#define HIL_UART UART_NUM_0
#define HIL_BAUD 115200
#define HIL_LOGICAL_STORAGE_SIZE 4096u
#define HIL_PHASE_MAGIC 0x4c534849u

typedef struct {
    uint32_t magic;
    uint32_t phase;
} hil_rtc_state_t;

RTC_DATA_ATTR static hil_rtc_state_t rtc_state;

static const esp_partition_t *latch_partition;
static ls_storage_backend_t raw_flash;
static ls_flash_mirror_t flash_mirror;
static uint8_t flash_workspace[HIL_LOGICAL_STORAGE_SIZE];
static ls_stream_transport_t stream;

static uint32_t timestamp_ms(void *context) {
    (void)context;
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static ls_result_t flash_result(esp_err_t result) {
    return result == ESP_OK ? LS_OK : LS_EIO;
}

static ls_result_t raw_read(void *context, size_t offset, void *destination, size_t length) {
    const esp_partition_t *partition = (const esp_partition_t *)context;
    return flash_result(esp_partition_read(partition, offset, destination, length));
}

static ls_result_t raw_write(void *context, size_t offset, const void *source, size_t length) {
    const esp_partition_t *partition = (const esp_partition_t *)context;
    return flash_result(esp_partition_write(partition, offset, source, length));
}

static ls_result_t raw_erase(void *context, size_t offset, size_t length) {
    const esp_partition_t *partition = (const esp_partition_t *)context;
    return flash_result(esp_partition_erase_range(partition, offset, length));
}

static ls_result_t raw_sync(void *context) {
    (void)context;
    return LS_OK;
}

static bool transport_available(void *context) {
    (void)context;
    return true;
}

static ls_result_t stream_write(void *context, const uint8_t *data, size_t length) {
    (void)context;
    int written = uart_write_bytes(HIL_UART, data, length);
    if (written < 0 || (size_t)written != length) {
        return LS_EIO;
    }
    return uart_wait_tx_done(HIL_UART, pdMS_TO_TICKS(2000)) == ESP_OK ? LS_OK : LS_EAGAIN;
}

static ls_result_t ack_status_result(uint8_t status) {
    if (ls_lsak_is_success(status)) {
        return LS_OK;
    }
    if (status == LS_LSAK_NACK_BUSY) {
        return LS_EAGAIN;
    }
    if (status == LS_LSAK_NACK_TOO_LARGE) {
        return LS_ENOSPACE;
    }
    if (status == LS_LSAK_NACK_UNAUTHORIZED) {
        return LS_EAUTH;
    }
    if (status == LS_LSAK_NACK_UNSUPPORTED) {
        return LS_ENOTSUP;
    }
    return status == LS_LSAK_NACK_CORRUPT ? LS_ECORRUPT : LS_EIO;
}

static ls_result_t wait_for_ack(void *context, uint32_t event_id, uint32_t timeout_ms) {
    (void)context;
    uint8_t window[LS_LSAK_SIZE] = {0};
    size_t used = 0;
    int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;

    while (esp_timer_get_time() < deadline_us) {
        uint8_t byte = 0;
        int received = uart_read_bytes(HIL_UART, &byte, 1, pdMS_TO_TICKS(20));
        if (received != 1) {
            continue;
        }
        if (used < sizeof window) {
            window[used++] = byte;
        } else {
            memmove(window, window + 1, sizeof window - 1);
            window[sizeof window - 1] = byte;
        }
        if (used == sizeof window) {
            ls_lsak_t ack;
            if (ls_lsak_parse(window, sizeof window, &ack) == LS_OK && ack.event_id == event_id) {
                return ack_status_result(ack.status);
            }
        }
    }
    return LS_EAGAIN;
}

static void init_uart(void) {
    uart_config_t config = {
        .baud_rate = HIL_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(HIL_UART, &config));
    esp_err_t result = uart_driver_install(HIL_UART, 2048, 0, 0, NULL, 0);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(result);
    }
    uart_flush_input(HIL_UART);
}

static bool init_latch(void) {
    latch_partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, "latch");
    if (!latch_partition) {
        printf("HIL:FAIL:partition-not-found\n");
        return false;
    }

    raw_flash = (ls_storage_backend_t){
        .name = "esp-partition",
        .context = (void *)latch_partition,
        .capacity = latch_partition->size,
        .erase_size = 4096,
        .write_size = 1,
        .read = raw_read,
        .write = raw_write,
        .erase = raw_erase,
        .sync = raw_sync,
    };
    ls_result_t result =
        ls_flash_mirror_init(&flash_mirror, &raw_flash, flash_workspace, sizeof flash_workspace);
    if (result != LS_OK) {
        printf("HIL:FAIL:flash-mirror:%d\n", (int)result);
        return false;
    }
    if (ls_storage_required_size() > flash_mirror.backend.capacity) {
        printf("HIL:FAIL:storage-size:%u>%u\n", (unsigned)ls_storage_required_size(),
               (unsigned)flash_mirror.backend.capacity);
        return false;
    }

    static const ls_identity_t identity = {
        .project_id = "latch-hil",
        .device_id = "esp32-com3",
        .product = "ESP32 Latch+Relay HIL",
        .hardware_revision = "ESP32-D0WD-V3-r3.1",
        .firmware_version = "0.2.0",
        .firmware_build_id = "esp32-relay-hil-0001",
        .architecture = "xtensa",
        .rtos = "freertos",
    };
    const ls_config_t config = {
        .identity = &identity,
        .architecture = LS_ARCH_XTENSA,
        .rtos = "freertos",
        .timestamp_ms = timestamp_ms,
        .reset_info = ls_esp_idf_reset_info,
    };
    result = ls_init(&config);
    if (result != LS_OK) {
        printf("HIL:FAIL:init:%d\n", (int)result);
        return false;
    }
    ls_storage_register(&flash_mirror.backend);
    result = ls_boot();
    if (result != LS_OK) {
        printf("HIL:FAIL:boot:%d\n", (int)result);
        return false;
    }
    return true;
}

static bool init_stream_transport(void) {
    stream = (ls_stream_transport_t){
        .write = stream_write,
        .wait_ack = wait_for_ack,
        .maximum_envelope = LS_MAX_EVENT_SIZE,
        .ack_timeout_ms = 3000,
    };
    if (ls_stream_transport_init(&stream) != LS_OK) {
        return false;
    }
    static ls_transport_backend_t serial_transport;
    serial_transport = (ls_transport_backend_t){
        .name = "uart0-latch-stream",
        .priority = 100,
        .available = transport_available,
        .send = ls_stream_transport_send,
        .max_payload = ls_stream_transport_max_payload,
        .context = &stream,
        .capabilities = LS_TRANSPORT_STREAM | LS_TRANSPORT_DURABLE_ACK,
        .retry_limit = 0,
    };
    ls_transport_register(&serial_transport);
    return true;
}

static void run_capture_then_panic(void) {
    printf("HIL:PHASE0:capture-before-panic boot=%u required_storage=%u\n",
           (unsigned)ls_boot_count(), (unsigned)ls_storage_required_size());
    ls_breadcrumb("physical_esp32_booted");
    ls_health_register("relay-link", 30000);
    ls_health_touch("relay-link");
    ls_metric_u32("supply_mv", 3300);
    ls_metric_increment("hil_runs", 1);
    const ls_power_sample_t power = {
        .vdd_mv = 3300,
        .battery_mv = 0,
        .current_ma = 80,
        .temperature_c = 25,
    };
    ls_power_sample(&power);
    ls_watchdog_checkpoint(0x321);
    ls_capture_message("esp32-before-intentional-panic", LS_SEVERITY_ERROR);

    rtc_state.phase = 1;
    printf("HIL:PHASE0:event-durable; triggering abort\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(250));
    abort();
}

static void run_recovery_and_delivery(void) {
    ls_reset_info_t reset = ls_get_reset_info();
    printf("HIL:PHASE1:recovered boot=%u reset=%u raw=%u previous_crash=%u\n",
           (unsigned)ls_boot_count(), (unsigned)reset.reason, (unsigned)reset.raw_reason,
           ls_previous_boot_crashed() ? 1u : 0u);
    printf("HIL:WAITING_FOR_RELAY:15s\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(15000));
    init_uart();
    if (!init_stream_transport()) {
        printf("HIL:FAIL:stream-init\n");
        return;
    }

    ls_result_t result = LS_EAGAIN;
    for (unsigned attempt = 1; attempt <= 20; ++attempt) {
        result = ls_flush();
        if (result == LS_OK) {
            break;
        }
        printf("HIL:RETRY:flush-pre-panic attempt=%u result=%d\n", attempt, (int)result);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    if (result != LS_OK) {
        printf("HIL:FAIL:pre-panic-delivery:%d\n", (int)result);
        return;
    }

    ls_breadcrumb("pre_panic_event_acked_by_relay");
    ls_metric_increment("durable_acks", 1);
    ls_capture_message("esp32-post-recovery-confirmation", LS_SEVERITY_INFO);
    result = ls_flush();
    if (result != LS_OK) {
        printf("HIL:FAIL:post-recovery-delivery:%d\n", (int)result);
        return;
    }
    ls_boot_mark_successful();
    rtc_state.phase = 2;
    printf("HIL:PASS:LATCH_RELAY_ESP32\n");
}

void app_main(void) {
    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_SW || reason == ESP_RST_PANIC) {
        rtc_state.magic = HIL_PHASE_MAGIC;
        rtc_state.phase = 1;
    } else if (rtc_state.magic != HIL_PHASE_MAGIC || reason == ESP_RST_POWERON ||
               reason == ESP_RST_EXT) {
        rtc_state.magic = HIL_PHASE_MAGIC;
        rtc_state.phase = 0;
    }

    printf("\nHIL:START:phase=%u esp_reset_reason=%u\n", (unsigned)rtc_state.phase,
           (unsigned)reason);
    if (!init_latch()) {
        return;
    }
    if (rtc_state.phase == 0) {
        run_capture_then_panic();
    } else if (rtc_state.phase == 1) {
        run_recovery_and_delivery();
    } else {
        printf("HIL:IDLE:previous-run-passed\n");
    }
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (rtc_state.phase == 2) {
            printf("HIL:PASS:LATCH_RELAY_ESP32:heartbeat\n");
            fflush(stdout);
        }
    }
}
