#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"
#include "../src/core/internal.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "update test failed: %s:%d\n", #condition, __LINE__);                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

#define LEGACY_BOOT_MAGIC 0x544F4F42u

typedef struct {
    uint32_t magic, version, boot_count, previous_uptime_ms;
    uint32_t consecutive_failures, first_failure_ms, last_sequence;
    uint32_t pending_crash_id, release_hash;
    uint8_t crash_pending, expected_reset, boot_successful, release_state;
    uint32_t crc;
} legacy_boot_t;

static uint8_t bytes[50000];
static ls_memory_storage_t memory;
static ls_storage_backend_t storage;
static const ls_identity_t identity = {
    .project_id = "update",
    .device_id = "host",
    .firmware_build_id = "update001",
};

static int configure(void) {
    ls_config_t config = {.identity = &identity};
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    CHECK(ls_boot() == LS_OK);
    return 0;
}

static int update_lifecycle(void) {
    memset(bytes, 0xff, sizeof bytes);
    memory = (ls_memory_storage_t){bytes, sizeof bytes};
    storage = (ls_storage_backend_t){
        .name = "ram",
        .context = &memory,
        .capacity = sizeof bytes,
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    CHECK(configure() == 0);
    CHECK(!ls_update_version_allowed(0u));
    CHECK(ls_update_version_allowed(1u));
    CHECK(ls_update_stage(0u, 1u, 1u) == LS_EINVAL);
    CHECK(ls_update_stage(1u, 0u, 1u) == LS_EINVAL);
    CHECK(ls_update_stage(1u, 1u, 0u) == LS_EINVAL);
    CHECK(ls_update_confirm_version(1u) == LS_EINVAL);
    CHECK(ls_update_mark_version_rollback(1u) == LS_EINVAL);

    CHECK(ls_update_stage(5u, 0x12345678u, 7u) == LS_OK);
    ls_update_state_t state = ls_update_get_state();
    CHECK(state.state == LS_UPDATE_PENDING);
    CHECK(state.pending_version == 5u);
    CHECK(state.image_fingerprint == 0x12345678u);
    CHECK(state.signing_key_id == 7u);
    CHECK(ls_update_confirm_version(4u) == LS_EINVAL);
    CHECK(ls_update_confirm_version(5u) == LS_OK);
    state = ls_update_get_state();
    CHECK(state.state == LS_UPDATE_CONFIRMED);
    CHECK(state.confirmed_version_floor == 5u);
    CHECK(!ls_update_version_allowed(4u));
    CHECK(ls_update_version_allowed(5u));
    CHECK(ls_update_stage(4u, 1u, 1u) == LS_EINVAL);

    CHECK(ls_update_stage(5u, 0x22u, 8u) == LS_OK);
    CHECK(ls_update_mark_version_rollback(4u) == LS_EINVAL);
    CHECK(ls_update_mark_version_rollback(5u) == LS_OK);
    state = ls_update_get_state();
    CHECK(state.state == LS_UPDATE_ROLLED_BACK);
    CHECK(state.confirmed_version_floor == 5u);
    CHECK(state.rollback_count == 1u);
    return 0;
}

static int legacy_boot_migration(void) {
    memset(bytes, 0xff, sizeof bytes);
    memory = (ls_memory_storage_t){bytes, sizeof bytes};
    storage = (ls_storage_backend_t){
        .name = "ram",
        .context = &memory,
        .capacity = sizeof bytes,
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    CHECK(configure() == 0);

    legacy_boot_t legacy = {
        .magic = LEGACY_BOOT_MAGIC,
        .version = 1u,
        .boot_count = 41u,
        .previous_uptime_ms = 100u,
        .last_sequence = 77u,
        .boot_successful = 1u,
        .release_state = 2u,
    };
    legacy.crc = ls_crc32(&legacy, offsetof(legacy_boot_t, crc));
    size_t offset = ls_spool_storage_size();
    memset(bytes + offset, 0xff, sizeof(ls_persistent_boot_t));
    memcpy(bytes + offset, &legacy, sizeof legacy);

    CHECK(configure() == 0);
    CHECK(ls_boot_count() == 42u);
    CHECK(ls_update_get_state().state == LS_UPDATE_NONE);
    CHECK(ls_update_version_allowed(1u));
    return 0;
}

int main(void) {
    CHECK(update_lifecycle() == 0);
    CHECK(legacy_boot_migration() == 0);
    return 0;
}
