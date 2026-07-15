#include "../core/internal.h"
#include "laststate/storage_sim.h"
static bool fail(ls_storage_sim_t *simulator) {
    simulator->operation_count++;
    return simulator->fail_operation && simulator->operation_count == simulator->fail_operation;
}
static ls_result_t bounds(ls_storage_sim_t *simulator, size_t offset, size_t length) {
    return !simulator || !simulator->data || offset > simulator->size ||
                   length > simulator->size - offset
               ? LS_EINVAL
               : LS_OK;
}
void ls_storage_sim_reset_faults(ls_storage_sim_t *simulator) {
    if (simulator) {
        simulator->operation_count = 0;
        simulator->fail_operation = 0;
        simulator->partial_bytes = 0;
    }
}
void ls_storage_sim_fail_at(ls_storage_sim_t *simulator, uint32_t operation, size_t partial_bytes) {
    if (simulator) {
        simulator->operation_count = 0;
        simulator->fail_operation = operation;
        simulator->partial_bytes = partial_bytes;
    }
}
ls_result_t ls_storage_sim_read(void *context, size_t offset, void *dst, size_t length) {
    ls_storage_sim_t *simulator = (ls_storage_sim_t *)context;
    if (bounds(simulator, offset, length) != LS_OK || (!dst && length))
        return LS_EINVAL;
    if (fail(simulator))
        return LS_EIO;
    if (length)
        ls_memcpy(dst, simulator->data + offset, length);
    return LS_OK;
}
ls_result_t ls_storage_sim_write(void *context, size_t offset, const void *src, size_t length) {
    ls_storage_sim_t *simulator = (ls_storage_sim_t *)context;
    if (bounds(simulator, offset, length) != LS_OK || (!src && length))
        return LS_EINVAL;
    bool interrupted = fail(simulator);
    size_t written = interrupted
                         ? (simulator->partial_bytes < length ? simulator->partial_bytes : length)
                         : length;
    const uint8_t *input = (const uint8_t *)src;
    if (simulator->enforce_nor)
        for (size_t i = 0; i < written; i++)
            if ((uint8_t)(~simulator->data[offset + i]) & input[i])
                return LS_EIO;
    for (size_t i = 0; i < written; i++)
        simulator->data[offset + i] =
            simulator->enforce_nor ? (uint8_t)(simulator->data[offset + i] & input[i]) : input[i];
    return interrupted ? LS_EIO : LS_OK;
}
ls_result_t ls_storage_sim_erase(void *context, size_t offset, size_t length) {
    ls_storage_sim_t *simulator = (ls_storage_sim_t *)context;
    if (bounds(simulator, offset, length) != LS_OK)
        return LS_EINVAL;
    bool interrupted = fail(simulator);
    size_t erased = interrupted
                        ? (simulator->partial_bytes < length ? simulator->partial_bytes : length)
                        : length;
    ls_memset(simulator->data + offset, 0xff, erased);
    return interrupted ? LS_EIO : LS_OK;
}
ls_result_t ls_storage_sim_sync(void *context) {
    ls_storage_sim_t *simulator = (ls_storage_sim_t *)context;
    return fail(simulator) ? LS_EIO : LS_OK;
}
