#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "storage bounds failed: %s:%d\n", #x, __LINE__);                       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    uint8_t bytes[64], copy[64];
    memset(bytes, 0xaa, sizeof bytes);
    ls_memory_storage_t memory = {bytes, sizeof bytes};
    CHECK(ls_memory_storage_read(&memory, 0, 0, 1) == LS_EINVAL);
    CHECK(ls_memory_storage_write(&memory, 0, 0, 1) == LS_EINVAL);
    CHECK(ls_memory_storage_read(&memory, 64, copy, 0) == LS_OK);
    CHECK(ls_memory_storage_read(&memory, 65, copy, 0) == LS_EINVAL);
    CHECK(ls_memory_storage_write(&memory, 63, copy, 2) == LS_EINVAL);
    CHECK(ls_memory_storage_erase(&memory, 63, 2) == LS_EINVAL);
    ls_storage_sim_t simulator = {bytes, sizeof bytes, false, 0, 0, 0};
    CHECK(ls_storage_sim_read(&simulator, 0, 0, 1) == LS_EINVAL);
    CHECK(ls_storage_sim_write(&simulator, 0, 0, 1) == LS_EINVAL);
    CHECK(ls_storage_sim_erase(&simulator, 60, 5) == LS_EINVAL);
    memset(bytes, 0xff, sizeof bytes);
    simulator.enforce_nor = true;
    uint8_t zero = 0, one = 1;
    CHECK(ls_storage_sim_write(&simulator, 0, &zero, 1) == LS_OK);
    CHECK(ls_storage_sim_write(&simulator, 0, &one, 1) == LS_EIO);
    CHECK(ls_storage_sim_erase(&simulator, 0, 1) == LS_OK);
    CHECK(ls_storage_sim_write(&simulator, 0, &one, 1) == LS_OK);
    ls_storage_sim_fail_at(&simulator, 1, 0);
    CHECK(ls_storage_sim_sync(&simulator) == LS_EIO);
    ls_storage_sim_reset_faults(&simulator);
    CHECK(ls_storage_sim_sync(&simulator) == LS_OK);
    return 0;
}
