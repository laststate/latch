#ifndef LASTSTATE_TI_RESET_H
#define LASTSTATE_TI_RESET_H

#include "reset_reason.h"

/*
 * TM4C/Tiva and MSP432E4 devices expose their reset causes through the
 * SYSCTL RESC layout below.  No DriverLib dependency is needed to use it.
 * Other TI families can use ls_ti_reset_port_t with their own device-header
 * masks.
 */
typedef ls_reset_mask_port_t ls_ti_reset_port_t;

enum {
    LS_TI_RESC_EXT = 1u << 0,
    LS_TI_RESC_POR = 1u << 1,
    LS_TI_RESC_BOR = 1u << 2,
    LS_TI_RESC_WDT0 = 1u << 3,
    LS_TI_RESC_SW = 1u << 4,
    LS_TI_RESC_WDT1 = 1u << 5,
    LS_TI_RESC_HIB = 1u << 6,
    LS_TI_RESC_HSSR = 1u << 12,
    LS_TI_RESC_MOSCFAIL = 1u << 16
};

ls_reset_info_t ls_ti_reset_info(void *context);
void ls_ti_reset_clear(ls_ti_reset_port_t *port);
void ls_ti_tiva_reset_profile(ls_ti_reset_port_t *port, volatile uint32_t *resc_register);
void ls_ti_msp432e4_reset_profile(ls_ti_reset_port_t *port, volatile uint32_t *resc_register);

#endif
