#include "internal.h"
ls_result_t ls_span_begin(uint16_t id) {
#if LS_ENABLE_PERFORMANCE
    for (size_t i = 0; i < LS_SPAN_CAPACITY; i++)
        if (!ls_runtime.spans[i].active) {
            ls_runtime.spans[i] = (ls_span_record_t){id, ls_uptime_ms(), 0, true};
            return LS_OK;
        }
    return LS_ENOSPACE;
#else
    (void)id;
    return LS_ENOTSUP;
#endif
}
ls_result_t ls_span_end(uint16_t id) {
    for (size_t i = 0; i < LS_SPAN_CAPACITY; i++)
        if (ls_runtime.spans[i].active && ls_runtime.spans[i].id == id) {
            ls_runtime.spans[i].elapsed_ms = ls_uptime_ms() - ls_runtime.spans[i].started_ms;
            ls_runtime.spans[i].active = false;
            ls_metric_u32("span_last_ms", ls_runtime.spans[i].elapsed_ms);
            return LS_OK;
        }
    return LS_EINVAL;
}
void ls_performance_report(ls_performance_kind_t kind, uint32_t value, uint32_t threshold) {
    if (value < threshold)
        return;
    ls_error_t error = {"performance", (int32_t)kind,
                        kind == LS_PERF_DEADLINE_MISS || kind == LS_PERF_DMA_TIMEOUT
                            ? LS_SEVERITY_ERROR
                            : LS_SEVERITY_WARNING,
                        "performance_threshold"};
    ls_capture_error(&error);
    ls_metric_u32("performance_value", value);
}
