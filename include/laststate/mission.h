#ifndef LASTSTATE_MISSION_H
#define LASTSTATE_MISSION_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "event.h"

typedef struct {
    char mission_id[LS_MISSION_ID_MAX];
    char dive_id[LS_DIVE_ID_MAX];
    char node_id[LS_NODE_ID_MAX];
    char vehicle_mode[LS_VEHICLE_MODE_MAX];
    uint32_t mission_started_ms;
    uint32_t phase;
    int32_t depth_cm;
    uint64_t incident_hi;
    uint64_t incident_lo;
    bool incident_active;
} ls_mission_context_t;

ls_result_t ls_mission_begin(const char *mission_id, const char *dive_id, const char *node_id);
void ls_mission_end(void);
void ls_mission_set_mode(const char *mode, uint32_t phase);
void ls_mission_set_depth_cm(int32_t depth_cm);
ls_result_t ls_mission_get(ls_mission_context_t *context);
uint32_t ls_mission_elapsed_ms(void);
/* Starts a cross-node incident correlation ID. Supplying both halves as zero
 * requests generation from the configured CSPRNG, with a deterministic
 * diagnostic fallback only when no random provider is available. */
ls_result_t ls_incident_begin(uint64_t incident_hi, uint64_t incident_lo);
void ls_incident_end(void);

#endif
