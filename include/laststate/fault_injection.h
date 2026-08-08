#ifndef LASTSTATE_FAULT_INJECTION_H
#define LASTSTATE_FAULT_INJECTION_H

#include <stdbool.h>
#include <stdint.h>

#include "event.h"

typedef struct {
    uint32_t name_hash;
    uint32_t hits;
    uint32_t trigger_hit;
    ls_result_t result;
    bool armed;
} ls_fault_injection_state_t;

ls_result_t ls_fault_injection_arm(const char *name, uint32_t trigger_hit, ls_result_t result);
void ls_fault_injection_disarm(const char *name);
void ls_fault_injection_clear(void);
ls_result_t ls_fault_injection_hit(const char *name);
ls_result_t ls_fault_injection_get(const char *name, ls_fault_injection_state_t *state);

#endif
