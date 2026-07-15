#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "secure power loss failed: %s:%d\n", #x, __LINE__);                    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint8_t physical[32768], baseline[32768], wear_space[1024], secure_space[1024],
    reopen_wear_space[1024], reopen_secure_space[1024];
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
static ls_result_t random_bytes(void *context, uint8_t *output, size_t length) {
    uint32_t *state = (uint32_t *)context;
    for (size_t i = 0; i < length; i++) {
        *state = *state * 1103515245u + 12345u;
        output[i] = (uint8_t)(*state >> 16);
    }
    return LS_OK;
}
int main(void) {
    uint8_t key[32];
    for (unsigned i = 0; i < 32; i++)
        key[i] = (uint8_t)(0xa0u + i);
    memset(physical, 0xff, sizeof physical);
    ls_storage_sim_t simulator = {physical, sizeof physical, true, 0, 0, 0};
    ls_storage_backend_t raw_backend = raw(&simulator);
    size_t sealed = ls_secure_storage_sealed_size(256);
    ls_flash_wear_level_t wear;
    CHECK(ls_flash_wear_init(&wear, &raw_backend, wear_space, sealed, 8) == LS_OK);
    uint32_t rng = 3;
    ls_secure_storage_t secure;
    CHECK(ls_secure_storage_init(&secure, &wear.backend, secure_space, sizeof secure_space, 256,
                                 key, 11, random_bytes, &rng) == LS_OK);
    uint8_t old_value[32], new_value[32], readback[32];
    memset(old_value, 0x11, sizeof old_value);
    memset(new_value, 0x77, sizeof new_value);
    CHECK(secure.backend.write(secure.backend.context, 64, old_value, sizeof old_value) == LS_OK);
    memcpy(baseline, physical, sizeof physical);
    for (uint32_t operation = 1; operation <= 6; operation++) {
        memcpy(physical, baseline, sizeof physical);
        simulator = (ls_storage_sim_t){physical, sizeof physical, true, 0, 0, 0};
        raw_backend = raw(&simulator);
        CHECK(ls_flash_wear_init(&wear, &raw_backend, wear_space, sealed, 8) == LS_OK);
        rng = 100u + operation;
        CHECK(ls_secure_storage_init(&secure, &wear.backend, secure_space, sizeof secure_space, 256,
                                     key, 11, random_bytes, &rng) == LS_OK);
        ls_storage_sim_fail_at(&simulator, operation, 16);
        (void)secure.backend.write(secure.backend.context, 64, new_value, sizeof new_value);
        ls_storage_sim_reset_faults(&simulator);
        ls_flash_wear_level_t reopened_wear;
        CHECK(ls_flash_wear_init(&reopened_wear, &raw_backend, reopen_wear_space, sealed, 8) ==
              LS_OK);
        ls_secure_storage_t reopened_secure;
        CHECK(ls_secure_storage_init(&reopened_secure, &reopened_wear.backend, reopen_secure_space,
                                     sizeof reopen_secure_space, 256, key, 11, random_bytes,
                                     &rng) == LS_OK);
        CHECK(reopened_secure.backend.read(reopened_secure.backend.context, 64, readback,
                                           sizeof readback) == LS_OK);
        CHECK(!memcmp(readback, old_value, sizeof readback) ||
              !memcmp(readback, new_value, sizeof readback));
    }
    return 0;
}
