#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "flash check failed: %s:%d\n", #condition, __LINE__);                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

enum {
    ERASE_SIZE = 1024u,
    WRITE_SIZE = 16u,
};

typedef struct {
    ls_storage_sim_t *simulator;
    unsigned erase_calls;
} strict_flash_t;

static uint8_t physical[8192];
static uint8_t baseline[8192];
static uint8_t workspace[1024];
static uint8_t recovery_workspace[1024];

static ls_result_t strict_read(void *context, size_t offset, void *destination, size_t length) {
    strict_flash_t *flash = (strict_flash_t *)context;
    return ls_storage_sim_read(flash->simulator, offset, destination, length);
}

static ls_result_t strict_write(void *context, size_t offset, const void *source, size_t length) {
    strict_flash_t *flash = (strict_flash_t *)context;
    if (offset % WRITE_SIZE || length % WRITE_SIZE) {
        return LS_EINVAL;
    }

    return ls_storage_sim_write(flash->simulator, offset, source, length);
}

static ls_result_t strict_erase(void *context, size_t offset, size_t length) {
    strict_flash_t *flash = (strict_flash_t *)context;
    if (offset % ERASE_SIZE || length % ERASE_SIZE) {
        return LS_EINVAL;
    }

    flash->erase_calls++;
    return ls_storage_sim_erase(flash->simulator, offset, length);
}

static ls_result_t strict_sync(void *context) {
    strict_flash_t *flash = (strict_flash_t *)context;
    return ls_storage_sim_sync(flash->simulator);
}

static ls_storage_backend_t raw_backend(strict_flash_t *flash) {
    return (ls_storage_backend_t){
        .name = "strict-nor-sim",
        .context = flash,
        .capacity = sizeof(physical),
        .erase_size = ERASE_SIZE,
        .write_size = WRITE_SIZE,
        .read = strict_read,
        .write = strict_write,
        .erase = strict_erase,
        .sync = strict_sync,
    };
}

int main(void) {
    CHECK(ls_flash_mirror_physical_size_for_write(sizeof(workspace), ERASE_SIZE, WRITE_SIZE) <=
          sizeof(physical));

    memset(physical, 0xff, sizeof(physical));
    ls_storage_sim_t simulator = {
        .data = physical,
        .size = sizeof(physical),
        .enforce_nor = true,
    };
    strict_flash_t flash = {.simulator = &simulator};
    ls_storage_backend_t raw = raw_backend(&flash);
    ls_flash_mirror_t mirror;
    CHECK(ls_flash_mirror_init(&mirror, &raw, workspace, sizeof(workspace)) == LS_OK);

    uint8_t old_value[32];
    uint8_t new_value[32];
    uint8_t readback[32];
    memset(old_value, 0x11, sizeof(old_value));
    memset(new_value, 0x77, sizeof(new_value));
    CHECK(mirror.backend.write(mirror.backend.context, 100u, old_value, sizeof(old_value)) ==
          LS_OK);
    memcpy(baseline, physical, sizeof(physical));

    flash.erase_calls = 0u;
    ls_storage_sim_fail_at(&simulator, 1u, 0u);
    CHECK(ls_flash_mirror_init(&mirror, &raw, workspace, sizeof(workspace)) == LS_EIO);
    CHECK(flash.erase_calls == 0u);
    ls_storage_sim_reset_faults(&simulator);

    for (uint32_t operation = 1u; operation <= 140u; operation++) {
        memcpy(physical, baseline, sizeof(physical));
        simulator = (ls_storage_sim_t){
            .data = physical,
            .size = sizeof(physical),
            .enforce_nor = true,
        };
        flash = (strict_flash_t){.simulator = &simulator};
        raw = raw_backend(&flash);
        CHECK(ls_flash_mirror_init(&mirror, &raw, workspace, sizeof(workspace)) == LS_OK);

        ls_storage_sim_fail_at(&simulator, operation, 8u);
        (void)mirror.backend.write(mirror.backend.context, 100u, new_value, sizeof(new_value));
        ls_storage_sim_reset_faults(&simulator);

        ls_flash_mirror_t recovered;
        CHECK(ls_flash_mirror_init(&recovered, &raw, recovery_workspace,
                                   sizeof(recovery_workspace)) == LS_OK);
        CHECK(recovered.backend.read(recovered.backend.context, 100u, readback, sizeof(readback)) ==
              LS_OK);
        CHECK(!memcmp(readback, old_value, sizeof(readback)) ||
              !memcmp(readback, new_value, sizeof(readback)));
    }

    return 0;
}
