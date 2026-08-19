// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// tests/test_profile.c
//
// Profile tests. Memory footprint, cycle counts, stack usage.
//
// Heap-free, bounded, deterministic.

#include "laststate/latch.h"
int main(void) {
    ls_identity_t identity = {
        .project_id = "profile", .device_id = "one", .firmware_build_id = "profile01"};
    ls_config_t config = {.identity = &identity};
    if (ls_init(&config) != LS_OK || ls_boot() != LS_OK)
        return 1;
    ls_breadcrumb("profile");
    ls_metric_i32("value", 1);
    ls_capture_message("profile", LS_SEVERITY_WARNING);
    return 0;
}
