#ifndef LASTSTATE_SELFTEST_H
#define LASTSTATE_SELFTEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "event.h"

typedef ls_result_t (*ls_selftest_fn)(void *context);

typedef struct {
    uint16_t id;
    const char *name;
    ls_selftest_fn run;
    void *context;
    bool required;
} ls_selftest_case_t;

typedef struct {
    size_t registered;
    size_t passed;
    size_t failed;
    uint32_t failed_required_mask;
} ls_selftest_report_t;

ls_result_t ls_selftest_register(const ls_selftest_case_t *test);
ls_result_t ls_selftest_run(ls_selftest_report_t *report);
void ls_selftest_clear(void);

#endif
