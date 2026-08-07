#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "wear check failed: %s:%d\n", #condition, __LINE__);                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

enum {
    ERASE_SIZE = 1024u,
    WRITE_SIZE = 16u,
};

typedef struct {
    ls_storage_sim_t *simulator;
} strict_flash_t;

static uint8_t flash[16384];
static uint8_t workspace[512];
static uint8_t recovery[512];

static ls_result_t strict_read(void *context, size_t offset, void *destination, size_t length) {
    strict_flash_t *flash_context = (strict_flash_t *)context;
    return ls_storage_sim_read(flash_context->simulator, offset, destination, length);
}

static ls_result_t strict_write(void *context, size_t offset, const void *source, size_t length) {
    strict_flash_t *flash_context = (strict_flash_t *)context;
    if (offset % WRITE_SIZE || length % WRITE_SIZE) {
        return LS_EINVAL;
    }

    return ls_storage_sim_write(flash_context->simulator, offset, source, length);
}

static ls_result_t strict_erase(void *context, size_t offset, size_t length) {
    strict_flash_t *flash_context = (strict_flash_t *)context;
    if (offset % ERASE_SIZE || length % ERASE_SIZE) {
        return LS_EINVAL;
    }

    return ls_storage_sim_erase(flash_context->simulator, offset, length);
}

static ls_result_t strict_sync(void *context) {
    strict_flash_t *flash_context = (strict_flash_t *)context;
    return ls_storage_sim_sync(flash_context->simulator);
}

static ls_storage_backend_t raw_backend(strict_flash_t *flash_context) {
    return (ls_storage_backend_t){
        .name = "strict-nor",
        .context = flash_context,
        .capacity = sizeof(flash),
        .erase_size = ERASE_SIZE,
        .write_size = WRITE_SIZE,
        .read = strict_read,
        .write = strict_write,
        .erase = strict_erase,
        .sync = strict_sync,
    };
}

int main(void) {
    CHECK(ls_flash_wear_physical_size(0u, ERASE_SIZE, 8u) == 0u);
    CHECK(ls_flash_wear_physical_size(64u, 0u, 8u) == 0u);
    CHECK(ls_flash_wear_physical_size(64u, ERASE_SIZE, 1u) == 0u);
    CHECK(ls_flash_wear_physical_size(64u, ERASE_SIZE, LS_FLASH_WEAR_MAX_SLOTS + 1u) == 0u);
    CHECK(ls_flash_wear_physical_size_for_write(64u, ERASE_SIZE, ERASE_SIZE + 1u, 8u) == 0u);
    CHECK(ls_flash_wear_physical_size_for_write(64u, ERASE_SIZE, 0u, 8u) > 0u);
    ls_flash_wear_level_t invalid;
    uint8_t invalid_workspace[64];
    ls_storage_backend_t invalid_backend = {0};
    CHECK(ls_flash_wear_init(NULL, &invalid_backend, invalid_workspace, 64u, 2u) == LS_EINVAL);
    CHECK(ls_flash_wear_init(&invalid, NULL, invalid_workspace, 64u, 2u) == LS_EINVAL);
    CHECK(ls_flash_wear_init(&invalid, &invalid_backend, invalid_workspace, 64u, 2u) == LS_EINVAL);
    CHECK(ls_flash_wear_physical_size_for_write(sizeof(workspace), ERASE_SIZE, WRITE_SIZE, 8u) <=
          sizeof(flash));

    memset(flash, 0xff, sizeof(flash));
    ls_storage_sim_t simulator = {
        .data = flash,
        .size = sizeof(flash),
        .enforce_nor = true,
    };
    strict_flash_t flash_context = {.simulator = &simulator};
    ls_storage_backend_t backend = raw_backend(&flash_context);
    ls_flash_wear_level_t wear;
    CHECK(ls_flash_wear_init(&wear, &backend, workspace, sizeof(workspace), 8u) == LS_OK);

    for (uint32_t sample = 0u; sample < 80u; sample++) {
        CHECK(wear.backend.write(wear.backend.context, 0u, &sample, sizeof(sample)) == LS_OK);
    }
    uint32_t value = 0u;

    CHECK(wear.backend.read(NULL, 0u, &value, 1u) == LS_EINVAL);
    CHECK(wear.backend.read(wear.backend.context, 0u, NULL, 1u) == LS_EINVAL);
    CHECK(wear.backend.read(wear.backend.context, sizeof(workspace) + 1u, &value, 0u) == LS_EINVAL);
    CHECK(wear.backend.write(NULL, 0u, &value, 1u) == LS_EINVAL);
    CHECK(wear.backend.write(wear.backend.context, 0u, NULL, 1u) == LS_EINVAL);
    CHECK(wear.backend.erase(NULL, 0u, 1u) == LS_EINVAL);
    CHECK(wear.backend.sync(NULL) == LS_EINVAL);
    CHECK(wear.backend.erase(wear.backend.context, 8u, sizeof(value)) == LS_OK);

    ls_flash_wear_stats_t stats = ls_flash_wear_stats(&wear);
    ls_flash_wear_stats_t empty_stats = ls_flash_wear_stats(NULL);
    CHECK(empty_stats.slots == 0u && empty_stats.minimum_erases == 0u);
    CHECK(stats.slots == 8u);
    CHECK(stats.maximum_erases - stats.minimum_erases <= 1u);
    CHECK(stats.generation == 82u);
    ls_flash_wear_lifetime_t lifetime;
    CHECK(ls_flash_wear_lifetime(&wear, 100000u, &lifetime) == LS_OK);
    CHECK(lifetime.rated_erase_cycles == 100000u);
    CHECK(lifetime.maximum_erases == stats.maximum_erases);
    CHECK(!lifetime.exhausted && lifetime.estimated_commits_remaining > 0u);
    CHECK(ls_flash_wear_lifetime(NULL, 100000u, &lifetime) == LS_EINVAL);
    CHECK(ls_flash_wear_lifetime(&wear, 0u, &lifetime) == LS_EINVAL);
    CHECK(ls_flash_wear_lifetime(&wear, 100000u, NULL) == LS_EINVAL);

    CHECK(wear.backend.read(wear.backend.context, 0u, &value, sizeof(value)) == LS_OK);
    CHECK(value == 79u);

    ls_flash_wear_level_t reopened;
    CHECK(ls_flash_wear_init(&reopened, &backend, recovery, sizeof(recovery), 8u) == LS_OK);
    CHECK(reopened.backend.read(reopened.backend.context, 0u, &value, sizeof(value)) == LS_OK);
    CHECK(value == 79u);

    return 0;
}
