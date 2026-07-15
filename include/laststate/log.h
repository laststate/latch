#ifndef LASTSTATE_LOG_H
#define LASTSTATE_LOG_H

#include <stddef.h>
#include <stdint.h>
#include "config.h"
#include "event.h"
typedef void (*ls_log_stream_fn)(void *context, const uint8_t *record, size_t length);
void ls_log_binary(ls_severity_t level, uint16_t message_id, uint16_t format_id,
                   const uint32_t *arguments, uint8_t argument_count);
void ls_log_set_stream(ls_log_stream_fn callback, void *context);
void ls_log_set_min_level(ls_severity_t level);
#define LS_LOG_INFO(id) ls_log_binary(LS_SEVERITY_INFO, (id), 0, 0, 0)
#define LS_LOG_ERROR(id, args, count) ls_log_binary(LS_SEVERITY_ERROR, (id), 0, (args), (count))
#endif
