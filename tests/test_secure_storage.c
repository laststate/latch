#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "secure storage failed: %s:%d\n", #x, __LINE__);                       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint8_t physical[32768], wear_workspace[2048], secure_workspace[2048], wear_recovery[2048],
    secure_recovery[2048];
static uint32_t rng = 1;
static ls_result_t random_bytes(void *context, uint8_t *out, size_t length) {
    uint32_t *state = (uint32_t *)context;
    for (size_t i = 0; i < length; i++) {
        *state = *state * 1664525u + 1013904223u;
        out[i] = (uint8_t)(*state >> 24);
    }
    return LS_OK;
}
static ls_storage_backend_t raw(ls_storage_sim_t *sim) {
    ls_storage_backend_t backend = {"nor",
                                    sim,
                                    sizeof physical,
                                    2048,
                                    1,
                                    ls_storage_sim_read,
                                    ls_storage_sim_write,
                                    ls_storage_sim_erase,
                                    ls_storage_sim_sync};
    return backend;
}
static int contains(const uint8_t *haystack, size_t haystack_length, const uint8_t *needle,
                    size_t needle_length) {
    if (needle_length > haystack_length)
        return 0;
    for (size_t i = 0; i <= haystack_length - needle_length; i++)
        if (!memcmp(haystack + i, needle, needle_length))
            return 1;
    return 0;
}
static int all_zero(const uint8_t *data, size_t length) {
    for (size_t index = 0; index < length; ++index)
        if (data[index] != 0u)
            return 0;
    return 1;
}
int main(void) {
    memset(physical, 0xff, sizeof physical);
    ls_storage_sim_t sim = {physical, sizeof physical, true, 0, 0, 0};
    ls_storage_backend_t backend = raw(&sim);
    size_t sealed = ls_secure_storage_sealed_size(512);
    CHECK(sealed < sizeof wear_workspace);
    ls_flash_wear_level_t wear;
    CHECK(ls_flash_wear_init(&wear, &backend, wear_workspace, sealed, 8) == LS_OK);
    uint8_t key[32];
    for (unsigned i = 0; i < 32; i++)
        key[i] = (uint8_t)i;
    ls_secure_storage_t secure;
    CHECK(ls_secure_storage_init(&secure, &wear.backend, secure_workspace, sizeof secure_workspace,
                                 512, key, 7, random_bytes, &rng) == LS_OK);
    uint8_t secret[64], readback[64];
    memset(secret, 0x37, sizeof secret);
    CHECK(secure.backend.write(secure.backend.context, 100, secret, sizeof secret) == LS_OK);
    CHECK(secure.backend.read(secure.backend.context, 100, readback, sizeof readback) == LS_OK);
    CHECK(!memcmp(secret, readback, sizeof secret));
    CHECK(!contains(physical, sizeof physical, secret, sizeof secret));
    ls_flash_wear_level_t reopened_wear;
    CHECK(ls_flash_wear_init(&reopened_wear, &backend, wear_recovery, sealed, 8) == LS_OK);
    ls_secure_storage_t reopened;
    CHECK(ls_secure_storage_init(&reopened, &reopened_wear.backend, secure_recovery,
                                 sizeof secure_recovery, 512, key, 7, random_bytes, &rng) == LS_OK);
    CHECK(reopened.backend.read(reopened.backend.context, 100, readback, sizeof readback) == LS_OK);
    CHECK(!memcmp(secret, readback, sizeof secret));
    uint8_t rotated_key[32];
    memset(rotated_key, 0x5c, sizeof rotated_key);
    CHECK(ls_secure_storage_rotate_key(&reopened, rotated_key, 8) == LS_OK);
    CHECK(reopened.key_id == 8u);
    CHECK(reopened.backend.read(reopened.backend.context, 100, readback, sizeof readback) == LS_OK);
    CHECK(!memcmp(secret, readback, sizeof secret));
    ls_secure_storage_destroy(&reopened);

    CHECK(ls_flash_wear_init(&reopened_wear, &backend, wear_recovery, sealed, 8) == LS_OK);
    CHECK(ls_secure_storage_init(&reopened, &reopened_wear.backend, secure_recovery,
                                 sizeof secure_recovery, 512, key, 7, random_bytes,
                                 &rng) == LS_ECORRUPT);
    CHECK(ls_secure_storage_init(&reopened, &reopened_wear.backend, secure_recovery,
                                 sizeof secure_recovery, 512, rotated_key, 8, random_bytes,
                                 &rng) == LS_OK);
    CHECK(reopened.backend.read(reopened.backend.context, 100, readback, sizeof readback) == LS_OK);
    CHECK(!memcmp(secret, readback, sizeof secret));
    CHECK(ls_secure_storage_rotate_key(0, rotated_key, 9) == LS_EINVAL);
    CHECK(ls_secure_storage_rotate_key(&reopened, 0, 9) == LS_EINVAL);
    CHECK(ls_secure_storage_rotate_key(&reopened, rotated_key, 0) == LS_EINVAL);
    CHECK(ls_secure_storage_rotate_key(&reopened, rotated_key, 8) == LS_EINVAL);
    ls_storage_backend_t *saved_storage = reopened.storage;
    uint8_t *saved_workspace = reopened.workspace;
    reopened.storage = 0;
    CHECK(ls_secure_storage_rotate_key(&reopened, rotated_key, 9) == LS_EINVAL);
    reopened.storage = saved_storage;
    reopened.workspace = 0;
    CHECK(ls_secure_storage_rotate_key(&reopened, rotated_key, 9) == LS_EINVAL);
    reopened.workspace = saved_workspace;
    uint8_t failed_key[32];
    memset(failed_key, 0x91, sizeof failed_key);
    ls_storage_sim_fail_at(&sim, 1, 0);
    CHECK(ls_secure_storage_rotate_key(&reopened, failed_key, 9) != LS_OK);
    CHECK(reopened.key_id == 8u && all_zero(reopened.workspace, reopened.workspace_size));
    ls_storage_sim_reset_faults(&sim);
    /* Operation 1 is the read performed by load_image(); fail the first
       persistence
     * operation so rotation must restore the old in-memory key. */
    ls_storage_sim_fail_at(&sim, 2, 0);
    CHECK(ls_secure_storage_rotate_key(&reopened, failed_key, 9) != LS_OK);
    CHECK(reopened.key_id == 8u);
    ls_storage_sim_reset_faults(&sim);
    CHECK(reopened.backend.read(reopened.backend.context, 100, readback, sizeof readback) == LS_OK);
    CHECK(!memcmp(secret, readback, sizeof secret));
    uint8_t wrong[32];
    memset(wrong, 0xaa, sizeof wrong);
    ls_secure_storage_t rejected;
    ls_secure_storage_destroy(&reopened);
    CHECK(ls_secure_storage_init(&rejected, &reopened_wear.backend, secure_recovery,
                                 sizeof secure_recovery, 512, wrong, 8, random_bytes,
                                 &rng) == LS_EAUTH);
    return 0;
}
