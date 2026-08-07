#ifndef LASTSTATE_TIME_SYNC_H
#define LASTSTATE_TIME_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "event.h"

typedef enum {
    LS_TIME_SOURCE_NONE = 0,
    LS_TIME_SOURCE_RTC,
    LS_TIME_SOURCE_GNSS,
    LS_TIME_SOURCE_NTP,
    LS_TIME_SOURCE_PTP,
    LS_TIME_SOURCE_HOST
} ls_time_source_t;

typedef struct {
    ls_time_source_t source;
    uint64_t utc_ms_at_sync;
    uint32_t monotonic_ms_at_sync;
    uint32_t uncertainty_ms;
    uint32_t generation;
    bool synchronized;
} ls_time_sync_state_t;

ls_result_t ls_time_sync_set(ls_time_source_t source, uint64_t utc_ms, uint32_t uncertainty_ms);
void ls_time_sync_clear(void);
ls_time_sync_state_t ls_time_sync_get(void);
ls_result_t ls_time_utc_ms(uint64_t *utc_ms);

#endif
