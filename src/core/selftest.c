#include "internal.h"
#include "laststate/selftest.h"
#include "laststate/blackbox.h"

ls_result_t ls_selftest_register(const ls_selftest_case_t *test) {
    if (!test || !test->run || !test->id || !test->name) {
        return LS_EINVAL;
    }
    for (size_t index = 0u; index < ls_runtime.selftest_count; ++index) {
        if (ls_runtime.selftests[index].id == test->id) {
            ls_runtime.selftests[index] = *test;
            return LS_OK;
        }
    }
    if (ls_runtime.selftest_count >= LS_SELFTEST_CAPACITY) {
        return LS_ENOSPACE;
    }
    ls_runtime.selftests[ls_runtime.selftest_count++] = *test;
    return LS_OK;
}

ls_result_t ls_selftest_run(ls_selftest_report_t *report) {
    if (!report) {
        return LS_EINVAL;
    }
    *report = (ls_selftest_report_t){.registered = ls_runtime.selftest_count};
    ls_result_t overall = LS_OK;
    for (size_t index = 0u; index < ls_runtime.selftest_count; ++index) {
        const ls_selftest_case_t *test = &ls_runtime.selftests[index];
        ls_result_t result = test->run(test->context);
        if (result == LS_OK) {
            report->passed++;
            (void)ls_blackbox_record_values(LS_BLACKBOX_STATE, test->id, 0u, 1, 0, 0, 0);
        } else {
            report->failed++;
            overall = LS_EIO;
            if (test->required && index < 32u) {
                report->failed_required_mask |= 1u << index;
            }
            (void)ls_blackbox_record_values(LS_BLACKBOX_STATE, test->id, LS_BLACKBOX_ERROR,
                                            result, test->required ? 1 : 0, 0, 0);
            ls_error_t error = {"selftest", (int32_t)test->id,
                                test->required ? LS_SEVERITY_ERROR : LS_SEVERITY_WARNING,
                                test->name};
            ls_capture_error(&error);
        }
    }
    return overall;
}

void ls_selftest_clear(void) {
    ls_memset(ls_runtime.selftests, 0, sizeof(ls_runtime.selftests));
    ls_runtime.selftest_count = 0u;
}
