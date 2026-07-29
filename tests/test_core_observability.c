#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "core observability check failed: %s:%d\n", #condition, __LINE__);     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint8_t storage_bytes[50000];
static uint32_t now_ms;
static unsigned reset_calls;
static unsigned assert_callback_calls;
static unsigned log_records;
static size_t last_log_length;

static uint32_t clock_ms(void *context) {
    (void)context;
    return now_ms;
}

static void reset_device(void *context) {
    (void)context;
    reset_calls++;
}

static void assert_callback(const char *expression, const char *file, int line) {
    if (expression && file && line == 77)
        assert_callback_calls++;
}

static void log_stream(void *context, const uint8_t *record, size_t length) {
    (void)context;
    if (record && length >= 4) {
        log_records++;
        last_log_length = length;
    }
}

static bool transport_available(void *context) {
    (void)context;
    return true;
}

static ls_result_t transport_send(void *context, const uint8_t *data, size_t length) {
    (void)context;
    return data && length ? LS_OK : LS_EINVAL;
}

static size_t transport_mtu(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static int setup(void) {
    static const ls_identity_t identity = {
        .project_id = "core-observability",
        .device_id = "host-test",
        .firmware_build_id = "core0001",
    };
    static ls_config_t config;
    static ls_memory_storage_t memory;
    static ls_storage_backend_t storage;
    static ls_transport_backend_t transport;

    memset(storage_bytes, 0xff, sizeof storage_bytes);
    config = (ls_config_t){
        .identity = &identity,
        .timestamp_ms = clock_ms,
        .reset = reset_device,
    };
    memory = (ls_memory_storage_t){storage_bytes, sizeof storage_bytes};
    storage = (ls_storage_backend_t){
        .name = "memory",
        .context = &memory,
        .capacity = sizeof storage_bytes,
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    transport = (ls_transport_backend_t){
        .name = "sink",
        .priority = 1,
        .available = transport_available,
        .send = transport_send,
        .max_payload = transport_mtu,
    };

    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);
    return 0;
}

static int test_health_and_power(void) {
    static const char *health_names[LS_HEALTH_CAPACITY + 1] = {
        "task00", "task01", "task02", "task03", "task04", "task05", "task06", "task07",   "task08",
        "task09", "task10", "task11", "task12", "task13", "task14", "task15", "overflow",
    };

    ls_health_register(0, 10);
    ls_health_register("invalid", 0);
    for (size_t i = 0; i < LS_HEALTH_CAPACITY + 1; i++)
        ls_health_register(health_names[i], 10);
    ls_health_register("task00", 20);
    ls_health_touch("missing");
    ls_health_set_active_task("worker");

    now_ms = 20;
    CHECK(ls_health_poll() == LS_EAGAIN);
    CHECK(ls_health_poll() == LS_OK);
    ls_health_touch("task00");
    now_ms = 41;
    CHECK(ls_health_poll() == LS_EAGAIN);

    ls_watchdog_fed();
    CHECK(ls_watchdog_last_feed_ms() == 41);
    ls_watchdog_checkpoint(42);
    CHECK(ls_watchdog_last_checkpoint() == 42);

    ls_power_sample(0);
    for (size_t i = 0; i < LS_POWER_SAMPLE_CAPACITY + 1; i++) {
        ls_power_sample_t sample = {
            .timestamp_ms = i == 0 ? 99u : 0u,
            .vdd_mv = (uint16_t)(3300u - i),
            .battery_mv = (uint16_t)(3200u - i),
            .current_ma = (int16_t)i,
            .temperature_c = 25,
        };
        ls_power_sample(&sample);
    }
    CHECK(ls_power_sample_count() == LS_POWER_SAMPLE_CAPACITY);
    return 0;
}

static int test_logging_and_asserts(void) {
    uint32_t arguments[LS_LOG_ARG_MAX + 2];
    for (size_t i = 0; i < LS_LOG_ARG_MAX + 2; i++)
        arguments[i] = (uint32_t)i;

    ls_log_set_stream(log_stream, 0);
    ls_log_set_min_level(LS_SEVERITY_WARNING);
    ls_log_binary(LS_SEVERITY_INFO, 1, 2, arguments, 2);
    CHECK(log_records == 0);
    ls_log_binary(LS_SEVERITY_ERROR, 2, 3, arguments, (uint8_t)(LS_LOG_ARG_MAX + 2));
    CHECK(log_records == 1);
    CHECK(last_log_length == 4u + LS_LOG_ARG_MAX * sizeof(uint32_t));
    ls_log_binary(LS_SEVERITY_WARNING, 3, 0, 0, 2);
    CHECK(log_records == 2);
    ls_log_set_stream(0, 0);
    ls_log_binary(LS_SEVERITY_FATAL, 4, 0, 0, 0);

    ls_assert_set_policy(LS_ASSERT_CONTINUE, 0);
    ls_assert_failed("value", "test.c", 70, 0);
    ls_assert_set_policy(LS_ASSERT_CALLBACK, assert_callback);
    ls_assert_failed("value", "test.c", 77, "callback");
    CHECK(assert_callback_calls == 1);
    ls_assert_set_policy(LS_ASSERT_CALLBACK, 0);
    ls_assert_failed("value", "test.c", 78, 0);
    ls_assert_set_policy(LS_ASSERT_RESET, 0);
    ls_assert_failed("value", "test.c", 79, "reset");
    CHECK(reset_calls == 1);
    ls_assert_set_policy(LS_ASSERT_CONTINUE, 0);
    return 0;
}

static int test_peripherals(void) {
    ls_peripheral_fault(0);
    ls_peripheral_fault_t unknown = {
        .domain = (ls_peripheral_domain_t)99,
        .fault = 7,
    };
    ls_peripheral_fault(&unknown);
    ls_i2c_fault(LS_I2C_TIMEOUT, 0x42, 0x10);
    ls_spi_fault(LS_SPI_CRC);
    ls_uart_fault(LS_UART_PARITY);
    ls_can_fault(LS_CAN_BUS_OFF, 10, 20, 0x1234);
    ls_usb_fault(LS_USB_ENDPOINT_STALL, 2, 0x55);
    ls_storage_fault(LS_STORAGE_ECC, 0x1000, 0xaa);
    ls_network_fault(LS_NET_TLS, 0xbb);
    return 0;
}

static int test_memory(void) {
    uint8_t memory[64] = {0};
    ls_heap_stats_t stats = {
        .free_bytes = 1024,
        .minimum_free_bytes = 512,
        .largest_block = 256,
    };

    CHECK(ls_dump_region_register(0, memory, sizeof memory, LS_DUMP_SAFE) == LS_EINVAL);
    CHECK(ls_dump_region_register("bad", 0, sizeof memory, LS_DUMP_SAFE) == LS_EINVAL);
    CHECK(ls_dump_region_register("bad", memory, 0, LS_DUMP_SAFE) == LS_EINVAL);
    CHECK(ls_dump_region_register("bad", memory, sizeof memory, 0) == LS_EINVAL);
    for (size_t i = 0; i < LS_MAX_DUMP_REGIONS; i++)
        CHECK(ls_dump_region_register("region", memory, sizeof memory, LS_DUMP_SAFE) == LS_OK);
    CHECK(ls_dump_region_register("overflow", memory, sizeof memory, LS_DUMP_SAFE) == LS_EINVAL);

    CHECK(ls_memory_redact(0, sizeof memory, LS_REDACT_ZERO) == LS_EINVAL);
    CHECK(ls_memory_redact(memory, 0, LS_REDACT_ZERO) == LS_EINVAL);
    ls_memory_exclude(memory, 1);
    for (size_t i = 1; i < LS_MAX_REDACTIONS; i++)
        CHECK(ls_memory_redact(memory + i, 1, LS_REDACT_HASH) == LS_OK);
    CHECK(ls_memory_redact(memory, 1, LS_REDACT_ZERO) == LS_EINVAL);

    CHECK(ls_stack_bounds_set(0, memory + sizeof memory) == LS_EINVAL);
    CHECK(ls_stack_bounds_set(memory + 1, memory) == LS_EINVAL);
    CHECK(ls_stack_bounds_set(memory, memory + sizeof memory) == LS_OK);
    CHECK(ls_capture_coredump((ls_capture_level_t)-1) == LS_EINVAL);
    CHECK(ls_capture_coredump((ls_capture_level_t)(LS_CAPTURE_FULL + 1)) == LS_EINVAL);
    CHECK(ls_capture_coredump(LS_CAPTURE_METADATA) == LS_OK);
    CHECK(ls_capture_coredump(LS_CAPTURE_FULL) == LS_OK);

    ls_heap_stats_update(0);
    ls_heap_stats_update(&stats);
    CHECK(ls_heap_stats_get().free_bytes == 1024);
    ls_memory_allocation_failed(128);
    ls_pool_exhausted("packet-pool");
    stats = ls_heap_stats_get();
    CHECK(stats.allocation_failures == 1);
    CHECK(stats.pool_exhaustions == 1);
    return 0;
}

static int test_performance(void) {
    CHECK(ls_span_end(999) == LS_EINVAL);
    for (uint16_t i = 0; i < LS_SPAN_CAPACITY; i++)
        CHECK(ls_span_begin(i) == LS_OK);
    CHECK(ls_span_begin(999) == LS_ENOSPACE);
    now_ms += 15;
    CHECK(ls_span_end(3) == LS_OK);
    CHECK(ls_span_end(3) == LS_EINVAL);
    CHECK(ls_span_begin(1000) == LS_OK);

    ls_performance_report(LS_PERF_LOOP_LATENCY, 9, 10);
    ls_performance_report(LS_PERF_LOOP_LATENCY, 10, 10);
    ls_performance_report(LS_PERF_DEADLINE_MISS, 11, 10);
    ls_performance_report(LS_PERF_DMA_TIMEOUT, 12, 10);
    return 0;
}

int main(void) {
    CHECK(setup() == 0);
    CHECK(test_health_and_power() == 0);
    CHECK(ls_flush() == LS_OK);
    CHECK(test_logging_and_asserts() == 0);
    CHECK(ls_flush() == LS_OK);
    CHECK(test_peripherals() == 0);
    CHECK(ls_flush() == LS_OK);
    CHECK(test_memory() == 0);
    CHECK(ls_flush() == LS_OK);
    CHECK(test_performance() == 0);
    puts("core observability tests passed");
    return 0;
}
