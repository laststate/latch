#include "../core/internal.h"

void ls_breadcrumb(const char *message) {
    ls_breadcrumb_t event = {0, LS_SEVERITY_INFO, 0, message, 0, 0};
    ls_breadcrumb_event(&event);
}
void ls_breadcrumb_set_min_level(ls_severity_t level) {
    ls_runtime.breadcrumb_min_level = level;
}
void ls_breadcrumb_set_policy(ls_breadcrumb_policy_t policy) {
    ls_runtime.breadcrumb_policy = policy;
}
void ls_breadcrumb_event(const ls_breadcrumb_t *event) {
#if LS_ENABLE_BREADCRUMBS
    if (!event || !ls_runtime.initialized || event->level < LS_COMPILED_MIN_LEVEL ||
        event->level < ls_runtime.breadcrumb_min_level)
        return;
    ls_enter_critical();
    if (ls_runtime.breadcrumb_count == LS_BREADCRUMB_CAPACITY &&
        ls_runtime.breadcrumb_policy == LS_BREADCRUMB_DROP_NEWEST) {
        ls_leave_critical();
        return;
    }
    if (ls_runtime.breadcrumb_count == LS_BREADCRUMB_CAPACITY &&
        ls_runtime.breadcrumb_policy == LS_BREADCRUMB_KEEP_ERRORS &&
        event->level < LS_SEVERITY_ERROR) {
        ls_leave_critical();
        return;
    }
    ls_breadcrumb_record_t *record = &ls_runtime.breadcrumbs[ls_runtime.breadcrumb_next];
    ls_memset(record, 0, sizeof *record);
    record->at_ms = ls_uptime_ms();
    record->message_id = event->message_id;
    record->severity = (uint8_t)event->level;
    record->category_hash = ls_hash_string(event->category);
    record->message_hash = event->message_id ? event->message_id : ls_hash_string(event->message);
#if LS_STORE_STRINGS
    ls_copy_string(record->category, sizeof record->category, event->category);
    ls_copy_string(record->message, sizeof record->message, event->message);
#endif
    record->value_count =
        event->value_count > LS_BREADCRUMB_KV_MAX ? LS_BREADCRUMB_KV_MAX : event->value_count;
    if (event->values && record->value_count)
        ls_memcpy(record->values, event->values, record->value_count * sizeof(record->values[0]));
    ls_runtime.breadcrumb_next = (ls_runtime.breadcrumb_next + 1u) % LS_BREADCRUMB_CAPACITY;
    if (ls_runtime.breadcrumb_count < LS_BREADCRUMB_CAPACITY)
        ls_runtime.breadcrumb_count++;
    ls_leave_critical();
#else
    (void)event;
#endif
}
