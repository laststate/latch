#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"
#include "../src/core/internal.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "memory capture failed: %s:%d\n", #x, __LINE__); return 1; } } while (0)

typedef struct {
    unsigned memory_fields;
    unsigned stack_fields;
    uint16_t memory_length;
    uint8_t memory_value[LS_DUMP_REGION_MAX_BYTES + 48u];
} visit_state_t;

static ls_result_t visit(void *context, uint16_t type, const uint8_t *value, uint16_t length) {
    visit_state_t *state = (visit_state_t *)context;
    if (type == LS_TLV_MEMORY) {
        ++state->memory_fields;
        state->memory_length = length;
        size_t n = length < sizeof state->memory_value ? length : sizeof state->memory_value;
        memcpy(state->memory_value, value, n);
    } else if (type == LS_TLV_STACK) {
        ++state->stack_fields;
    }
    return LS_OK;
}

static int encode_for_mode(ls_redaction_mode_t mode, bool redact, bool safe, visit_state_t *state,
                           ls_envelope_info_t *info) {
    static const ls_identity_t identity = {.project_id="mem", .device_id="host", .firmware_build_id="memcap01"};
    ls_config_t config = {.identity=&identity};
    CHECK(ls_init(&config) == LS_OK);
    static uint8_t region[LS_DUMP_REGION_MAX_BYTES + 64u];
    for (size_t i=0; i<sizeof region; ++i) region[i]=(uint8_t)(i+1u);
    if (safe) {
        CHECK(ls_dump_region_register("region", region, sizeof region, LS_DUMP_SAFE) == LS_OK);
    } else {
        /* Exercise the defensive skip of an unsafe descriptor that may originate in old retained state. */
        ls_runtime.dump_regions[0] = (ls_dump_region_t){"unsafe", region, sizeof region, 0u};
        ls_runtime.dump_region_count = 1u;
    }
    if (redact)
        CHECK(ls_memory_redact(region, sizeof region, mode) == LS_OK);
    ls_event_t event = {.type=LS_EVENT_COREDUMP, .priority=LS_PRIORITY_ERROR, .timestamp_ms=1u,
                        .domain="memory", .severity=LS_SEVERITY_ERROR, .message="capture",
                        .capture_level=LS_CAPTURE_SELECTIVE};
    uint8_t out[LS_MAX_EVENT_SIZE]; size_t length=0u;
    CHECK(ls_envelope_encode(&event, out, sizeof out, &length) == LS_OK);
    CHECK(ls_envelope_validate(out, length, info) == LS_OK);
    memset(state, 0, sizeof *state);
    CHECK(ls_envelope_visit(out, length, visit, state) == LS_OK);
    return 0;
}

int main(void) {
    visit_state_t state; ls_envelope_info_t info;

    CHECK(encode_for_mode(LS_REDACT_ZERO, false, true, &state, &info) == 0);
    CHECK(state.memory_fields == 1u);
    CHECK(ls_envelope_is_truncated(&info)); /* region is intentionally larger than configured maximum */

    CHECK(encode_for_mode(LS_REDACT_HASH, true, true, &state, &info) == 0);
    CHECK(state.memory_fields == 1u && state.memory_length == 20u); /* 16-byte metadata + CRC32 */

    CHECK(encode_for_mode(LS_REDACT_ZERO, true, true, &state, &info) == 0);
    CHECK(state.memory_fields == 1u && state.memory_length > 16u);
    for (size_t i=16u; i<state.memory_length; ++i) CHECK(state.memory_value[i] == 0u);

    CHECK(encode_for_mode(LS_REDACT_EXCLUDE, true, true, &state, &info) == 0);
    CHECK(state.memory_fields == 0u && ls_envelope_is_truncated(&info));

    CHECK(encode_for_mode(LS_REDACT_ZERO, false, false, &state, &info) == 0);
    CHECK(state.memory_fields == 0u);

    /* Bounded stack snapshot with PSP and MSP selection. */
    static const ls_identity_t identity = {.project_id="stack", .device_id="host", .firmware_build_id="stack001"};
    ls_config_t config = {.identity=&identity};
    CHECK(ls_init(&config) == LS_OK);
    uint8_t stack[LS_STACK_SNAPSHOT_MAX + 32u];
    memset(stack, 0x5a, sizeof stack);
    CHECK(ls_stack_bounds_set(stack, stack + sizeof stack) == LS_OK);
    ls_arch_context_t cpu = {.architecture=LS_ARCH_CORTEX_M, .msp=(uint32_t)(uintptr_t)(stack+8u), .psp=(uint32_t)(uintptr_t)(stack+16u)};
    /* On 64-bit hosts ls_arch_context_t has 32-bit MCU SPs; directly set runtime bounds into the same low-width domain
       only if the host pointer can be represented. The positive path is otherwise physically exercised by Cortex-M HIL. */
    if ((uintptr_t)stack <= UINT32_MAX) {
        ls_event_t event = {.type=LS_EVENT_CRASH,.priority=LS_PRIORITY_CRITICAL,.timestamp_ms=2u,
                            .domain="stack",.severity=LS_SEVERITY_FATAL,.message="snapshot",
                            .cpu=&cpu,.capture_level=LS_CAPTURE_STACK};
        uint8_t out[LS_MAX_EVENT_SIZE]; size_t length=0u;
        CHECK(ls_envelope_encode(&event,out,sizeof out,&length)==LS_OK);
        memset(&state,0,sizeof state);
        CHECK(ls_envelope_visit(out,length,visit,&state)==LS_OK);
        CHECK(state.stack_fields == 1u);
    }

    return 0;
}
