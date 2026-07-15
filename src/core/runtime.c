#include "internal.h"

ls_runtime_t ls_runtime;

ls_result_t ls_init(const ls_config_t *config) {
    if (!config || !config->identity)
        return LS_EINVAL;
    const char *build =
        config->identity->firmware_build_id ? config->identity->firmware_build_id : ls_build_id();
    if (!ls_build_id_validate(build))
        return LS_EINVAL;
    ls_memset(&ls_runtime, 0, sizeof ls_runtime);
    ls_runtime.config = *config;
    ls_runtime.breadcrumb_min_level = LS_SEVERITY_DEBUG;
    ls_runtime.breadcrumb_policy = LS_BREADCRUMB_DROP_OLDEST;
    ls_runtime.assert_policy = LS_ASSERT_CONTINUE;
    ls_runtime.log_min_level = LS_SEVERITY_DEBUG;
    ls_runtime.random_state =
        ls_hash_string(build) ^ ls_hash_string(config->identity->device_id) ^ 0x9e3779b9u;
    ls_runtime.security_policy =
        (ls_security_policy_t){LS_SECURITY_XCHACHA20_POLY1305, 1u, true, false};
    ls_runtime.initialized = true;
    return LS_OK;
}

ls_result_t ls_boot(void) {
    if (!ls_runtime.initialized)
        return LS_EINVAL;
    ls_minimal_snapshot_t retained_snapshot;
    bool retained_fault = ls_minimal_snapshot_read(&retained_snapshot);
    ls_result_t result = ls_spool_init();
    if (result != LS_OK)
        return result;
    result = ls_boot_state_load();
    if (result != LS_OK && result != LS_ECORRUPT)
        return result;
    ls_runtime.boot_started_ms =
        ls_runtime.config.timestamp_ms
            ? ls_runtime.config.timestamp_ms(ls_runtime.config.timestamp_context)
            : 0;
    ls_runtime.reset_info = ls_runtime.config.reset_info
                                ? ls_runtime.config.reset_info(ls_runtime.config.reset_info_context)
                                : (ls_reset_info_t){LS_RESET_UNKNOWN, 0, 0, 0, false, false, false};
    ls_runtime.previous_crashed = ls_runtime.persistent.crash_pending != 0 || retained_fault;
    bool expected = ls_runtime.persistent.expected_reset != 0;
    bool reset_failure = ls_runtime.reset_info.reason == LS_RESET_WATCHDOG ||
                         ls_runtime.reset_info.reason == LS_RESET_INDEPENDENT_WATCHDOG ||
                         ls_runtime.reset_info.reason == LS_RESET_WINDOW_WATCHDOG ||
                         ls_runtime.reset_info.reason == LS_RESET_LOCKUP;
    if (ls_runtime.previous_crashed || (reset_failure && !expected)) {
        if (ls_runtime.persistent.previous_uptime_ms <= LS_BOOT_LOOP_WINDOW_MS)
            ls_runtime.persistent.consecutive_failures++;
        else
            ls_runtime.persistent.consecutive_failures = 1;
    } else if (ls_runtime.persistent.boot_successful) {
        ls_runtime.persistent.consecutive_failures = 0;
    }
    ls_runtime.boot_loop = ls_runtime.persistent.consecutive_failures >= LS_BOOT_LOOP_THRESHOLD;
    ls_runtime.safe_mode = ls_runtime.boot_loop;
    ls_runtime.persistent.boot_count++;
    ls_runtime.persistent.boot_successful = 0;
    ls_runtime.persistent.crash_pending = 0;
    ls_runtime.persistent.expected_reset = 0;
    ls_runtime.boot_count = ls_runtime.persistent.boot_count;
    ls_runtime.sequence = ls_runtime.persistent.last_sequence > ls_runtime.sequence
                              ? ls_runtime.persistent.last_sequence
                              : ls_runtime.sequence;
    ls_runtime.reset_info.boot_count = ls_runtime.boot_count;
    ls_runtime.reset_info.previous_uptime_ms = ls_runtime.persistent.previous_uptime_ms;
    ls_runtime.reset_info.expected = expected;
    ls_runtime.reset_info.crash_pending = ls_runtime.previous_crashed;
    ls_runtime.reset_info.boot_loop = ls_runtime.boot_loop;
    result = ls_boot_state_save();
    if (result != LS_OK)
        return result;
    if (retained_fault)
        (void)ls_capture_minimal_recover();
    return LS_OK;
}

uint32_t ls_uptime_ms(void) {
    uint32_t now = ls_runtime.config.timestamp_ms
                       ? ls_runtime.config.timestamp_ms(ls_runtime.config.timestamp_context)
                       : 0;
    return now - ls_runtime.boot_started_ms;
}
ls_reset_info_t ls_get_reset_info(void) {
    return ls_runtime.reset_info;
}
bool ls_previous_boot_crashed(void) {
    return ls_runtime.previous_crashed;
}
bool ls_boot_loop_detected(void) {
    return ls_runtime.boot_loop;
}
uint32_t ls_boot_count(void) {
    return ls_runtime.boot_count;
}

void ls_capture_error(const ls_error_t *error) {
    if (!error)
        return;
    ls_event_t event = {.type = LS_EVENT_ERROR,
                        .priority = error->severity >= LS_SEVERITY_ERROR ? LS_PRIORITY_ERROR
                                                                         : LS_PRIORITY_WARNING,
                        .timestamp_ms = ls_uptime_ms(),
                        .domain = error->domain,
                        .code = error->code,
                        .severity = error->severity,
                        .message = error->message,
                        .capture_level = LS_CAPTURE_SNAPSHOT};
    (void)ls_capture_event(&event);
}
void ls_capture_message(const char *message, ls_severity_t severity) {
    ls_error_t error = {"message", 0, severity, message};
    ls_capture_error(&error);
}

ls_result_t ls_capture_event(const ls_event_t *event) {
    uint8_t encoded[LS_MAX_EVENT_SIZE];
    size_t length = 0;
    if (!event)
        return LS_EINVAL;
    ls_enter_critical();
    if (ls_runtime.capturing) {
        ls_leave_critical();
        return LS_EAGAIN;
    }
    ls_runtime.capturing = true;
    ls_leave_critical();
    ls_event_t filtered = *event;
    ls_result_t result = LS_OK;
    if (ls_policy_apply(&filtered)) {
        result = ls_envelope_encode(&filtered, encoded, sizeof encoded, &length);
        if (result == LS_OK)
            result = ls_spool_append(encoded, length, filtered.priority);
    }
    ls_enter_critical();
    ls_runtime.capturing = false;
    ls_leave_critical();
    return result;
}
