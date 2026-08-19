// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// src/core/boot.c
//
// Boot, reset-reason, and safe-mode implementation. Detects the
// reset cause, decides whether the previous reset was expected,
// and arms safe-mode when the boot loop threshold trips.
//
// Heap-free, bounded, deterministic.

#include "internal.h"
#define LS_BOOT_MAGIC 0x544F4F42u
#define LS_BOOT_VERSION 3u
#define LS_BOOT_VERSION_V2 2u
#define LS_BOOT_VERSION_LEGACY 1u

typedef struct {
    uint32_t magic, version, boot_count, previous_uptime_ms;
    uint32_t consecutive_failures, first_failure_ms, last_sequence;
    uint32_t pending_crash_id, release_hash;
    uint8_t crash_pending, expected_reset, boot_successful, release_state;
    uint32_t crc;
} ls_persistent_boot_v1_t;

_Static_assert(sizeof(ls_persistent_boot_v1_t) == 44u, "legacy boot layout changed");

typedef struct {
    uint32_t magic, version, boot_count, previous_uptime_ms;
    uint32_t consecutive_failures, first_failure_ms, last_sequence;
    uint32_t pending_crash_id, release_hash;
    uint8_t crash_pending, expected_reset, boot_successful, release_state;
    uint32_t update_version_floor, update_pending_version, update_image_fingerprint;
    uint32_t update_signing_key_id, update_rollback_count;
    uint8_t update_state;
    uint8_t update_reserved[3];
    uint32_t crc;
} ls_persistent_boot_v2_t;

_Static_assert(sizeof(ls_persistent_boot_v2_t) == 68u, "v2 boot layout changed");

static size_t boot_offset(void) {
    return ls_spool_storage_size();
}
static uint32_t boot_crc(const ls_persistent_boot_t *state) {
    return ls_crc32(state, offsetof(ls_persistent_boot_t, crc));
}
static uint32_t boot_v1_crc(const ls_persistent_boot_v1_t *state) {
    return ls_crc32(state, offsetof(ls_persistent_boot_v1_t, crc));
}
static uint32_t boot_v2_crc(const ls_persistent_boot_v2_t *state) {
    return ls_crc32(state, offsetof(ls_persistent_boot_v2_t, crc));
}
static void boot_from_v1(const ls_persistent_boot_v1_t *legacy) {
    ls_runtime.persistent.magic = LS_BOOT_MAGIC;
    ls_runtime.persistent.version = LS_BOOT_VERSION;
    ls_runtime.persistent.boot_count = legacy->boot_count;
    ls_runtime.persistent.previous_uptime_ms = legacy->previous_uptime_ms;
    ls_runtime.persistent.consecutive_failures = legacy->consecutive_failures;
    ls_runtime.persistent.first_failure_ms = legacy->first_failure_ms;
    ls_runtime.persistent.last_sequence = legacy->last_sequence;
    ls_runtime.persistent.pending_crash_id = legacy->pending_crash_id;
    ls_runtime.persistent.release_hash = legacy->release_hash;
    ls_runtime.persistent.crash_pending = legacy->crash_pending;
    ls_runtime.persistent.expected_reset = legacy->expected_reset;
    ls_runtime.persistent.boot_successful = legacy->boot_successful;
    ls_runtime.persistent.release_state = legacy->release_state;
}
static void boot_from_v2(const ls_persistent_boot_v2_t *legacy) {
    ls_memset(&ls_runtime.persistent, 0, sizeof(ls_runtime.persistent));
    ls_runtime.persistent.magic = LS_BOOT_MAGIC;
    ls_runtime.persistent.version = LS_BOOT_VERSION;
    ls_runtime.persistent.boot_count = legacy->boot_count;
    ls_runtime.persistent.previous_uptime_ms = legacy->previous_uptime_ms;
    ls_runtime.persistent.consecutive_failures = legacy->consecutive_failures;
    ls_runtime.persistent.first_failure_ms = legacy->first_failure_ms;
    ls_runtime.persistent.last_sequence = legacy->last_sequence;
    ls_runtime.persistent.pending_crash_id = legacy->pending_crash_id;
    ls_runtime.persistent.release_hash = legacy->release_hash;
    ls_runtime.persistent.crash_pending = legacy->crash_pending;
    ls_runtime.persistent.expected_reset = legacy->expected_reset;
    ls_runtime.persistent.boot_successful = legacy->boot_successful;
    ls_runtime.persistent.release_state = legacy->release_state;
    ls_runtime.persistent.update_version_floor = legacy->update_version_floor;
    ls_runtime.persistent.update_pending_version = legacy->update_pending_version;
    ls_runtime.persistent.update_image_fingerprint = legacy->update_image_fingerprint;
    ls_runtime.persistent.update_signing_key_id = legacy->update_signing_key_id;
    ls_runtime.persistent.update_rollback_count = legacy->update_rollback_count;
    ls_runtime.persistent.update_state = legacy->update_state;
}
ls_result_t ls_boot_state_load(void) {
    ls_memset(&ls_runtime.persistent, 0, sizeof ls_runtime.persistent);
    if (!ls_runtime.storage || !ls_runtime.storage->read)
        return LS_OK;
    if (ls_runtime.storage->capacity < boot_offset() + sizeof(ls_persistent_boot_v1_t))
        return LS_ENOSPACE;

    uint32_t prefix[2] = {0u, 0u};
    ls_result_t result =
        ls_runtime.storage->read(ls_runtime.storage->context, boot_offset(), prefix, sizeof prefix);
    if (result != LS_OK)
        return result;
    if (prefix[0] != LS_BOOT_MAGIC) {
        ls_runtime.persistent.magic = LS_BOOT_MAGIC;
        ls_runtime.persistent.version = LS_BOOT_VERSION;
        return LS_ECORRUPT;
    }
    if (prefix[1] == LS_BOOT_VERSION_LEGACY) {
        ls_persistent_boot_v1_t legacy;
        result = ls_runtime.storage->read(ls_runtime.storage->context, boot_offset(), &legacy,
                                          sizeof legacy);
        if (result != LS_OK)
            return result;
        if (legacy.crc != boot_v1_crc(&legacy)) {
            ls_runtime.persistent.magic = LS_BOOT_MAGIC;
            ls_runtime.persistent.version = LS_BOOT_VERSION;
            return LS_ECORRUPT;
        }
        boot_from_v1(&legacy);
        return LS_OK;
    }
    if (prefix[1] == LS_BOOT_VERSION_V2) {
        if (ls_runtime.storage->capacity < boot_offset() + sizeof(ls_persistent_boot_v2_t))
            return LS_ENOSPACE;
        ls_persistent_boot_v2_t legacy;
        result = ls_runtime.storage->read(ls_runtime.storage->context, boot_offset(), &legacy,
                                          sizeof legacy);
        if (result != LS_OK)
            return result;
        if (legacy.crc != boot_v2_crc(&legacy)) {
            ls_runtime.persistent.magic = LS_BOOT_MAGIC;
            ls_runtime.persistent.version = LS_BOOT_VERSION;
            return LS_ECORRUPT;
        }
        boot_from_v2(&legacy);
        return LS_OK;
    }
    if (prefix[1] != LS_BOOT_VERSION ||
        ls_runtime.storage->capacity < boot_offset() + sizeof(ls_persistent_boot_t)) {
        ls_runtime.persistent.magic = LS_BOOT_MAGIC;
        ls_runtime.persistent.version = LS_BOOT_VERSION;
        return prefix[1] == LS_BOOT_VERSION ? LS_ENOSPACE : LS_ECORRUPT;
    }
    ls_persistent_boot_t state;
    result =
        ls_runtime.storage->read(ls_runtime.storage->context, boot_offset(), &state, sizeof state);
    if (result != LS_OK)
        return result;
    if (state.crc != boot_crc(&state)) {
        ls_runtime.persistent.magic = LS_BOOT_MAGIC;
        ls_runtime.persistent.version = LS_BOOT_VERSION;
        return LS_ECORRUPT;
    }
    ls_runtime.persistent = state;
    return LS_OK;
}
ls_result_t ls_boot_state_save(void) {
    if (!ls_runtime.storage || !ls_runtime.storage->write)
        return LS_OK;
    if (ls_runtime.storage->capacity < boot_offset() + sizeof(ls_persistent_boot_t))
        return LS_ENOSPACE;
    ls_runtime.persistent.magic = LS_BOOT_MAGIC;
    ls_runtime.persistent.version = LS_BOOT_VERSION;
    ls_runtime.persistent.previous_uptime_ms = ls_uptime_ms();
    ls_runtime.persistent.last_sequence = ls_runtime.sequence;
    ls_runtime.persistent.crc = boot_crc(&ls_runtime.persistent);
    ls_result_t result = ls_storage_program(ls_runtime.storage, boot_offset(),
                                            &ls_runtime.persistent, sizeof ls_runtime.persistent);
    if (result == LS_OK && ls_runtime.storage->sync)
        result = ls_runtime.storage->sync(ls_runtime.storage->context);
    return result;
}
void ls_boot_state_mark_crash(uint32_t event_id) {
    ls_runtime.persistent.crash_pending = 1;
    ls_runtime.persistent.boot_successful = 0;
    ls_runtime.persistent.pending_crash_id = event_id;
    (void)ls_boot_state_save();
}
void ls_reset_mark_expected(bool expected) {
    ls_runtime.persistent.expected_reset = expected ? 1u : 0u;
    ls_runtime.reset_info.expected = expected;
    (void)ls_boot_state_save();
}
void ls_boot_mark_successful(void) {
    ls_runtime.persistent.boot_successful = 1;
    ls_runtime.persistent.crash_pending = 0;
    ls_runtime.persistent.consecutive_failures = 0;
    (void)ls_boot_state_save();
}
bool ls_safe_mode_requested(void) {
    return ls_runtime.safe_mode;
}
void ls_release_mark_pending(const char *release) {
    ls_runtime.persistent.release_hash = ls_hash_string(release);
    ls_runtime.persistent.release_state = 1;
    (void)ls_boot_state_save();
}
void ls_release_confirm(void) {
    ls_runtime.persistent.release_state = 2;
    (void)ls_boot_state_save();
}
void ls_release_mark_rollback(void) {
    ls_runtime.persistent.release_state = 3;
    (void)ls_boot_state_save();
}

bool ls_update_version_allowed(uint32_t version_counter) {
    return version_counter != 0u && version_counter >= ls_runtime.persistent.update_version_floor;
}

ls_result_t ls_update_stage(uint32_t version_counter, uint32_t image_fingerprint,
                            uint32_t signing_key_id) {
    if (!ls_update_version_allowed(version_counter) || image_fingerprint == 0u ||
        signing_key_id == 0u)
        return LS_EINVAL;
    ls_runtime.persistent.update_pending_version = version_counter;
    ls_runtime.persistent.update_image_fingerprint = image_fingerprint;
    ls_runtime.persistent.update_signing_key_id = signing_key_id;
    ls_runtime.persistent.update_state = (uint8_t)LS_UPDATE_PENDING;
    return ls_boot_state_save();
}

ls_result_t ls_update_confirm_version(uint32_t version_counter) {
    if (ls_runtime.persistent.update_state != (uint8_t)LS_UPDATE_PENDING || version_counter == 0u ||
        version_counter != ls_runtime.persistent.update_pending_version)
        return LS_EINVAL;
    if (version_counter > ls_runtime.persistent.update_version_floor)
        ls_runtime.persistent.update_version_floor = version_counter;
    ls_runtime.persistent.update_pending_version = 0u;
    ls_runtime.persistent.update_state = (uint8_t)LS_UPDATE_CONFIRMED;
    return ls_boot_state_save();
}

ls_result_t ls_update_mark_version_rollback(uint32_t failed_version) {
    if (ls_runtime.persistent.update_state != (uint8_t)LS_UPDATE_PENDING || failed_version == 0u ||
        failed_version != ls_runtime.persistent.update_pending_version)
        return LS_EINVAL;
    if (ls_runtime.persistent.update_rollback_count != UINT32_MAX)
        ls_runtime.persistent.update_rollback_count++;
    ls_runtime.persistent.update_pending_version = 0u;
    ls_runtime.persistent.update_state = (uint8_t)LS_UPDATE_ROLLED_BACK;
    return ls_boot_state_save();
}

ls_result_t ls_update_abort_stage(uint32_t version_counter) {
    if (ls_runtime.persistent.update_state != (uint8_t)LS_UPDATE_PENDING || version_counter == 0u ||
        version_counter != ls_runtime.persistent.update_pending_version) {
        return LS_EINVAL;
    }
    ls_runtime.persistent.update_pending_version = 0u;
    ls_runtime.persistent.update_image_fingerprint = 0u;
    ls_runtime.persistent.update_signing_key_id = 0u;
    ls_runtime.persistent.update_state = (uint8_t)LS_UPDATE_NONE;
    return ls_boot_state_save();
}

ls_update_state_t ls_update_get_state(void) {
    return (ls_update_state_t){
        .confirmed_version_floor = ls_runtime.persistent.update_version_floor,
        .pending_version = ls_runtime.persistent.update_pending_version,
        .image_fingerprint = ls_runtime.persistent.update_image_fingerprint,
        .signing_key_id = ls_runtime.persistent.update_signing_key_id,
        .rollback_count = ls_runtime.persistent.update_rollback_count,
        .state = (ls_update_state_code_t)ls_runtime.persistent.update_state,
    };
}
