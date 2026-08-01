#include "laststate/latch.h"

#if LS_CONSTRAINED_PROFILE != 1
#error "the constrained profile test must compile with LS_CONSTRAINED_PROFILE=1"
#endif
#if LS_ENABLE_BREADCRUMBS || LS_ENABLE_METRICS || LS_ENABLE_LOGS || LS_ENABLE_POWER_SAMPLES || \
    LS_ENABLE_PERFORMANCE || LS_ENABLE_DUMPS || LS_ENABLE_ASSERTS || LS_ENABLE_STACK_SNAPSHOT || \
    LS_STORE_STRINGS
#error "the constrained profile must disable optional retained observability"
#endif
#if LS_MAX_EVENT_SIZE != 512u || LS_SPOOL_MAX_RECORDS != 1u || LS_MAX_TRANSPORTS != 1u
#error "the constrained profile's bounded defaults changed unexpectedly"
#endif

int main(void) {
    static const ls_identity_t identity = {
        .project_id = "small", .device_id = "board", .firmware_build_id = "profile01"};
    ls_config_t config = {.identity = &identity};
    if (ls_storage_required_size() > 1024u)
        return 1;
    if (ls_init(&config) != LS_OK || ls_boot() != LS_OK)
        return 2;
    ls_capture_message("bounded cooperative event", LS_SEVERITY_ERROR);
    return 0;
}
