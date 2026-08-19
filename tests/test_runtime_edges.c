// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// tests/test_runtime_edges.c
//
// Runtime edge-case tests. Double init, null config, version
// mismatch, and subsystem coupling.
//
// Heap-free, bounded, deterministic.

#include <stdio.h>
#include "laststate/latch.h"
#include "../src/core/internal.h"

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "runtime edges failed: %s:%d\n", #x, __LINE__);                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static ls_reset_reason_t next_reason;
static uint32_t preset_uptime, preset_failures;
static bool preset_expected, preset_successful, preset_crash;
static ls_reset_info_t reset_info(void *context) {
    (void)context;
    /* The callback occurs after persistent-state load and lets this unit test model prior-boot
     * state without storage. */
    ls_runtime.persistent.previous_uptime_ms = preset_uptime;
    ls_runtime.persistent.consecutive_failures = preset_failures;
    ls_runtime.persistent.expected_reset = preset_expected ? 1u : 0u;
    ls_runtime.persistent.boot_successful = preset_successful ? 1u : 0u;
    ls_runtime.persistent.crash_pending = preset_crash ? 1u : 0u;
    return (ls_reset_info_t){.reason = next_reason, .raw_reason = (uint32_t)next_reason};
}
static void presets_clear(void) {
    preset_uptime = 0u;
    preset_failures = 0u;
    preset_expected = false;
    preset_successful = false;
    preset_crash = false;
}
static int init_runtime(void) {
    static const ls_identity_t id = {
        .project_id = "runtime", .device_id = "host", .firmware_build_id = "runtime1"};
    ls_config_t cfg = {.identity = &id, .reset_info = reset_info};
    return ls_init(&cfg) == LS_OK ? 0 : 1;
}

int main(void) {
    CHECK(ls_init(NULL) == LS_EINVAL);
    ls_config_t missing = {0};
    CHECK(ls_init(&missing) == LS_EINVAL);
    static const ls_identity_t generated_id = {
        .project_id = "runtime", .device_id = "host", .firmware_build_id = NULL};
    ls_config_t fallback = {.identity = &generated_id};
    CHECK(ls_init(&fallback) == LS_OK);

    ls_runtime.initialized = false;
    CHECK(ls_boot() == LS_EINVAL);
    presets_clear();
    CHECK(init_runtime() == 0);
    ls_capture_error(NULL);
    CHECK(ls_capture_event(NULL) == LS_EINVAL);
    ls_event_t e = {.type = LS_EVENT_MESSAGE,
                    .priority = LS_PRIORITY_WARNING,
                    .domain = "runtime",
                    .severity = LS_SEVERITY_WARNING,
                    .message = "busy",
                    .capture_level = LS_CAPTURE_METADATA};
    ls_runtime.capturing = true;
    CHECK(ls_capture_event(&e) == LS_EAGAIN);
    ls_runtime.capturing = false;

    const ls_reset_reason_t failure_reasons[] = {LS_RESET_WATCHDOG, LS_RESET_INDEPENDENT_WATCHDOG,
                                                 LS_RESET_WINDOW_WATCHDOG, LS_RESET_LOCKUP};
    for (size_t i = 0; i < sizeof failure_reasons / sizeof failure_reasons[0]; ++i) {
        presets_clear();
        next_reason = failure_reasons[i];
        preset_uptime = i == 0u ? LS_BOOT_LOOP_WINDOW_MS + 1u : 1u;
        CHECK(init_runtime() == 0);
        CHECK(ls_boot() == LS_OK);
        CHECK(ls_runtime.persistent.consecutive_failures == 1u);
    }

    presets_clear();
    next_reason = LS_RESET_WATCHDOG;
    preset_expected = true;
    preset_failures = 2u;
    preset_successful = true;
    CHECK(init_runtime() == 0);
    CHECK(ls_boot() == LS_OK);
    CHECK(ls_runtime.persistent.consecutive_failures == 0u);

    presets_clear();
    next_reason = LS_RESET_POWER_ON;
    preset_successful = true;
    preset_failures = 3u;
    CHECK(init_runtime() == 0);
    CHECK(ls_boot() == LS_OK);
    CHECK(ls_runtime.persistent.consecutive_failures == 0u);

    presets_clear();
    next_reason = LS_RESET_POWER_ON;
    preset_crash = true;
    preset_uptime = 1u;
    CHECK(init_runtime() == 0);
    CHECK(ls_boot() == LS_OK);
    CHECK(ls_previous_boot_crashed());
    return 0;
}
