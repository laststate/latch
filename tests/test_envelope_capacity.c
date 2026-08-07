#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#include "../src/core/internal.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"envelope capacity failed: %s:%d\n",#x,__LINE__); return 1; } } while(0)

static int sweep_event(ls_event_t *event) {
    uint8_t out[LS_MAX_EVENT_SIZE];
    bool saw_space = false, saw_ok = false, saw_truncated = false;
    for (size_t capacity = LS_LEP_HEADER_SIZE + 4u; capacity <= sizeof out; ++capacity) {
        size_t length = 0u;
        ls_result_t result = ls_envelope_encode(event, out, capacity, &length);
        if (result == LS_ENOSPACE) {
            saw_space = true;
            continue;
        }
        CHECK(result == LS_OK);
        CHECK(length <= capacity);
        saw_ok = true;
        ls_envelope_info_t info;
        CHECK(ls_envelope_validate(out, length, &info) == LS_OK);
        if (ls_envelope_is_truncated(&info)) saw_truncated = true;
    }
    CHECK(saw_space && saw_ok && saw_truncated);
    return 0;
}

int main(void) {
    static const ls_identity_t id={.project_id="capacity-project-with-a-long-name",.device_id="capacity-device-with-a-long-name",
                                   .firmware_version="2026.08-commercial-auv",.firmware_build_id="capcov01"};
    ls_config_t cfg={.identity=&id,.architecture=LS_ARCH_CORTEX_M};
    CHECK(ls_init(&cfg)==LS_OK);

    ls_breadcrumb_kv_t kv[LS_BREADCRUMB_KV_MAX];
    memset(kv,0,sizeof kv);
    for (size_t i=0;i<LS_BREADCRUMB_KV_MAX;++i) { kv[i].key_id=(uint16_t)(i+1u); kv[i].type=LS_VALUE_U32; kv[i].value.u32=(uint32_t)i; }
    ls_breadcrumb_t crumb={.category="navigation-and-control",.level=LS_SEVERITY_ERROR,.message_id=42u,
                           .message="long breadcrumb payload for truncation coverage",.values=kv,.value_count=LS_BREADCRUMB_KV_MAX};
    ls_breadcrumb_event(&crumb);
    for (unsigned i=0;i<LS_METRIC_WINDOW_SIZE;++i) ls_metric_i32("motor-current-window",(int32_t)i-3);
    ls_power_sample_t power={.timestamp_ms=123u,.vdd_mv=3300u,.battery_mv=14800u,.current_ma=1200,.temperature_c=37,
                             .charger_status=2u,.power_flags=3u};
    ls_power_sample(&power);
    ls_health_register("control", 100u); ls_health_touch("control"); ls_watchdog_checkpoint(7u); ls_watchdog_fed();

    ls_arch_context_t cpu={.architecture=LS_ARCH_CORTEX_M,.fault=LS_FAULT_HARD,.has_fpu=true,.fpu_lazy=true,
                           .lr=1u,.pc=2u,.msp=3u,.psp=4u,.cfsr=5u,.hfsr=6u,.fault_address=7u,.signal_number=8};
    for (unsigned i=0;i<32u;++i) cpu.registers[i]=i+100u;
    for (unsigned i=0;i<16u;++i) cpu.s[i]=i+200u;
    ls_assert_info_t assertion={.expression="very_long_assert_expression_that_exercises_string_bounding_and_optional_payload_truncation",
                                .file="src/navigation/controller_with_a_deliberately_long_filename_for_coverage.c",
                                .message="controller invariant failed while validating a bounded commercial AUV diagnostic path",
                                .line=321};
    ls_peripheral_fault_t peripheral={.domain=LS_PERIPHERAL_CAN,.fault=LS_CAN_BUS_OFF,.instance=2u,.status=3u,
                                      .address=4u,.reg=5u,.timeout_ms=6u,.auxiliary={7u,8u,9u,10u}};
    ls_log_info_t log={.message_id=9u,.format_id=10u,.argument_count=LS_LOG_ARG_MAX};
    for (unsigned i=0;i<LS_LOG_ARG_MAX;++i) log.arguments[i]=1000u+i;
    ls_event_t event={.type=LS_EVENT_CRASH,.priority=LS_PRIORITY_CRITICAL,.timestamp_ms=999u,.fingerprint=0x1234u,
                      .domain="navigation",.code=-55,.severity=LS_SEVERITY_FATAL,.message="controller fault",
                      .cpu=&cpu,.assertion=&assertion,.peripheral=&peripheral,.log=&log,.capture_level=LS_CAPTURE_METADATA,
                      .repeat_count=3u,.first_seen_ms=10u,.last_seen_ms=999u};
    CHECK(sweep_event(&event)==0);

    /* RV64 has an additive full-width TLV. Sweep both with and without the sidecar so receivers see an explicit truncated descriptor. */
    ls_riscv64_context_t wide={0};
    for (unsigned i=0;i<32u;++i) wide.x[i]=UINT64_C(0x100000000)+i;
    wide.mstatus=UINT64_C(0x200000001); wide.mcause=UINT64_C(0x200000002); wide.mtval=UINT64_C(0x200000003); wide.mepc=UINT64_C(0x200000004);
    cpu.architecture=LS_ARCH_RISCV64; cpu.has_fpu=false; cpu.fpu_lazy=false;
    event.cpu=&cpu; event.riscv64=&wide; event.assertion=NULL; event.peripheral=NULL; event.log=NULL;
    CHECK(sweep_event(&event)==0);
    event.riscv64=NULL;
    CHECK(sweep_event(&event)==0);
    return 0;
}
