#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "AUV runtime check failed: %s:%d\n", #condition, __LINE__);            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint32_t now_ms;
static uint8_t storage_bytes[60000];
static uint8_t last_envelope[LS_MAX_EVENT_SIZE];
static size_t last_envelope_length;
static unsigned destroyed_keys;

static uint32_t clock_ms(void *context) {
    (void)context;
    return now_ms;
}
static bool online(void *context) {
    (void)context;
    return true;
}
static size_t mtu(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}
static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    (void)context;
    if (length > sizeof(last_envelope))
        return LS_ENOSPACE;
    memcpy(last_envelope, data, length);
    last_envelope_length = length;
    return LS_OK;
}
static ls_result_t random_bytes(void *context, uint8_t *output, size_t length) {
    uint8_t *counter = (uint8_t *)context;
    for (size_t index = 0u; index < length; ++index)
        output[index] = (*counter)++;
    return LS_OK;
}
static ls_result_t selftest_ok(void *context) {
    (void)context;
    return LS_OK;
}
static ls_result_t selftest_fail(void *context) {
    (void)context;
    return LS_EIO;
}
static ls_result_t se_random(void *context, uint8_t *output, size_t length) {
    return random_bytes(context, output, length);
}
static ls_result_t se_sign(void *context, const uint8_t digest[32], uint8_t signature[64]) {
    (void)context;
    for (size_t index = 0u; index < 64u; ++index)
        signature[index] = digest[index % 32u] ^ 0xa5u;
    return LS_OK;
}
static ls_result_t se_destroy(void *context, uint32_t key_id) {
    (void)context;
    CHECK(key_id == 8u);
    destroyed_keys++;
    return LS_OK;
}
static ls_result_t se_cert(void *context, uint8_t *output, size_t capacity, size_t *length) {
    (void)context;
    if (!output || !length || capacity < 1u)
        return LS_ENOSPACE;
    output[0] = 1u;
    *length = 1u;
    return LS_OK;
}

typedef struct {
    bool blackbox, mission, time_sync, provisioning, supervisor, environment;
} tlvs_t;
static ls_result_t visit(void *context, uint16_t type, const uint8_t *value, uint16_t length) {
    tlvs_t *tlvs = (tlvs_t *)context;
    CHECK(value != NULL);
    CHECK(length != 0u);
    if (type == LS_TLV_BLACKBOX)
        tlvs->blackbox = true;
    if (type == LS_TLV_MISSION)
        tlvs->mission = true;
    if (type == LS_TLV_TIME_SYNC)
        tlvs->time_sync = true;
    if (type == LS_TLV_PROVISIONING)
        tlvs->provisioning = true;
    if (type == LS_TLV_SUPERVISOR)
        tlvs->supervisor = true;
    if (type == LS_TLV_ENVIRONMENT)
        tlvs->environment = true;
    return LS_OK;
}

static int setup(void) {
    static const ls_identity_t identity = {
        .project_id = "auv-runtime",
        .device_id = "auv-07-nav",
        .product = "commercial-auv",
        .hardware_revision = "revA",
        .firmware_version = "0.3.0",
        .firmware_build_id = "auv-runtime-0001",
        .bootloader_version = "mcuboot-test",
        .git_commit = "deadbeef",
        .architecture = "host",
        .rtos = "test",
    };
    static ls_memory_storage_t memory;
    static ls_storage_backend_t storage;
    static ls_transport_backend_t transport;
    ls_config_t config = {.identity = &identity, .timestamp_ms = clock_ms};

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
        .send = send_data,
        .max_payload = mtu,
    };
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);
    return 0;
}

static int test_blackbox_mission_time(void) {
    ls_blackbox_clear();
    CHECK(ls_blackbox_record_values(LS_BLACKBOX_STATE, 4u, 0u, 1, 2, 3, 4) == LS_OK);
    ls_trace_task_switch(1u, 2u);
    ls_trace_irq_enter(19u);
    ls_trace_irq_exit(19u);
    ls_trace_dma(2u, true, 0u);
    ls_trace_state(3u, 7, 8);
    ls_trace_link(5u, false, 99u);

    ls_blackbox_stats_t stats;
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK);
    CHECK(stats.count >= 6u);
    ls_blackbox_set_profile(LS_BLACKBOX_PROFILE_QUIET);
    size_t before = stats.count;
    CHECK(ls_blackbox_record_values(LS_BLACKBOX_USER, 1u, 0u, 0, 0, 0, 0) == LS_OK);
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK);
    CHECK(stats.count == before);
    CHECK(ls_blackbox_record_values(LS_BLACKBOX_USER, 1u, LS_BLACKBOX_IMPORTANT, 0, 0, 0, 0) ==
          LS_OK);
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK);
    CHECK(stats.count == before + 1u);
    ls_blackbox_anomaly_begin(50u);
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK);
    CHECK(stats.profile == LS_BLACKBOX_PROFILE_ANOMALY);
    now_ms += 60u;
    ls_blackbox_poll();
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK);
    CHECK(stats.profile == LS_BLACKBOX_PROFILE_QUIET);
    ls_blackbox_set_profile(LS_BLACKBOX_PROFILE_NORMAL);

    CHECK(ls_mission_begin("mission-atlantic-04", "dive-17", "nav-mcu") == LS_OK);
    ls_mission_set_mode("survey", 2u);
    ls_mission_set_depth_cm(12345);
    CHECK(ls_incident_begin(0x1122334455667788ull, 0x99aabbccddeeff00ull) == LS_OK);
    ls_mission_context_t mission;
    CHECK(ls_mission_get(&mission) == LS_OK);
    CHECK(strcmp(mission.mission_id, "mission-atlantic-04") == 0);
    CHECK(mission.depth_cm == 12345);
    CHECK(mission.incident_active);
    CHECK(ls_mission_elapsed_ms() == 0u);

    CHECK(ls_time_sync_set(LS_TIME_SOURCE_GNSS, 1800000000000ull, 25u) == LS_OK);
    now_ms += 123u;
    uint64_t utc = 0u;
    CHECK(ls_time_utc_ms(&utc) == LS_OK);
    CHECK(utc == 1800000000123ull);
    CHECK(ls_time_sync_get().source == LS_TIME_SOURCE_GNSS);
    return 0;
}

static int test_health_power_supervisor(void) {
    ls_mission_context_t mission;
    ls_health_register("control", 20u);
    now_ms += 5u;
    ls_health_touch("control");
    ls_health_t health;
    CHECK(ls_health_get("control", &health) == LS_OK);
    CHECK(health.deadline_ms == 20u);
    now_ms += 25u;
    CHECK(ls_health_poll() == LS_EAGAIN);
    CHECK(ls_health_get("control", &health) == LS_OK);
    CHECK(health.expired && health.misses == 1u && health.max_lateness_ms >= 5u);
    ls_health_t snapshot[4];
    CHECK(ls_health_snapshot(snapshot, 4u) >= 1u);
    ls_health_touch("control");

    ls_power_sample_t power = {
        .vdd_mv = 2900u, .battery_mv = 10500u, .current_ma = 420, .temperature_c = 72};
    ls_power_sample(&power);
    ls_power_summary_t summary;
    CHECK(ls_power_get_summary(&summary) == LS_OK);
    CHECK(summary.minimum_vdd_mv == 2900u);
    CHECK(summary.maximum_temperature_c == 72);
    CHECK(ls_power_brownout_suspected(3000u, 100u));

    ls_environment_sample_t environment = {
        .pressure_pa = 230000u,
        .depth_cm = 1250,
        .internal_temperature_c = 38,
        .humidity_permyriad = 6400u,
        .vibration_mg_rms = 1750u,
        .flags = LS_ENV_LEAK_DETECTED,
    };
    ls_environment_sample(&environment);
    ls_environment_summary_t environment_summary;
    CHECK(ls_environment_get_summary(&environment_summary) == LS_OK);
    CHECK(environment_summary.sample_count == 1u);
    CHECK(environment_summary.maximum_depth_cm == 1250);
    CHECK(environment_summary.maximum_pressure_pa == 230000u);
    CHECK(environment_summary.leak_events == 1u);
    CHECK(ls_environment_leak_detected());
    CHECK(ls_mission_get(&mission) == LS_OK);
    CHECK(mission.depth_cm == 1250);

    ls_watchdog_checkpoint(77u);
    ls_watchdog_fed();
    ls_supervisor_config_t config = {
        .watchdog_max_stale_ms = 50u,
        .minimum_vdd_mv = 3000u,
        .minimum_battery_mv = 10000u,
        .maximum_temperature_c = 70,
        .minimum_heap_free_bytes = 0u,
        .maximum_spool_percent = 100u,
        .maximum_vibration_mg_rms = 1000u,
        .anomaly_hold_ms = 100u,
    };
    CHECK(ls_supervisor_configure(&config) == LS_OK);
    CHECK(ls_supervisor_poll() == LS_EAGAIN);
    ls_supervisor_status_t status = ls_supervisor_get_status();
    CHECK(status.active_alarms & LS_SUPERVISOR_VDD_LOW);
    CHECK(status.active_alarms & LS_SUPERVISOR_TEMPERATURE_HIGH);
    CHECK(status.active_alarms & LS_SUPERVISOR_ENVIRONMENT_LEAK);
    CHECK(status.active_alarms & LS_SUPERVISOR_VIBRATION_HIGH);
    return 0;
}

static int test_provisioning_attestation_selftest_faults(void) {
    uint8_t random_counter = 1u;
    ls_security_set_random_provider(random_bytes, &random_counter);
    CHECK(ls_provisioning_activate(7u, 1u) == LS_OK);
    CHECK(ls_provisioning_get_state().state == LS_PROVISIONING_ACTIVE);
    CHECK(ls_provisioning_begin_rotation(8u, 2u) == LS_OK);
    CHECK(ls_provisioning_commit_rotation() == LS_OK);
    CHECK(ls_provisioning_get_state().key_id == 8u);

    ls_secure_element_t element = {.context = &random_counter,
                                   .random = se_random,
                                   .sign_sha256 = se_sign,
                                   .read_certificate = se_cert,
                                   .destroy_key = se_destroy};
    uint8_t digest[32], signature[64];
    static const uint8_t challenge[] = {1u, 2u, 3u, 4u};
    CHECK(ls_provisioning_attest(&element, challenge, sizeof(challenge), digest, signature) ==
          LS_OK);
    CHECK(signature[0] == (uint8_t)(digest[0] ^ 0xa5u));

    ls_selftest_clear();
    ls_selftest_case_t pass = {1u, "flash", selftest_ok, NULL, true};
    ls_selftest_case_t fail = {2u, "pressure-sensor", selftest_fail, NULL, false};
    CHECK(ls_selftest_register(&pass) == LS_OK);
    CHECK(ls_selftest_register(&fail) == LS_OK);
    ls_selftest_report_t report;
    CHECK(ls_selftest_run(&report) == LS_EIO);
    CHECK(report.registered == 2u && report.passed == 1u && report.failed == 1u);
    CHECK(report.failed_required_mask == 0u);

    ls_fault_injection_clear();
    CHECK(ls_fault_injection_arm("spool.before_commit", 2u, LS_EIO) == LS_OK);
    CHECK(ls_fault_injection_hit("spool.before_commit") == LS_OK);
    CHECK(ls_fault_injection_hit("spool.before_commit") == LS_EIO);
    ls_fault_injection_state_t state;
    CHECK(ls_fault_injection_get("spool.before_commit", &state) == LS_OK);
    CHECK(state.hits == 2u);
    ls_fault_injection_disarm("spool.before_commit");
    CHECK(ls_fault_injection_get("spool.before_commit", &state) == LS_EAGAIN);

    ls_arch_context_t crash = {.architecture = LS_ARCH_CORTEX_M,
                               .fault = LS_FAULT_HARD,
                               .pc = 0x08001234u,
                               .lr = 0x08005678u,
                               .cfsr = 0x82u,
                               .fault_address = 0x20001234u};
    uint32_t fingerprint = ls_crash_fingerprint(&crash);
    CHECK(fingerprint != 0u);
    CHECK(fingerprint == ls_crash_fingerprint(&crash));
    CHECK(ls_event_fingerprint("can", 4, 55u) != 0u);
    return 0;
}

static int test_envelope_extensions_and_retention(void) {
    ls_capture_message("mission-health-snapshot", LS_SEVERITY_ERROR);
    CHECK(ls_flush() == LS_OK);
    CHECK(last_envelope_length > LS_LEP_HEADER_SIZE);
    tlvs_t tlvs = {0};
    CHECK(ls_envelope_visit(last_envelope, last_envelope_length, visit, &tlvs) == LS_OK);
    CHECK(tlvs.blackbox && tlvs.mission && tlvs.time_sync && tlvs.provisioning && tlvs.supervisor &&
          tlvs.environment);

    ls_blackbox_stats_t before;
    CHECK(ls_blackbox_get_stats(&before) == LS_OK);
    CHECK(before.count > 0u);
    /* ls_init clears normal runtime state but must preserve the retained recorder. */
    static const ls_identity_t identity = {.project_id = "retention",
                                           .device_id = "auv-07-nav",
                                           .firmware_build_id = "auv-runtime-0001"};
    ls_config_t config = {.identity = &identity, .timestamp_ms = clock_ms};
    CHECK(ls_init(&config) == LS_OK);
    ls_blackbox_stats_t after;
    CHECK(ls_blackbox_get_stats(&after) == LS_OK);
    CHECK(after.count == before.count);
    ls_blackbox_freeze();
    CHECK(ls_blackbox_record_values(LS_BLACKBOX_USER, 2u, 0u, 1, 2, 3, 4) == LS_EBUSY);
    ls_blackbox_thaw();
    CHECK(ls_blackbox_record_values(LS_BLACKBOX_USER, 2u, 0u, 1, 2, 3, 4) == LS_OK);

    CHECK(setup() == 0);
    uint8_t context = 0u;
    ls_secure_element_t element = {.context = &context,
                                   .random = se_random,
                                   .sign_sha256 = se_sign,
                                   .read_certificate = se_cert,
                                   .destroy_key = se_destroy};
    destroyed_keys = 0u;
    CHECK(ls_provisioning_decommission_secure(&element) == LS_OK);
    CHECK(destroyed_keys == 1u);
    CHECK(ls_provisioning_get_state().state == LS_PROVISIONING_DECOMMISSIONED);
    return 0;
}

int main(void) {
    memset(storage_bytes, 0xff, sizeof(storage_bytes));
    now_ms = 100u;
    CHECK(setup() == 0);
    CHECK(test_blackbox_mission_time() == 0);
    CHECK(test_health_power_supervisor() == 0);
    CHECK(test_provisioning_attestation_selftest_faults() == 0);
    CHECK(test_envelope_extensions_and_retention() == 0);
    puts("AUV runtime tests passed");
    return 0;
}
