#ifndef LASTSTATE_PORT_RESET_REASON_H
#define LASTSTATE_PORT_RESET_REASON_H

#include <stdint.h>

#include "laststate/event.h"

/*
 * Vendor reset status registers often expose several sticky causes at once.
 * These masks describe how one such register maps onto Latch's portable
 * reset vocabulary.  The first matching cause is deliberately deterministic:
 * electrical and watchdog failures take precedence over a following software
 * reset request recorded in the same sticky register.
 */
typedef struct {
    uint32_t power_on_mask;
    uint32_t pin_mask;
    uint32_t software_mask;
    uint32_t watchdog_mask;
    uint32_t independent_watchdog_mask;
    uint32_t window_watchdog_mask;
    uint32_t brownout_mask;
    uint32_t low_power_mask;
    uint32_t lockup_mask;
    uint32_t security_mask;
    uint32_t bootloader_mask;
    uint32_t firmware_update_mask;
    uint32_t clock_failure_mask;
} ls_reset_reason_masks_t;

/* Normalize a raw status word without reading or changing hardware. */
ls_reset_info_t ls_reset_reason_from_masks(uint32_t raw_reason,
                                           const ls_reset_reason_masks_t *masks);

/*
 * A portable adapter for the common one-word reset-status register shape.
 * clear_mask is written exactly as supplied when ls_reset_mask_port_clear()
 * is called.  Use a NULL clear_register when a device needs a vendor HAL
 * sequence instead of a single write-to-clear operation.
 */
typedef struct {
    volatile uint32_t *status_register;
    volatile uint32_t *clear_register;
    uint32_t clear_mask;
    ls_reset_reason_masks_t masks;
} ls_reset_mask_port_t;

ls_reset_info_t ls_reset_mask_port_info(void *context);
void ls_reset_mask_port_clear(ls_reset_mask_port_t *port);

#endif
