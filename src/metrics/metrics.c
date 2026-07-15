#include "../core/internal.h"

static ls_metric_record_t *find_metric(const char *name) {
    uint32_t hash = ls_hash_string(name);
    for (size_t i = 0; i < ls_runtime.metric_count; i++)
        if (ls_runtime.metrics[i].name_hash == hash)
            return &ls_runtime.metrics[i];
    return 0;
}
static void set_metric(const char *name, int32_t value, ls_metric_type_t type) {
#if LS_ENABLE_METRICS
    if (!name)
        return;
    ls_enter_critical();
    ls_metric_record_t *metric = find_metric(name);
    if (!metric && ls_runtime.metric_count < LS_METRIC_CAPACITY) {
        metric = &ls_runtime.metrics[ls_runtime.metric_count++];
        ls_memset(metric, 0, sizeof *metric);
        metric->name_hash = ls_hash_string(name);
        metric->minimum = value;
        metric->maximum = value;
#if LS_STORE_STRINGS
        ls_copy_string(metric->name, sizeof metric->name, name);
#endif
    }
    if (metric) {
        metric->previous = metric->value;
        metric->value = value;
        metric->type = (uint8_t)type;
        if (!metric->count || value < metric->minimum)
            metric->minimum = value;
        if (!metric->count || value > metric->maximum)
            metric->maximum = value;
        metric->sum += value;
        metric->count++;
        metric->window[metric->window_next] = value;
        metric->window_next = (metric->window_next + 1u) % LS_METRIC_WINDOW_SIZE;
        if (metric->window_count < LS_METRIC_WINDOW_SIZE)
            metric->window_count++;
    }
    ls_leave_critical();
#else
    (void)name;
    (void)value;
    (void)type;
#endif
}
void ls_metric_i32(const char *name, int32_t value) {
    set_metric(name, value, LS_METRIC_I32);
}
void ls_metric_u32(const char *name, uint32_t value) {
    set_metric(name, (int32_t)value, LS_METRIC_U32);
}
void ls_metric_bool(const char *name, bool value) {
    set_metric(name, value ? 1 : 0, LS_METRIC_BOOL);
}
void ls_metric_increment(const char *name, uint32_t amount) {
    ls_metric_record_t *metric = find_metric(name);
    set_metric(name, (metric ? metric->value : 0) + (int32_t)amount, LS_METRIC_COUNTER);
}
ls_result_t ls_metric_get(const char *name, ls_metric_snapshot_t *snapshot) {
    if (!name || !snapshot)
        return LS_EINVAL;
    ls_metric_record_t *metric = find_metric(name);
    if (!metric)
        return LS_EINVAL;
    ls_memset(snapshot, 0, sizeof *snapshot);
    snapshot->name_hash = metric->name_hash;
    snapshot->type = (ls_metric_type_t)metric->type;
#if LS_STORE_STRINGS
    ls_copy_string(snapshot->name, sizeof snapshot->name, metric->name);
#endif
    snapshot->current = metric->value;
    snapshot->previous = metric->previous;
    snapshot->minimum = metric->minimum;
    snapshot->maximum = metric->maximum;
    snapshot->sum = metric->sum;
    snapshot->count = metric->count;
    int64_t window_sum = 0;
    for (uint8_t i = 0; i < metric->window_count; i++)
        window_sum += metric->window[i];
    snapshot->average = metric->window_count ? (int32_t)(window_sum / metric->window_count) : 0;
    return LS_OK;
}
void ls_metrics_reset_window(const char *name) {
    ls_metric_record_t *metric = find_metric(name);
    if (metric) {
        metric->window_count = 0;
        metric->window_next = 0;
    }
}
