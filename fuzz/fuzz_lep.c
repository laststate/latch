#include <stddef.h>
#include <stdint.h>
#include "laststate/latch.h"
static ls_result_t visit(void *context, uint16_t type, const uint8_t *value, uint16_t length) {
    (void)context;
    (void)type;
    (void)value;
    (void)length;
    return LS_OK;
}
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ls_envelope_info_t info;
    if (ls_envelope_validate(data, size, &info) == LS_OK)
        (void)ls_envelope_visit(data, size, visit, 0);
    return 0;
}
