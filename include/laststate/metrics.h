#ifndef LASTSTATE_METRICS_H
#define LASTSTATE_METRICS_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "event.h"

typedef enum {
    LS_METRIC_I32 = 1,
    LS_METRIC_U32,
    LS_METRIC_BOOL,
    LS_METRIC_COUNTER
} ls_metric_type_t;
typedef struct {
    char name[LS_METRIC_NAME_MAX];
    uint32_t name_hash;
    ls_metric_type_t type;
    int32_t current, previous, minimum, maximum, average;
    int64_t sum;
    uint32_t count;
} ls_metric_snapshot_t;

void ls_metric_i32(const char *name, int32_t value);
void ls_metric_u32(const char *name, uint32_t value);
void ls_metric_bool(const char *name, bool value);
void ls_metric_increment(const char *name, uint32_t amount);
ls_result_t ls_metric_get(const char *name, ls_metric_snapshot_t *snapshot);
void ls_metrics_reset_window(const char *name);
#endif
