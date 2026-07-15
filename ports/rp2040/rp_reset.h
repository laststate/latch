#ifndef LASTSTATE_RP_RESET_H
#define LASTSTATE_RP_RESET_H
#include <stdint.h>
#include "laststate/event.h"
typedef struct {
    volatile uint32_t *reason_register;
    uint32_t watchdog_mask, forced_mask, brownout_mask, power_on_mask;
} ls_rp_reset_port_t;
ls_reset_info_t ls_rp_reset_info(void *context);
#endif
