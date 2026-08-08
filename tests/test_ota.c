#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "ota check failed: %s:%d\n", #x, __LINE__);                            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint8_t storage_bytes[50000];
typedef struct {
    unsigned auth, stage, boot, confirm, rollback;
    uint32_t expected_version;
    uint32_t expected_key;
    ls_result_t auth_result, stage_result, boot_result, confirm_result, rollback_result;
} fake_t;
static ls_result_t authenticate(void *ctx, uint32_t version, uint32_t key, const uint8_t digest[32],
                                const uint8_t *sig, size_t siglen) {
    fake_t *f = ctx;
    f->auth++;
    CHECK(version == f->expected_version);
    CHECK(key == f->expected_key);
    CHECK(digest[0] || digest[1] || digest[2] || digest[3]);
    CHECK(sig && siglen == 4u);
    return f->auth_result;
}
static ls_result_t stage(void *ctx, const uint8_t *image, size_t length) {
    fake_t *f = ctx;
    f->stage++;
    CHECK(image && length == 8u);
    return f->stage_result;
}
static ls_result_t request_boot(void *ctx, uint32_t version) {
    fake_t *f = ctx;
    f->boot++;
    CHECK(version == f->expected_version);
    return f->boot_result;
}
static ls_result_t confirm(void *ctx, uint32_t version) {
    fake_t *f = ctx;
    f->confirm++;
    CHECK(version == f->expected_version);
    return f->confirm_result;
}
static ls_result_t rollback(void *ctx, uint32_t version) {
    fake_t *f = ctx;
    f->rollback++;
    CHECK(version == f->expected_version);
    return f->rollback_result;
}

int main(void) {
    memset(storage_bytes, 0xff, sizeof storage_bytes);
    static const ls_identity_t identity = {.project_id = "ota",
                                           .device_id = "auv",
                                           .firmware_version = "0.3.0",
                                           .firmware_build_id = "ota-build-0001"};
    ls_config_t config = {.identity = &identity};
    ls_memory_storage_t memory = {storage_bytes, sizeof storage_bytes};
    ls_storage_backend_t storage = {.name = "ram",
                                    .context = &memory,
                                    .capacity = sizeof storage_bytes,
                                    .read = ls_memory_storage_read,
                                    .write = ls_memory_storage_write,
                                    .erase = ls_memory_storage_erase};
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    CHECK(ls_boot() == LS_OK);
    fake_t fake = {.expected_version = 5u, .expected_key = 9u};
    ls_ota_backend_t backend = {.context = &fake,
                                .authenticate = authenticate,
                                .stage = stage,
                                .request_boot = request_boot,
                                .confirm = confirm,
                                .rollback = rollback};
    const uint8_t image[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t signature[4] = {9, 8, 7, 6};

    CHECK(ls_ota_stage_candidate(&backend, 5u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_OK);
    CHECK(fake.auth == 1u && fake.stage == 1u && fake.boot == 1u);
    CHECK(ls_update_get_state().state == LS_UPDATE_PENDING);
    CHECK(ls_get_reset_info().expected);
    CHECK(ls_ota_confirm_running(&backend, 5u) == LS_OK);
    CHECK(fake.confirm == 1u);
    CHECK(ls_update_get_state().confirmed_version_floor == 5u);
    CHECK(!ls_update_version_allowed(4u));

    /* Backend boot-request failure must not leave a phantom pending candidate. */
    fake.expected_version = 6u;
    fake.boot_result = LS_EIO;
    fake.auth = fake.stage = fake.boot = 0u;
    CHECK(ls_ota_stage_candidate(&backend, 6u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_EIO);
    CHECK(fake.auth == 1u && fake.stage == 1u && fake.boot == 1u);
    CHECK(ls_update_get_state().state == LS_UPDATE_NONE);
    CHECK(ls_update_get_state().confirmed_version_floor == 5u);

    /* A newer candidate can be staged again and explicitly rolled back. */
    fake.boot_result = LS_OK;
    fake.rollback_result = LS_OK;
    fake.auth = fake.stage = fake.boot = fake.rollback = 0u;
    CHECK(ls_ota_stage_candidate(&backend, 6u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_OK);
    CHECK(ls_update_get_state().state == LS_UPDATE_PENDING);
    CHECK(ls_ota_request_rollback(&backend, 6u) == LS_OK);
    CHECK(fake.rollback == 1u);
    CHECK(ls_update_get_state().state == LS_UPDATE_ROLLED_BACK);
    CHECK(ls_update_get_state().rollback_count == 1u);
    CHECK(ls_update_get_state().confirmed_version_floor == 5u);

    /* Authentication failure must not stage or alter persistent update state. */
    fake.expected_version = 7u;
    fake.auth_result = LS_EAUTH;
    fake.auth = fake.stage = fake.boot = 0u;
    CHECK(ls_ota_stage_candidate(&backend, 7u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_EAUTH);
    CHECK(fake.auth == 1u && fake.stage == 0u && fake.boot == 0u);
    CHECK(ls_update_get_state().state == LS_UPDATE_ROLLED_BACK);

    /* Defensive API matrix: every required callback and argument is fail closed. */
    ls_ota_backend_t broken = backend;
    CHECK(ls_ota_stage_candidate(NULL, 8u, 9u, image, sizeof image, signature, sizeof signature) ==
          LS_EINVAL);
    broken.authenticate = NULL;
    CHECK(ls_ota_stage_candidate(&broken, 8u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_EINVAL);
    broken = backend;
    broken.stage = NULL;
    CHECK(ls_ota_stage_candidate(&broken, 8u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_EINVAL);
    broken = backend;
    broken.request_boot = NULL;
    CHECK(ls_ota_stage_candidate(&broken, 8u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_EINVAL);
    broken = backend;
    broken.confirm = NULL;
    CHECK(ls_ota_stage_candidate(&broken, 8u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_EINVAL);
    broken = backend;
    broken.rollback = NULL;
    CHECK(ls_ota_stage_candidate(&broken, 8u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_EINVAL);
    CHECK(ls_ota_stage_candidate(&backend, 0u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_EINVAL);
    CHECK(ls_ota_stage_candidate(&backend, 8u, 0u, image, sizeof image, signature,
                                 sizeof signature) == LS_EINVAL);
    CHECK(ls_ota_stage_candidate(&backend, 8u, 9u, NULL, sizeof image, signature,
                                 sizeof signature) == LS_EINVAL);
    CHECK(ls_ota_stage_candidate(&backend, 8u, 9u, image, 0u, signature, sizeof signature) ==
          LS_EINVAL);
    CHECK(ls_ota_stage_candidate(&backend, 8u, 9u, image, sizeof image, NULL, sizeof signature) ==
          LS_EINVAL);
    CHECK(ls_ota_stage_candidate(&backend, 8u, 9u, image, sizeof image, signature, 0u) ==
          LS_EINVAL);
    CHECK(ls_ota_stage_candidate(&backend, 4u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_EINVAL);
    CHECK(ls_ota_confirm_running(NULL, 7u) == LS_EINVAL);
    CHECK(ls_ota_confirm_running(&backend, 0u) == LS_EINVAL);
    CHECK(ls_ota_request_rollback(NULL, 7u) == LS_EINVAL);
    CHECK(ls_ota_request_rollback(&backend, 0u) == LS_EINVAL);

    /* Stage failure does not create product metadata. */
    fake.expected_version = 7u;
    fake.auth_result = LS_OK;
    fake.stage_result = LS_EIO;
    fake.auth = fake.stage = fake.boot = 0u;
    CHECK(ls_ota_stage_candidate(&backend, 7u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_EIO);
    CHECK(fake.auth == 1u && fake.stage == 1u && fake.boot == 0u);
    CHECK(ls_update_get_state().state == LS_UPDATE_ROLLED_BACK);

    /* Confirmation backend failure leaves the candidate pending for a safe retry. */
    fake.stage_result = LS_OK;
    fake.confirm_result = LS_EIO;
    fake.auth = fake.stage = fake.boot = fake.confirm = 0u;
    CHECK(ls_ota_stage_candidate(&backend, 7u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_OK);
    CHECK(ls_ota_confirm_running(&backend, 7u) == LS_EIO);
    CHECK(ls_update_get_state().state == LS_UPDATE_PENDING);
    fake.confirm_result = LS_OK;
    CHECK(ls_ota_confirm_running(&backend, 7u) == LS_OK);
    CHECK(ls_update_get_state().confirmed_version_floor == 7u);

    /* Rollback backend failure similarly preserves pending state. */
    fake.expected_version = 8u;
    fake.rollback_result = LS_EIO;
    fake.auth = fake.stage = fake.boot = fake.rollback = 0u;
    CHECK(ls_ota_stage_candidate(&backend, 8u, 9u, image, sizeof image, signature,
                                 sizeof signature) == LS_OK);
    CHECK(ls_ota_request_rollback(&backend, 8u) == LS_EIO);
    CHECK(ls_update_get_state().state == LS_UPDATE_PENDING);
    fake.rollback_result = LS_OK;
    CHECK(ls_ota_request_rollback(&backend, 8u) == LS_OK);
    CHECK(ls_update_get_state().state == LS_UPDATE_ROLLED_BACK);
    puts("ota tests passed");
    return 0;
}
