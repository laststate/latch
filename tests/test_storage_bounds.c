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

static uint8_t fake_bytes[32];
static ls_result_t fake_read_result = LS_OK;
static ls_result_t fake_write_result = LS_OK;
static ls_result_t fake_read(void *context, size_t offset, void *dst, size_t length) {
    (void)context;
    if (fake_read_result != LS_OK) return fake_read_result;
    if (offset > sizeof(fake_bytes) || length > sizeof(fake_bytes) - offset) return LS_EINVAL;
    if (length) memcpy(dst, fake_bytes + offset, length);
    return LS_OK;
}
static ls_result_t fake_write(void *context, size_t offset, const void *src, size_t length) {
    (void)context;
    if (fake_write_result != LS_OK) return fake_write_result;
    if (offset > sizeof(fake_bytes) || length > sizeof(fake_bytes) - offset) return LS_EINVAL;
    if (length) memcpy(fake_bytes + offset, src, length);
    return LS_OK;
}

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

    /* Exhaust the public memory-backend defensive matrix. */
    CHECK(ls_memory_storage_read(NULL, 0, copy, 0) == LS_EINVAL);
    ls_memory_storage_t no_data = {NULL, 10u};
    CHECK(ls_memory_storage_read(&no_data, 0, copy, 0) == LS_EINVAL);
    CHECK(ls_memory_storage_read(&memory, 0, NULL, 0) == LS_OK);
    CHECK(ls_memory_storage_write(&memory, 0, NULL, 0) == LS_OK);
    CHECK(ls_memory_storage_erase(NULL, 0, 0) == LS_EINVAL);
    CHECK(ls_memory_storage_erase(&memory, 64u, 0u) == LS_OK);

    ls_storage_backend_t backend = {.name="fake", .capacity=sizeof(fake_bytes),
        .read=fake_read, .write=fake_write};
    uint8_t input[8] = {0,0,0,0,0,0,0,0};
    CHECK(ls_storage_program(NULL, 0u, input, 1u) == LS_EINVAL);
    ls_storage_backend_t missing = backend; missing.read = NULL;
    CHECK(ls_storage_program(&missing, 0u, input, 1u) == LS_EINVAL);
    missing = backend; missing.write = NULL;
    CHECK(ls_storage_program(&missing, 0u, input, 1u) == LS_EINVAL);
    CHECK(ls_storage_program(&backend, sizeof(fake_bytes)+1u, input, 0u) == LS_EINVAL);
    CHECK(ls_storage_program(&backend, sizeof(fake_bytes), input, 1u) == LS_EINVAL);
    CHECK(ls_storage_program(&backend, 0u, NULL, 1u) == LS_EINVAL);
    CHECK(ls_storage_program(&backend, 0u, NULL, 0u) == LS_OK);

    memset(fake_bytes, 0xff, sizeof(fake_bytes));
    backend.write_size = LS_STORAGE_MAX_WRITE_SIZE + 1u;
    CHECK(ls_storage_program(&backend, 0u, input, 1u) == LS_ENOTSUP);
    backend.write_size = 1u;
    fake_write_result = LS_EIO;
    CHECK(ls_storage_program(&backend, 0u, input, 1u) == LS_EIO);
    fake_write_result = LS_OK;
    CHECK(ls_storage_program(&backend, 0u, input, 1u) == LS_OK);

    /* Aligned programming: unchanged units, changed units, read/write failures, and NOR 0->1 rejection. */
    memset(fake_bytes, 0xff, sizeof(fake_bytes));
    backend.write_size = 4u;
    uint8_t unchanged = 0xffu;
    CHECK(ls_storage_program(&backend, 1u, &unchanged, 1u) == LS_OK);
    CHECK(ls_storage_program(&backend, 1u, input, 3u) == LS_OK);
    uint8_t programmed_one = 1u;
    CHECK(ls_storage_program(&backend, 1u, &programmed_one, 1u) == LS_EIO);
    memset(fake_bytes, 0xff, sizeof(fake_bytes));
    fake_read_result = LS_EIO;
    CHECK(ls_storage_program(&backend, 0u, input, 1u) == LS_EIO);
    fake_read_result = LS_OK;
    fake_write_result = LS_EIO;
    CHECK(ls_storage_program(&backend, 0u, input, 1u) == LS_EIO);
    fake_write_result = LS_OK;

    ls_storage_backend_t odd = backend; odd.capacity = 5u;
    CHECK(ls_storage_program(&odd, 4u, input, 1u) == LS_ENOSPACE);
    ls_storage_backend_t huge = backend; huge.capacity = SIZE_MAX;
    CHECK(ls_storage_program(&huge, SIZE_MAX-1u, input, 1u) == LS_EOVERFLOW);

    int erased = -1;
    CHECK(ls_storage_is_erased(&backend, 0u, 1u, NULL) == LS_EINVAL);
    missing = backend; missing.read = NULL;
    CHECK(ls_storage_is_erased(&missing, 0u, 1u, &erased) == LS_EINVAL);
    memset(fake_bytes, 0xff, sizeof(fake_bytes));
    fake_read_result = LS_EIO;
    CHECK(ls_storage_is_erased(&backend, 0u, 1u, &erased) == LS_EIO);
    fake_read_result = LS_OK;
    CHECK(ls_storage_is_erased(&backend, 0u, sizeof(fake_bytes), &erased) == LS_OK && erased == 1);
    fake_bytes[17] = 0u;
    CHECK(ls_storage_is_erased(&backend, 0u, sizeof(fake_bytes), &erased) == LS_OK && erased == 0);
    return 0;
}
