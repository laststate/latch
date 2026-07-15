#ifndef LASTSTATE_POLICY_H
#define LASTSTATE_POLICY_H
#include <stdbool.h>
#include <stdint.h>
#include "event.h"
typedef enum {
    LS_PER_SECOND = 1000u,
    LS_PER_MINUTE = 60000u,
    LS_PER_HOUR = 3600000u
} ls_rate_period_t;
typedef struct {
    uint32_t fingerprint, count, first_seen_ms, last_seen_ms;
} ls_dedup_snapshot_t;
ls_result_t ls_sampling_set(const char *domain, float probability);
ls_result_t ls_sampling_set_permyriad(const char *domain, uint16_t permyriad);
ls_result_t ls_rate_limit_set(const char *domain, uint16_t maximum, ls_rate_period_t period);
void ls_policy_reset(void);
size_t ls_dedup_count(void);
ls_result_t ls_dedup_get(size_t index, ls_dedup_snapshot_t *snapshot);
#endif
