#include "internal.h"
#define LS_BOOT_MAGIC 0x544F4F42u
#define LS_BOOT_VERSION 1u

static size_t boot_offset(void) {
    return ls_spool_storage_size();
}
static uint32_t boot_crc(const ls_persistent_boot_t *state) {
    return ls_crc32(state, offsetof(ls_persistent_boot_t, crc));
}
ls_result_t ls_boot_state_load(void) {
    ls_memset(&ls_runtime.persistent, 0, sizeof ls_runtime.persistent);
    if (!ls_runtime.storage || !ls_runtime.storage->read)
        return LS_OK;
    if (ls_runtime.storage->capacity < boot_offset() + sizeof(ls_persistent_boot_t))
        return LS_ENOSPACE;
    ls_persistent_boot_t state;
    ls_result_t result =
        ls_runtime.storage->read(ls_runtime.storage->context, boot_offset(), &state, sizeof state);
    if (result != LS_OK)
        return result;
    if (state.magic != LS_BOOT_MAGIC || state.version != LS_BOOT_VERSION ||
        state.crc != boot_crc(&state)) {
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
