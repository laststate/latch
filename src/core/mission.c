#include "internal.h"
#include "laststate/mission.h"
#include "laststate/blackbox.h"

static uint64_t read_u64_le(const uint8_t bytes[8]) {
    uint64_t value = 0u;
    for (unsigned i = 0u; i < 8u; ++i) {
        value |= (uint64_t)bytes[i] << (i * 8u);
    }
    return value;
}

ls_result_t ls_mission_begin(const char *mission_id, const char *dive_id, const char *node_id) {
    if (!mission_id || !mission_id[0]) {
        return LS_EINVAL;
    }
    ls_memset(&ls_runtime.mission, 0, sizeof(ls_runtime.mission));
    ls_copy_string(ls_runtime.mission.mission_id, sizeof(ls_runtime.mission.mission_id),
                   mission_id);
    ls_copy_string(ls_runtime.mission.dive_id, sizeof(ls_runtime.mission.dive_id), dive_id);
    ls_copy_string(ls_runtime.mission.node_id, sizeof(ls_runtime.mission.node_id), node_id);
    ls_runtime.mission.mission_started_ms = ls_uptime_ms();
    (void)ls_blackbox_record_values(
        LS_BLACKBOX_MISSION, 1u, LS_BLACKBOX_IMPORTANT, (int32_t)ls_hash_string(mission_id),
        (int32_t)ls_hash_string(dive_id), (int32_t)ls_hash_string(node_id), 0);
    return LS_OK;
}

void ls_mission_end(void) {
    (void)ls_blackbox_record_values(LS_BLACKBOX_MISSION, 2u, LS_BLACKBOX_IMPORTANT,
                                    (int32_t)ls_hash_string(ls_runtime.mission.mission_id),
                                    ls_runtime.mission.depth_cm, (int32_t)ls_runtime.mission.phase,
                                    0);
    ls_memset(&ls_runtime.mission, 0, sizeof(ls_runtime.mission));
}

void ls_mission_set_mode(const char *mode, uint32_t phase) {
    ls_copy_string(ls_runtime.mission.vehicle_mode, sizeof(ls_runtime.mission.vehicle_mode), mode);
    ls_runtime.mission.phase = phase;
    (void)ls_blackbox_record_values(LS_BLACKBOX_MISSION, 3u, 0u, (int32_t)ls_hash_string(mode),
                                    (int32_t)phase, ls_runtime.mission.depth_cm, 0);
}

void ls_mission_set_depth_cm(int32_t depth_cm) {
    ls_runtime.mission.depth_cm = depth_cm;
    (void)ls_blackbox_record_values(LS_BLACKBOX_SENSOR, 1u, 0u, depth_cm, 0, 0, 0);
}

ls_result_t ls_mission_get(ls_mission_context_t *context) {
    if (!context) {
        return LS_EINVAL;
    }
    ls_memcpy(context, &ls_runtime.mission, sizeof(*context));
    return LS_OK;
}

uint32_t ls_mission_elapsed_ms(void) {
    return ls_runtime.mission.mission_id[0] ? ls_uptime_ms() - ls_runtime.mission.mission_started_ms
                                            : 0u;
}

ls_result_t ls_incident_begin(uint64_t incident_hi, uint64_t incident_lo) {
    if (incident_hi == 0u && incident_lo == 0u) {
        uint8_t random[16];
        if (ls_security_random(random, sizeof(random)) == LS_OK) {
            incident_lo = read_u64_le(random);
            incident_hi = read_u64_le(random + 8u);
            ls_secure_zero(random, sizeof(random));
        } else {
            uint32_t seed =
                ls_hash_string(ls_runtime.config.identity ? ls_runtime.config.identity->device_id
                                                          : 0) ^
                ls_hash_string(ls_build_id()) ^ ls_boot_count() ^ ls_uptime_ms();
            incident_hi = ((uint64_t)seed << 32) |
                          (uint64_t)ls_event_fingerprint("incident", (int32_t)ls_boot_count(),
                                                         ls_runtime.sequence);
            incident_lo = ((uint64_t)ls_runtime.sequence << 32) |
                          (uint64_t)ls_event_fingerprint("mission", (int32_t)ls_uptime_ms(), seed);
        }
        if (incident_hi == 0u && incident_lo == 0u) {
            incident_lo = 1u;
        }
    }
    ls_runtime.mission.incident_hi = incident_hi;
    ls_runtime.mission.incident_lo = incident_lo;
    ls_runtime.mission.incident_active = true;
    (void)ls_blackbox_record_values(LS_BLACKBOX_MISSION, 4u, LS_BLACKBOX_IMPORTANT,
                                    (int32_t)incident_hi, (int32_t)(incident_hi >> 32),
                                    (int32_t)incident_lo, (int32_t)(incident_lo >> 32));
    return LS_OK;
}

void ls_incident_end(void) {
    ls_runtime.mission.incident_active = false;
    ls_runtime.mission.incident_hi = 0u;
    ls_runtime.mission.incident_lo = 0u;
}
