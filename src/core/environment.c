#include "internal.h"
#include "laststate/environment.h"

void ls_environment_sample(const ls_environment_sample_t *sample) {
    if (!sample) {
        return;
    }
    ls_environment_sample_t value = *sample;
    if (!value.timestamp_ms && ls_runtime.initialized) {
        value.timestamp_ms = ls_uptime_ms();
    }
    ls_runtime.environment.last = value;
    if (ls_runtime.environment.sample_count != UINT32_MAX) {
        ls_runtime.environment.sample_count++;
    }
    if (value.pressure_pa > ls_runtime.environment.maximum_pressure_pa) {
        ls_runtime.environment.maximum_pressure_pa = value.pressure_pa;
    }
    if (value.depth_cm > ls_runtime.environment.maximum_depth_cm) {
        ls_runtime.environment.maximum_depth_cm = value.depth_cm;
    }
    if (value.internal_temperature_c > ls_runtime.environment.maximum_temperature_c) {
        ls_runtime.environment.maximum_temperature_c = value.internal_temperature_c;
    }
    if (value.humidity_permyriad > ls_runtime.environment.maximum_humidity_permyriad) {
        ls_runtime.environment.maximum_humidity_permyriad = value.humidity_permyriad;
    }
    if (value.vibration_mg_rms > ls_runtime.environment.maximum_vibration_mg_rms) {
        ls_runtime.environment.maximum_vibration_mg_rms = value.vibration_mg_rms;
    }
    if (value.flags & (LS_ENV_LEAK_DETECTED | LS_ENV_WATER_INGRESS)) {
        if (ls_runtime.environment.leak_events != UINT32_MAX) {
            ls_runtime.environment.leak_events++;
        }
        ls_blackbox_anomaly_begin(30000u);
    }
    ls_mission_set_depth_cm(value.depth_cm);
    (void)ls_blackbox_record_values(
        LS_BLACKBOX_SENSOR, 0x454eu,
        (uint16_t)((value.flags & (LS_ENV_LEAK_DETECTED | LS_ENV_WATER_INGRESS))
                       ? (LS_BLACKBOX_IMPORTANT | LS_BLACKBOX_ERROR)
                       : 0u),
        value.depth_cm, (int32_t)value.pressure_pa, (int32_t)value.vibration_mg_rms,
        (int32_t)value.flags);
}

ls_result_t ls_environment_get_summary(ls_environment_summary_t *summary) {
    if (!summary) {
        return LS_EINVAL;
    }
    *summary = ls_runtime.environment;
    return LS_OK;
}

bool ls_environment_leak_detected(void) {
    return (ls_runtime.environment.last.flags & (LS_ENV_LEAK_DETECTED | LS_ENV_WATER_INGRESS)) !=
           0u;
}
