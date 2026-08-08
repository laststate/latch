#include <stdio.h>

#include "laststate/latch.h"
#include "esp_idf.h"
#include "freertos_latch.h"
#include "rp_reset.h"
#include "zephyr_ble_gatt.h"
#include "zephyr_can.h"
#include "zephyr_latch.h"
#include "zephyr_lorawan.h"
#include "zephyr_tls_socket.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "port adapter failed: %s:%d\n", #condition, __LINE__);                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    static const ls_identity_t identity = {
        .project_id = "ports", .device_id = "host", .firmware_build_id = "ports001"};
    ls_config_t config = {.identity = &identity};
    CHECK(ls_init(&config) == LS_OK);
    CHECK(ls_boot() == LS_OK);

    ls_freertos_task_switched_in(NULL);
    ls_freertos_task_snapshot_t task = {
        .name = "control", .handle = (void *)(uintptr_t)0x1234u, .stack_high_watermark = 321u};
    ls_freertos_task_switched_in(&task);
    CHECK(ls_watchdog_last_checkpoint() == 0x1234u);
    ls_metric_snapshot_t metric;
    CHECK(ls_metric_get("task_stack_watermark", &metric) == LS_OK);
    CHECK(metric.current == 321);
    ls_freertos_stack_overflow(NULL);
    ls_freertos_stack_overflow(&task);
    ls_freertos_malloc_failed(2048u);
    ls_freertos_scheduler_state(2u, 7u, 4096u, 1024u);
    ls_heap_stats_t heap = ls_heap_stats_get();
    CHECK(heap.free_bytes == 4096u);
    CHECK(heap.minimum_free_bytes == 1024u);

    ls_zephyr_fatal(9u, NULL, 0u);
    ls_zephyr_fatal(10u, "sensor", 0x20001000u);
    ls_zephyr_thread_sample("sensor", 88u, 12345u);
    ls_zephyr_heap_failure(99u);
    CHECK(ls_metric_get("thread_stack_unused", &metric) == LS_OK);
    CHECK(metric.current == 88);

    volatile uint32_t reason = 0u;
    ls_rp_reset_port_t rp = {
        .reason_register = &reason,
        .watchdog_mask = 1u << 0,
        .forced_mask = 1u << 1,
        .brownout_mask = 1u << 2,
        .power_on_mask = 1u << 3,
    };
    CHECK(ls_rp_reset_info(NULL).reason == LS_RESET_UNKNOWN);
    CHECK(ls_rp_reset_info(&rp).reason == LS_RESET_UNKNOWN);
    reason = rp.power_on_mask;
    CHECK(ls_rp_reset_info(&rp).reason == LS_RESET_POWER_ON);
    reason = rp.brownout_mask;
    CHECK(ls_rp_reset_info(&rp).reason == LS_RESET_BROWNOUT);
    reason = rp.forced_mask;
    CHECK(ls_rp_reset_info(&rp).reason == LS_RESET_SOFTWARE);
    reason = rp.watchdog_mask | rp.forced_mask;
    CHECK(ls_rp_reset_info(&rp).reason == LS_RESET_WATCHDOG);

    CHECK(ls_esp_idf_reset_info(NULL).reason == LS_RESET_UNKNOWN);

#if !defined(__ZEPHYR__)
    ls_zephyr_ble_transport_t ble = {0};
    CHECK(ls_zephyr_ble_transport_init(&ble, NULL, NULL, NULL) == LS_ENOTSUP);
    ls_zephyr_ble_set_connection(&ble, NULL);
    ls_zephyr_ble_set_connection(NULL, NULL);

    ls_zephyr_can_transport_t can = {0};
    CHECK(ls_zephyr_can_transport_init(&can, NULL, 0u, false, false, 1u) == LS_ENOTSUP);

    ls_zephyr_lorawan_transport_t lora = {0};
    CHECK(ls_zephyr_lorawan_transport_init(&lora, 1u, 51u, false) == LS_ENOTSUP);
    ls_zephyr_lorawan_set_joined(&lora, true);
    ls_zephyr_lorawan_set_joined(NULL, false);

    ls_zephyr_tls_socket_transport_t tls = {.socket_fd = 3, .connected = true};
    uint8_t certificate = 1u;
    CHECK(ls_zephyr_tls_credential_add(1, &certificate, 1u) == LS_ENOTSUP);
    CHECK(ls_zephyr_tls_socket_connect(&tls, "example.invalid", "443", 1, true, 1024u) ==
          LS_ENOTSUP);
    ls_zephyr_tls_socket_close(&tls);
    ls_zephyr_tls_socket_close(NULL);
#endif
    return 0;
}
