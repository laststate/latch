#ifndef LASTSTATE_BREADCRUMB_H
#define LASTSTATE_BREADCRUMB_H

#include "config.h"
#include "event.h"

typedef enum { LS_VALUE_I32, LS_VALUE_U32, LS_VALUE_BOOL } ls_value_type_t;
typedef struct {
    uint16_t key_id;
    ls_value_type_t type;
    union {
        int32_t i32;
        uint32_t u32;
        bool boolean;
    } value;
} ls_breadcrumb_kv_t;
typedef struct {
    const char *category;
    ls_severity_t level;
    uint16_t message_id;
    const char *message;
    const ls_breadcrumb_kv_t *values;
    uint8_t value_count;
} ls_breadcrumb_t;
typedef enum {
    LS_BREADCRUMB_DROP_OLDEST,
    LS_BREADCRUMB_DROP_NEWEST,
    LS_BREADCRUMB_KEEP_ERRORS
} ls_breadcrumb_policy_t;

void ls_breadcrumb(const char *message);
void ls_breadcrumb_event(const ls_breadcrumb_t *event);
void ls_breadcrumb_set_min_level(ls_severity_t level);
void ls_breadcrumb_set_policy(ls_breadcrumb_policy_t policy);
#endif
