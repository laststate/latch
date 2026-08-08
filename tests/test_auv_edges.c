#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "auv edge check failed: %s:%d\n", #x, __LINE__);                       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint32_t now_ms;
static uint8_t storage_bytes[64000];

static uint32_t clock_ms(void *context) {
    (void)context;
    return now_ms;
}
static bool online(void *context) {
    (void)context;
    return true;
}
static size_t max_payload(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}
static ls_result_t sink(void *context, const uint8_t *data, size_t length) {
    (void)context;
    return data && length ? LS_OK : LS_EINVAL;
}
static ls_result_t random_ok(void *context, uint8_t *output, size_t length) {
    uint8_t *value = (uint8_t *)context;
    if (!output && length)
        return LS_EINVAL;
    for (size_t i = 0; i < length; ++i)
        output[i] = (*value)++;
    return LS_OK;
}
static ls_result_t random_fail(void *context, uint8_t *output, size_t length) {
    (void)context;
    (void)output;
    (void)length;
    return LS_EIO;
}

static ls_result_t self_ok(void *ctx) {
    (void)ctx;
    return LS_OK;
}
static ls_result_t self_fail(void *ctx) {
    (void)ctx;
    return LS_EIO;
}

typedef struct {
    unsigned destroy_calls;
    bool fail_destroy;
} se_state_t;
static ls_result_t se_sign(void *ctx, const uint8_t digest[32], uint8_t signature[64]) {
    (void)ctx;
    if (!digest || !signature)
        return LS_EINVAL;
    for (size_t i = 0; i < 64; ++i)
        signature[i] = (uint8_t)(digest[i % 32] ^ 0x5au);
    return LS_OK;
}
static ls_result_t se_destroy(void *ctx, uint32_t key_id) {
    se_state_t *state = (se_state_t *)ctx;
    state->destroy_calls++;
    if (!key_id)
        return LS_EINVAL;
    return state->fail_destroy ? LS_EIO : LS_OK;
}

static int setup_with_identity(const ls_identity_t *identity) {
    static ls_memory_storage_t memory;
    static ls_storage_backend_t storage;
    static ls_transport_backend_t transport;
    ls_config_t config = {.identity = identity, .timestamp_ms = clock_ms};
    memset(storage_bytes, 0xff, sizeof(storage_bytes));
    now_ms = 1000u;
    memory = (ls_memory_storage_t){storage_bytes, sizeof(storage_bytes)};
    storage = (ls_storage_backend_t){
        .name = "ram",
        .context = &memory,
        .capacity = sizeof(storage_bytes),
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    transport = (ls_transport_backend_t){
        .name = "sink",
        .priority = 1u,
        .available = online,
        .send = sink,
        .max_payload = max_payload,
    };
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);
    return 0;
}

static int setup(void) {
    static const ls_identity_t identity = {
        .project_id = "auv-edges",
        .device_id = "auv-edge-node",
        .product = "commercial-auv",
        .hardware_revision = "revZ",
        .firmware_version = "0.3.0",
        .firmware_build_id = "auv-edge-build-0001",
        .bootloader_version = "boot-test",
        .git_commit = "abcdef01",
        .architecture = "host",
        .rtos = "test",
    };
    return setup_with_identity(&identity);
}

static int test_blackbox_edges(void) {
    ls_blackbox_clear();
    CHECK(ls_blackbox_get_stats(NULL) == LS_EINVAL);
    CHECK(ls_blackbox_copy(NULL, 1u, NULL) == LS_EINVAL);
    size_t count = 123u;
    CHECK(ls_blackbox_copy(NULL, 0u, &count) == LS_OK && count == 0u);
    ls_blackbox_record_t rec = {0};
    CHECK(ls_blackbox_get_recent(0u, NULL) == LS_EINVAL);
    CHECK(ls_blackbox_get_recent(0u, &rec) == LS_EAGAIN);
    CHECK(ls_blackbox_record(NULL) == LS_EINVAL);
    rec.kind = 0u;
    CHECK(ls_blackbox_record(&rec) == LS_EINVAL);
    rec.kind = (uint16_t)LS_BLACKBOX_USER + 1u;
    CHECK(ls_blackbox_record(&rec) == LS_EINVAL);

    ls_blackbox_set_profile(LS_BLACKBOX_PROFILE_QUIET);
    ls_blackbox_set_profile((ls_blackbox_profile_t)99);
    ls_blackbox_stats_t stats;
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK);
    CHECK(stats.profile == LS_BLACKBOX_PROFILE_QUIET);
    ls_blackbox_anomaly_begin(0u);
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK && stats.profile == LS_BLACKBOX_PROFILE_QUIET);

    /* Quiet drops ordinary records but retains watchdog/power evidence. */
    CHECK(ls_blackbox_record_values(LS_BLACKBOX_USER, 1u, 0u, 1, 2, 3, 4) == LS_OK);
    CHECK(ls_blackbox_record_values(LS_BLACKBOX_WATCHDOG, 2u, 0u, 1, 0, 0, 0) == LS_OK);
    CHECK(ls_blackbox_record_values(LS_BLACKBOX_POWER, 3u, 0u, 2, 0, 0, 0) == LS_OK);
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK && stats.count == 2u);

    ls_blackbox_set_profile(LS_BLACKBOX_PROFILE_NORMAL);
    for (size_t i = 0; i < LS_BLACKBOX_CAPACITY + 5u; ++i) {
        CHECK(ls_blackbox_record_values(LS_BLACKBOX_USER, (uint16_t)i, 0u, (int32_t)i, 0, 0, 0) ==
              LS_OK);
    }
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK);
    CHECK(stats.count == LS_BLACKBOX_CAPACITY);
    CHECK(stats.overwritten_records >= 5u);
    CHECK(ls_blackbox_get_recent(0u, &rec) == LS_OK);
    CHECK(rec.value[0] == (int32_t)(LS_BLACKBOX_CAPACITY + 4u));
    CHECK(ls_blackbox_get_recent(LS_BLACKBOX_CAPACITY, &rec) == LS_EAGAIN);

    ls_blackbox_record_t short_copy[3];
    count = 0u;
    CHECK(ls_blackbox_copy(short_copy, 3u, &count) == LS_OK && count == 3u);
    CHECK(short_copy[0].value[0] < short_copy[2].value[0]);

    ls_blackbox_freeze();
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK && stats.frozen);
    CHECK(ls_blackbox_record_values(LS_BLACKBOX_USER, 9u, LS_BLACKBOX_IMPORTANT, 0, 0, 0, 0) ==
          LS_EBUSY);
    ls_blackbox_thaw();
    CHECK(ls_blackbox_record_values(LS_BLACKBOX_USER, 9u, LS_BLACKBOX_IMPORTANT, 0, 0, 0, 0) ==
          LS_OK);

    ls_blackbox_anomaly_begin(10u);
    now_ms += 5u;
    ls_blackbox_poll();
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK && stats.profile == LS_BLACKBOX_PROFILE_ANOMALY);
    now_ms += 10u;
    ls_blackbox_poll();
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK && stats.profile == LS_BLACKBOX_PROFILE_NORMAL);
    return 0;
}

static int test_mission_time_edges(void) {
    CHECK(ls_mission_get(NULL) == LS_EINVAL);
    CHECK(ls_mission_begin(NULL, "d", "n") == LS_EINVAL);
    CHECK(ls_mission_begin("", "d", "n") == LS_EINVAL);
    CHECK(ls_mission_elapsed_ms() == 0u);
    ls_mission_end();
    CHECK(ls_mission_begin("M1", NULL, NULL) == LS_OK);
    now_ms += 42u;
    CHECK(ls_mission_elapsed_ms() == 42u);
    ls_mission_set_mode(NULL, 7u);
    ls_mission_set_depth_cm(-123);
    ls_mission_context_t context;
    CHECK(ls_mission_get(&context) == LS_OK);
    CHECK(context.phase == 7u && context.depth_cm == -123);
    CHECK(context.vehicle_mode[0] == '\0');

    uint8_t random_counter = 1u;
    ls_security_set_random_provider(random_ok, &random_counter);
    CHECK(ls_incident_begin(0u, 0u) == LS_OK);
    CHECK(ls_mission_get(&context) == LS_OK);
    CHECK(context.incident_active && (context.incident_hi || context.incident_lo));
    ls_incident_end();
    CHECK(ls_mission_get(&context) == LS_OK);
    CHECK(!context.incident_active);

    ls_security_set_random_provider(random_fail, NULL);
    CHECK(ls_incident_begin(0u, 0u) == LS_OK);
    CHECK(ls_mission_get(&context) == LS_OK);
    CHECK(context.incident_active && (context.incident_hi || context.incident_lo));
    ls_security_set_random_provider(NULL, NULL);
    ls_mission_end();

    uint64_t utc = 0u;
    ls_time_sync_clear();
    CHECK(ls_time_utc_ms(NULL) == LS_EINVAL);
    CHECK(ls_time_utc_ms(&utc) == LS_EAGAIN);
    CHECK(ls_time_sync_set(LS_TIME_SOURCE_NONE, 1u, 0u) == LS_EINVAL);
    CHECK(ls_time_sync_set((ls_time_source_t)99, 1u, 0u) == LS_EINVAL);
    CHECK(ls_time_sync_set(LS_TIME_SOURCE_RTC, 0u, 0u) == LS_EINVAL);
    CHECK(ls_time_sync_set(LS_TIME_SOURCE_PTP, UINT64_MAX - 1u, 1u) == LS_OK);
    now_ms += 3u;
    CHECK(ls_time_utc_ms(&utc) == LS_EOVERFLOW);
    ls_time_sync_clear();
    CHECK(!ls_time_sync_get().synchronized);
    CHECK(ls_time_sync_set(LS_TIME_SOURCE_HOST, 123456789ull, 9u) == LS_OK);
    CHECK(ls_time_utc_ms(&utc) == LS_OK && utc == 123456789ull);
    return 0;
}

static int test_environment_health_edges(void) {
    ls_environment_sample(NULL);
    CHECK(ls_environment_get_summary(NULL) == LS_EINVAL);
    ls_environment_summary_t env;
    CHECK(ls_environment_get_summary(&env) == LS_OK);
    unsigned before = env.sample_count;
    ls_environment_sample_t a = {.timestamp_ms = 777u,
                                 .pressure_pa = 200000u,
                                 .depth_cm = 1000,
                                 .internal_temperature_c = 30,
                                 .humidity_permyriad = 5000u,
                                 .vibration_mg_rms = 500u};
    ls_environment_sample(&a);
    ls_environment_sample_t b = {.pressure_pa = 100000u,
                                 .depth_cm = 100,
                                 .internal_temperature_c = 20,
                                 .humidity_permyriad = 1000u,
                                 .vibration_mg_rms = 100u};
    ls_environment_sample(&b);
    CHECK(ls_environment_get_summary(&env) == LS_OK);
    CHECK(env.sample_count == before + 2u);
    CHECK(env.maximum_pressure_pa >= 200000u && env.maximum_depth_cm >= 1000);
    CHECK(!ls_environment_leak_detected());
    b.flags = LS_ENV_WATER_INGRESS | LS_ENV_PRESSURE_SENSOR_FAULT | LS_ENV_VIBRATION_LIMIT;
    ls_environment_sample(&b);
    CHECK(ls_environment_leak_detected());

    CHECK(ls_health_get(NULL, NULL) == LS_EINVAL);
    ls_health_t health;
    CHECK(ls_health_get("missing", &health) == LS_EAGAIN);
    CHECK(ls_health_snapshot(NULL, 4u) == 0u);
    CHECK(ls_health_snapshot(&health, 0u) == 0u);
    ls_health_register(NULL, 10u);
    ls_health_register("zero", 0u);
    ls_health_register("nav", 10u);
    ls_health_register("nav", 20u); /* replacement deadline path */
    CHECK(ls_health_get("nav", &health) == LS_OK && health.deadline_ms == 20u);
    now_ms += 25u;
    CHECK(ls_health_poll() == LS_EAGAIN);
    CHECK(ls_health_poll() == LS_OK); /* already expired: don't duplicate the miss */
    CHECK(ls_health_get("nav", &health) == LS_OK && health.misses == 1u);
    ls_health_touch("missing");
    ls_health_touch("nav");
    CHECK(ls_health_get("nav", &health) == LS_OK && !health.expired);

    CHECK(ls_power_get_summary(NULL) == LS_EINVAL);
    CHECK(!ls_power_brownout_suspected(0u, 100u));
    ls_power_sample(NULL);
    ls_power_sample_t p1 = {.timestamp_ms = now_ms,
                            .vdd_mv = 3300u,
                            .battery_mv = 12000u,
                            .current_ma = -50,
                            .temperature_c = 20};
    ls_power_sample_t p2 = {.timestamp_ms = now_ms + 1u,
                            .vdd_mv = 2800u,
                            .battery_mv = 11000u,
                            .current_ma = 500,
                            .temperature_c = 40};
    ls_power_sample(&p1);
    now_ms += 1u;
    ls_power_sample(&p2);
    ls_power_summary_t power;
    CHECK(ls_power_get_summary(&power) == LS_OK);
    CHECK(power.minimum_vdd_mv <= 2800u && power.maximum_current_ma >= 500);
    CHECK(ls_power_brownout_suspected(3000u, 0u));
    now_ms += 100u;
    CHECK(!ls_power_brownout_suspected(3000u, 10u));
    return 0;
}

static int test_supervisor_edges(void) {
    /* Fresh runtime to avoid carrying environmental alarms from prior test. */
    CHECK(setup() == 0);
    CHECK(ls_supervisor_poll() == LS_ENOTSUP);
    CHECK(ls_supervisor_configure(NULL) == LS_EINVAL);
    ls_supervisor_config_t bad = {.maximum_spool_percent = 101u};
    CHECK(ls_supervisor_configure(&bad) == LS_EINVAL);

    ls_supervisor_config_t cfg = {
        .watchdog_max_stale_ms = 10u,
        .minimum_vdd_mv = 3000u,
        .minimum_battery_mv = 10000u,
        .maximum_temperature_c = 60,
        .minimum_heap_free_bytes = 100u,
        .maximum_spool_percent = 1u,
        .maximum_vibration_mg_rms = 1000u,
        .anomaly_hold_ms = 20u,
    };
    CHECK(ls_supervisor_configure(&cfg) == LS_OK);
    /* No samples/watchdog/spool yet, but zero heap trips the configured heap guard. */
    CHECK(ls_supervisor_poll() == LS_EAGAIN);
    CHECK(ls_supervisor_get_status().active_alarms & LS_SUPERVISOR_HEAP_LOW);
    CHECK(ls_flush() == LS_OK);

    ls_heap_stats_t heap = {.free_bytes = 1000u, .minimum_free_bytes = 900u, .largest_block = 700u};
    ls_heap_stats_update(&heap);
    now_ms += 1u;
    ls_watchdog_fed();
    ls_power_sample_t power = {.vdd_mv = 3300u, .battery_mv = 12000u, .temperature_c = 30};
    ls_power_sample(&power);
    CHECK(ls_supervisor_poll() == LS_OK);
    uint32_t transitions = ls_supervisor_get_status().transitions;
    CHECK(transitions >= 2u);
    CHECK(ls_supervisor_poll() == LS_OK);
    CHECK(ls_supervisor_get_status().transitions == transitions);

    /* Battery threshold is independently observable from VDD. */
    power.battery_mv = 9000u;
    ls_power_sample(&power);
    CHECK(ls_supervisor_poll() == LS_EAGAIN);
    CHECK(ls_supervisor_get_status().active_alarms & LS_SUPERVISOR_BATTERY_LOW);
    CHECK(ls_flush() == LS_OK);
    power.battery_mv = 12000u;
    ls_power_sample(&power);
    CHECK(ls_supervisor_poll() == LS_OK);

    now_ms += 20u;
    CHECK(ls_supervisor_poll() == LS_EAGAIN);
    CHECK(ls_supervisor_get_status().active_alarms & LS_SUPERVISOR_WATCHDOG_STALE);

    ls_health_register("control", 5u);
    now_ms += 10u;
    ls_environment_sample_t env = {.vibration_mg_rms = 1500u,
                                   .flags = LS_ENV_PRESSURE_SENSOR_FAULT | LS_ENV_WATER_INGRESS};
    ls_environment_sample(&env);
    CHECK(ls_supervisor_poll() == LS_EAGAIN);
    uint32_t alarms = ls_supervisor_get_status().active_alarms;
    CHECK(alarms & LS_SUPERVISOR_HEALTH_DEADLINE);
    CHECK(alarms & LS_SUPERVISOR_ENV_SENSOR_FAULT);
    CHECK(alarms & LS_SUPERVISOR_ENVIRONMENT_LEAK);
    CHECK(alarms & LS_SUPERVISOR_VIBRATION_HIGH);

    /* A captured event fills at least one percent of this tiny spool, exercising high watermark. */
    ls_capture_message("queued", LS_SEVERITY_WARNING);
    CHECK(ls_supervisor_poll() == LS_EAGAIN);
    CHECK(ls_supervisor_get_status().active_alarms & LS_SUPERVISOR_SPOOL_HIGH);
    CHECK(ls_flush() == LS_OK);
    return 0;
}

static int test_selftest_fault_injection_edges(void) {
    ls_selftest_clear();
    CHECK(ls_selftest_register(NULL) == LS_EINVAL);
    ls_selftest_case_t invalid = {0};
    CHECK(ls_selftest_register(&invalid) == LS_EINVAL);
    invalid.id = 1u;
    invalid.name = "bad";
    CHECK(ls_selftest_register(&invalid) == LS_EINVAL);
    CHECK(ls_selftest_run(NULL) == LS_EINVAL);

    ls_selftest_case_t first = {.id = 1u, .name = "first", .run = self_ok, .required = true};
    CHECK(ls_selftest_register(&first) == LS_OK);
    first.name = "replaced";
    first.run = self_fail;
    CHECK(ls_selftest_register(&first) == LS_OK);
    for (uint16_t i = 2u; i <= LS_SELFTEST_CAPACITY; ++i) {
        ls_selftest_case_t test = {.id = i, .name = "ok", .run = self_ok, .required = false};
        CHECK(ls_selftest_register(&test) == LS_OK);
    }
    ls_selftest_case_t overflow = {
        .id = (uint16_t)(LS_SELFTEST_CAPACITY + 1u), .name = "overflow", .run = self_ok};
    CHECK(ls_selftest_register(&overflow) == LS_ENOSPACE);
    ls_selftest_report_t report;
    CHECK(ls_selftest_run(&report) == LS_EIO);
    CHECK(report.registered == LS_SELFTEST_CAPACITY && report.failed == 1u);
    CHECK(report.failed_required_mask & 1u);
    ls_selftest_clear();
    CHECK(ls_selftest_run(&report) == LS_OK && report.registered == 0u);

    ls_fault_injection_clear();
    ls_fault_injection_state_t state;
    CHECK(ls_fault_injection_arm(NULL, 1u, LS_EIO) == LS_EINVAL);
    CHECK(ls_fault_injection_arm("", 1u, LS_EIO) == LS_EINVAL);
    CHECK(ls_fault_injection_arm("x", 0u, LS_EIO) == LS_EINVAL);
    CHECK(ls_fault_injection_arm("x", 1u, LS_OK) == LS_EINVAL);
    CHECK(ls_fault_injection_hit(NULL) == LS_EINVAL);
    CHECK(ls_fault_injection_hit("inactive") == LS_OK);
    CHECK(ls_fault_injection_get(NULL, &state) == LS_EINVAL);
    CHECK(ls_fault_injection_get("x", NULL) == LS_EINVAL);
    CHECK(ls_fault_injection_get("x", &state) == LS_EAGAIN);

    CHECK(ls_fault_injection_arm("x", 2u, LS_EIO) == LS_OK);
    CHECK(ls_fault_injection_hit("unknown") == LS_OK);
    CHECK(ls_fault_injection_hit("x") == LS_OK);
    CHECK(ls_fault_injection_hit("x") == LS_EIO);
    CHECK(ls_fault_injection_get("x", &state) == LS_OK && state.hits == 2u);
    CHECK(ls_fault_injection_arm("x", 1u, LS_EAGAIN) == LS_OK); /* re-arm existing */
    CHECK(ls_fault_injection_hit("x") == LS_EAGAIN);

    char names[LS_FAULT_INJECTION_CAPACITY][20];
    ls_fault_injection_clear();
    for (size_t i = 0u; i < LS_FAULT_INJECTION_CAPACITY; ++i) {
        snprintf(names[i], sizeof(names[i]), "point-%zu", i);
        CHECK(ls_fault_injection_arm(names[i], 1u, LS_EIO) == LS_OK);
    }
    CHECK(ls_fault_injection_arm("too-many", 1u, LS_EIO) == LS_ENOSPACE);
    ls_fault_injection_disarm(NULL);
    ls_fault_injection_disarm("not-found");
    ls_fault_injection_disarm(names[0]);
    CHECK(ls_fault_injection_hit(names[1]) == LS_EIO); /* active remains true */
    for (size_t i = 1u; i < LS_FAULT_INJECTION_CAPACITY; ++i)
        ls_fault_injection_disarm(names[i]);
    CHECK(ls_fault_injection_hit("after-clear") == LS_OK);
    return 0;
}

static int test_provisioning_edges(void) {
    CHECK(setup() == 0);
    CHECK(ls_provisioning_get_state().state == LS_PROVISIONING_UNPROVISIONED);
    CHECK(ls_provisioning_activate(0u, 1u) == LS_EINVAL);
    CHECK(ls_provisioning_activate(1u, 0u) == LS_EINVAL);
    CHECK(ls_provisioning_activate(1u, 1u) == LS_OK);
    CHECK(ls_provisioning_activate(2u, 2u) == LS_EINVAL);
    CHECK(ls_provisioning_begin_rotation(0u, 2u) == LS_EINVAL);
    CHECK(ls_provisioning_begin_rotation(1u, 2u) == LS_EINVAL);
    CHECK(ls_provisioning_begin_rotation(2u, 1u) == LS_EINVAL);
    CHECK(ls_provisioning_begin_rotation(2u, 2u) == LS_OK);
    CHECK(ls_provisioning_begin_rotation(3u, 3u) == LS_EINVAL);
    CHECK(ls_provisioning_commit_rotation() == LS_OK);
    CHECK(ls_provisioning_commit_rotation() == LS_EINVAL);
    CHECK(ls_provisioning_revoke() == LS_OK);
    CHECK(ls_provisioning_revoke() == LS_EINVAL);
    CHECK(ls_provisioning_activate(3u, 3u) == LS_OK);

    se_state_t se = {0};
    ls_secure_element_t no_destroy = {.context = &se, .sign_sha256 = se_sign};
    CHECK(ls_provisioning_decommission_secure(NULL) == LS_EINVAL);
    CHECK(ls_provisioning_decommission_secure(&no_destroy) == LS_ENOTSUP);
    /* The attempted secure flow is durably DECOMMISSIONING; retry with a real destroy hook. */
    CHECK(ls_provisioning_get_state().state == LS_PROVISIONING_DECOMMISSIONING);
    se.fail_destroy = true;
    ls_secure_element_t element = {
        .context = &se, .sign_sha256 = se_sign, .destroy_key = se_destroy};
    CHECK(ls_provisioning_decommission_secure(&element) == LS_EIO);
    CHECK(ls_provisioning_get_state().state == LS_PROVISIONING_DECOMMISSIONING);
    se.fail_destroy = false;
    CHECK(ls_provisioning_decommission_secure(&element) == LS_OK);
    CHECK(ls_provisioning_get_state().state == LS_PROVISIONING_DECOMMISSIONED);
    CHECK(ls_provisioning_decommission_secure(&element) == LS_OK);
    CHECK(ls_provisioning_decommission() == LS_OK);

    uint8_t digest[32], signature[64], challenge[129] = {0};
    CHECK(ls_provisioning_attest(NULL, challenge, 1u, digest, signature) == LS_EINVAL);
    CHECK(ls_provisioning_attest(&element, NULL, 1u, digest, signature) == LS_EINVAL);
    CHECK(ls_provisioning_attest(&element, challenge, 1u, NULL, signature) == LS_EINVAL);
    CHECK(ls_provisioning_attest(&element, challenge, 1u, digest, NULL) == LS_EINVAL);
    CHECK(ls_provisioning_attest(&element, challenge, 128u, digest, signature) == LS_OK);
    CHECK(ls_provisioning_attest(&element, challenge, sizeof(challenge), digest, signature) ==
          LS_EINVAL);
    CHECK(ls_provisioning_attest(&element, NULL, 0u, digest, signature) == LS_OK);
    return 0;
}

static int test_fingerprint_trace(void) {
    CHECK(ls_crash_fingerprint(NULL) == 0u);
    CHECK(ls_event_fingerprint(NULL, 1, 2u) != 0u);
    ls_arch_context_t context = {.architecture = LS_ARCH_CORTEX_M,
                                 .fault = LS_FAULT_HARD,
                                 .pc = 0x12345678u,
                                 .lr = 0xabcdef01u,
                                 .cfsr = 3u,
                                 .hfsr = 4u};
    CHECK(ls_crash_fingerprint(&context) != 0u);
    ls_trace_mutex_timeout(1u, 250u);
    ls_trace_dma(2u, false, LS_EIO);
    ls_trace_link(3u, true, 0u);
    return 0;
}

int main(void) {
    CHECK(setup() == 0);
    CHECK(test_blackbox_edges() == 0);
    CHECK(test_mission_time_edges() == 0);
    CHECK(test_environment_health_edges() == 0);
    CHECK(test_supervisor_edges() == 0);
    CHECK(test_selftest_fault_injection_edges() == 0);
    CHECK(test_provisioning_edges() == 0);
    CHECK(test_fingerprint_trace() == 0);
    puts("AUV edge tests passed");
    return 0;
}
