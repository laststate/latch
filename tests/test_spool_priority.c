#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "spool priority failed: %s:%d\n", #condition, __LINE__);               \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint8_t storage_bytes[50000];
static int transport_online;
static int transport_fail;
static unsigned delivered;
static uint8_t delivered_priorities[LS_SPOOL_MAX_RECORDS + 4u];

static bool available(void *context) {
    (void)context;
    return transport_online != 0;
}

static size_t mtu(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static ls_result_t visit_priority(void *context, uint16_t type, const uint8_t *value,
                                  uint16_t length) {
    (void)context;
    if (type == LS_TLV_EVENT && length >= 1u && delivered < sizeof(delivered_priorities)) {
        delivered_priorities[delivered] = value[0];
    }
    return LS_OK;
}

static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    (void)context;
    CHECK(data != NULL);
    CHECK(length > LS_LEP_HEADER_SIZE);
    if (transport_fail) {
        return LS_EIO;
    }
    CHECK(ls_envelope_visit(data, length, visit_priority, NULL) == LS_OK);
    delivered++;
    return LS_OK;
}

static int configure(void) {
    static const ls_identity_t identity = {
        .project_id = "spool-priority",
        .device_id = "host",
        .firmware_build_id = "priority1",
    };
    static ls_memory_storage_t memory;
    static ls_storage_backend_t storage;
    static ls_transport_backend_t transport;

    memory = (ls_memory_storage_t){storage_bytes, sizeof storage_bytes};
    storage = (ls_storage_backend_t){
        .name = "ram",
        .context = &memory,
        .capacity = sizeof storage_bytes,
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    transport = (ls_transport_backend_t){
        .name = "sink",
        .priority = 1u,
        .available = available,
        .send = send_data,
        .max_payload = mtu,
    };
    ls_config_t config = {.identity = &identity};
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);
    return 0;
}

static int critical_reserve_survives_normal_saturation(void) {
    memset(storage_bytes, 0xff, sizeof storage_bytes);
    transport_online = 0;
    transport_fail = 0;
    delivered = 0u;
    memset(delivered_priorities, 0xff, sizeof delivered_priorities);
    CHECK(configure() == 0);

    const size_t normal_capacity = LS_SPOOL_MAX_RECORDS - LS_SPOOL_RESERVED_CRITICAL;
    for (size_t index = 0u; index < normal_capacity; index++) {
        ls_error_t error = {
            .domain = "normal",
            .code = (int32_t)(index + 1u),
            .severity = LS_SEVERITY_WARNING,
            .message = "normal event",
        };
        ls_capture_error(&error);
    }

    ls_spool_stats_t stats;
    CHECK(ls_spool_get_stats(&stats) == LS_OK);
    CHECK(stats.capacity == LS_SPOOL_MAX_RECORDS);
    CHECK(stats.reserved_critical == LS_SPOOL_RESERVED_CRITICAL);
    CHECK(stats.committed == normal_capacity);

#if LS_SPOOL_RESERVED_CRITICAL > 0
    ls_error_t overflow = {
        .domain = "normal",
        .code = 1000,
        .severity = LS_SEVERITY_WARNING,
        .message = "must be dropped before reserve",
    };
    ls_capture_error(&overflow);
    CHECK(ls_spool_get_stats(&stats) == LS_OK);
    CHECK(stats.committed == normal_capacity);
    CHECK(stats.dropped_records == 1u);

    ls_arch_context_t context = {
        .architecture = LS_ARCH_CORTEX_M,
        .fault = LS_FAULT_HARD,
        .pc = 0x08001234u,
        .lr = 0x08005678u,
    };
    /* Critical may use the critical reserve, but not the final emergency slot. */
    ls_assert_set_policy(LS_ASSERT_CONTINUE, NULL);
    ls_assert_failed("control_invariant", __FILE__, __LINE__, "critical evidence");
    CHECK(ls_spool_get_stats(&stats) == LS_OK);
    CHECK(stats.committed == normal_capacity + 1u);
    CHECK(stats.reserved_emergency == LS_SPOOL_RESERVED_EMERGENCY);
#if LS_SPOOL_RESERVED_EMERGENCY > 0
    ls_assert_failed("second_control_invariant", __FILE__, __LINE__, "must preserve emergency");
    CHECK(ls_spool_get_stats(&stats) == LS_OK);
    CHECK(stats.committed == normal_capacity + 1u);
#endif

    CHECK(ls_capture_cpu_context(&context) == LS_OK);
    CHECK(ls_spool_get_stats(&stats) == LS_OK);
    CHECK(stats.committed == normal_capacity + 2u);
    CHECK(stats.high_watermark >= normal_capacity + 2u);

    transport_online = 1;
    CHECK(ls_flush() == LS_OK);
    CHECK(delivered == normal_capacity + 2u);
    CHECK(delivered_priorities[0] == LS_PRIORITY_EMERGENCY);
    CHECK(delivered_priorities[1] == LS_PRIORITY_CRITICAL);
    for (size_t index = 2u; index < delivered; ++index) {
        CHECK(delivered_priorities[index] >= LS_PRIORITY_WARNING);
    }
#endif
    return 0;
}

static int retry_saturation_is_observable(void) {
    memset(storage_bytes, 0xff, sizeof storage_bytes);
    transport_online = 1;
    transport_fail = 1;
    delivered = 0u;
    CHECK(configure() == 0);
    ls_capture_message("retry me", LS_SEVERITY_ERROR);

    for (unsigned attempt = 0u; attempt < 9u; attempt++) {
        CHECK(ls_flush() == LS_EIO);
    }

    ls_spool_stats_t stats;
    CHECK(ls_spool_get_stats(&stats) == LS_OK);
    CHECK(stats.committed == 1u);
    CHECK(stats.transport_failures == 9u);
    CHECK(stats.retry_saturated == 1u);

    transport_fail = 0;
    CHECK(ls_flush() == LS_OK);
    CHECK(delivered == 1u);
    CHECK(ls_spool_get_stats(&stats) == LS_OK);
    CHECK(stats.committed == 0u);
    return 0;
}

int main(void) {
    CHECK(ls_spool_get_stats(NULL) == LS_EINVAL);
    CHECK(critical_reserve_survives_normal_saturation() == 0);
    CHECK(retry_saturation_is_observable() == 0);
    return 0;
}
