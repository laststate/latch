#include "internal.h"
#include "laststate/time_sync.h"

ls_result_t ls_time_sync_set(ls_time_source_t source, uint64_t utc_ms, uint32_t uncertainty_ms) {
    if (source <= LS_TIME_SOURCE_NONE || source > LS_TIME_SOURCE_HOST || utc_ms == 0u) {
        return LS_EINVAL;
    }
    ls_runtime.time_sync.source = source;
    ls_runtime.time_sync.utc_ms_at_sync = utc_ms;
    ls_runtime.time_sync.monotonic_ms_at_sync = ls_uptime_ms();
    ls_runtime.time_sync.uncertainty_ms = uncertainty_ms;
    if (ls_runtime.time_sync.generation != UINT32_MAX) {
        ls_runtime.time_sync.generation++;
    }
    ls_runtime.time_sync.synchronized = true;
    return LS_OK;
}

void ls_time_sync_clear(void) {
    uint32_t generation = ls_runtime.time_sync.generation;
    ls_memset(&ls_runtime.time_sync, 0, sizeof(ls_runtime.time_sync));
    ls_runtime.time_sync.generation = generation;
}

ls_time_sync_state_t ls_time_sync_get(void) {
    return ls_runtime.time_sync;
}

ls_result_t ls_time_utc_ms(uint64_t *utc_ms) {
    if (!utc_ms) {
        return LS_EINVAL;
    }
    if (!ls_runtime.time_sync.synchronized) {
        return LS_EAGAIN;
    }
    uint32_t delta = ls_uptime_ms() - ls_runtime.time_sync.monotonic_ms_at_sync;
    if (UINT64_MAX - ls_runtime.time_sync.utc_ms_at_sync < delta) {
        return LS_EOVERFLOW;
    }
    *utc_ms = ls_runtime.time_sync.utc_ms_at_sync + delta;
    return LS_OK;
}
