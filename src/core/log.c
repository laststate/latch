#include "internal.h"
void ls_log_set_stream(ls_log_stream_fn callback, void *context) {
    ls_runtime.log_stream = callback;
    ls_runtime.log_stream_context = context;
}
void ls_log_set_min_level(ls_severity_t level) {
    ls_runtime.log_min_level = level;
}
void ls_log_binary(ls_severity_t level, uint16_t message_id, uint16_t format_id,
                   const uint32_t *arguments, uint8_t argument_count) {
#if LS_ENABLE_LOGS
    if (level < LS_COMPILED_MIN_LEVEL || level < ls_runtime.log_min_level)
        return;
    ls_log_info_t log;
    ls_memset(&log, 0, sizeof log);
    log.message_id = message_id;
    log.format_id = format_id;
    log.argument_count = argument_count > LS_LOG_ARG_MAX ? LS_LOG_ARG_MAX : argument_count;
    if (arguments && log.argument_count)
        ls_memcpy(log.arguments, arguments, log.argument_count * sizeof(uint32_t));
    if (ls_runtime.log_stream) {
        uint8_t compact[4 + LS_LOG_ARG_MAX * 4];
        ls_writer_t writer = {compact, sizeof compact, 0};
        (void)ls_writer_u8(&writer, (uint8_t)level);
        (void)ls_writer_u16(&writer, message_id);
        (void)ls_writer_u8(&writer, log.argument_count);
        for (uint8_t i = 0; i < log.argument_count; i++)
            (void)ls_writer_u32(&writer, log.arguments[i]);
        ls_runtime.log_stream(ls_runtime.log_stream_context, compact, writer.length);
    }
    ls_event_t event = {.type = LS_EVENT_LOG,
                        .priority =
                            level >= LS_SEVERITY_ERROR ? LS_PRIORITY_ERROR : LS_PRIORITY_DIAGNOSTIC,
                        .timestamp_ms = ls_uptime_ms(),
                        .domain = "log",
                        .code = message_id,
                        .severity = level,
                        .log = &log,
                        .capture_level = LS_CAPTURE_METADATA};
    (void)ls_capture_event(&event);
#else
    (void)level;
    (void)message_id;
    (void)format_id;
    (void)arguments;
    (void)argument_count;
#endif
}
