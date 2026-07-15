#include "internal.h"
void ls_assert_set_policy(ls_assert_policy_t policy, ls_assert_callback_t callback) {
    ls_runtime.assert_policy = policy;
    ls_runtime.assert_callback = callback;
}
void ls_assert_failed(const char *expression, const char *file, int line, const char *message) {
    ls_assert_info_t assertion = {expression, file, message, line};
    ls_event_t event = {.type = LS_EVENT_ERROR,
                        .priority = LS_PRIORITY_CRITICAL,
                        .timestamp_ms = ls_uptime_ms(),
                        .domain = "assert",
                        .code = line,
                        .severity = LS_SEVERITY_FATAL,
                        .message = message ? message : expression,
                        .assertion = &assertion,
                        .capture_level = LS_CAPTURE_SNAPSHOT};
    (void)ls_capture_event(&event);
    switch (ls_runtime.assert_policy) {
    case LS_ASSERT_RESET:
        ls_reset_mark_expected(false);
        if (ls_runtime.config.reset)
            ls_runtime.config.reset(ls_runtime.config.reset_context);
        break;
    case LS_ASSERT_HALT:
        for (;;) {
        }
    case LS_ASSERT_BREAKPOINT:
#if defined(_MSC_VER)
        __debugbreak();
#elif defined(__GNUC__) || defined(__clang__)
        __builtin_trap();
#endif
        break;
    case LS_ASSERT_CALLBACK:
        if (ls_runtime.assert_callback)
            ls_runtime.assert_callback(expression, file, line);
        break;
    case LS_ASSERT_CONTINUE:
    default:
        break;
    }
}
