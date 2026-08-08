#include "reset_reason.h"

static bool ls_reset_mask_matches(uint32_t raw_reason, uint32_t mask) {
    return mask != 0u && (raw_reason & mask) != 0u;
}

ls_reset_info_t ls_reset_reason_from_masks(uint32_t raw_reason,
                                           const ls_reset_reason_masks_t *masks) {
    ls_reset_info_t info = {LS_RESET_UNKNOWN, raw_reason, 0u, 0u, false, false, false};
    if (!masks)
        return info;

    if (ls_reset_mask_matches(raw_reason, masks->brownout_mask))
        info.reason = LS_RESET_BROWNOUT;
    else if (ls_reset_mask_matches(raw_reason, masks->independent_watchdog_mask))
        info.reason = LS_RESET_INDEPENDENT_WATCHDOG;
    else if (ls_reset_mask_matches(raw_reason, masks->window_watchdog_mask))
        info.reason = LS_RESET_WINDOW_WATCHDOG;
    else if (ls_reset_mask_matches(raw_reason, masks->watchdog_mask))
        info.reason = LS_RESET_WATCHDOG;
    else if (ls_reset_mask_matches(raw_reason, masks->lockup_mask))
        info.reason = LS_RESET_LOCKUP;
    else if (ls_reset_mask_matches(raw_reason, masks->security_mask))
        info.reason = LS_RESET_SECURITY;
    else if (ls_reset_mask_matches(raw_reason, masks->clock_failure_mask))
        info.reason = LS_RESET_CLOCK_FAILURE;
    else if (ls_reset_mask_matches(raw_reason, masks->software_mask))
        info.reason = LS_RESET_SOFTWARE;
    else if (ls_reset_mask_matches(raw_reason, masks->pin_mask))
        info.reason = LS_RESET_PIN;
    else if (ls_reset_mask_matches(raw_reason, masks->power_on_mask))
        info.reason = LS_RESET_POWER_ON;
    else if (ls_reset_mask_matches(raw_reason, masks->low_power_mask))
        info.reason = LS_RESET_LOW_POWER_WAKE;
    else if (ls_reset_mask_matches(raw_reason, masks->bootloader_mask))
        info.reason = LS_RESET_BOOTLOADER;
    else if (ls_reset_mask_matches(raw_reason, masks->firmware_update_mask))
        info.reason = LS_RESET_FIRMWARE_UPDATE;
    return info;
}

ls_reset_info_t ls_reset_mask_port_info(void *context) {
    ls_reset_mask_port_t *port = (ls_reset_mask_port_t *)context;
    if (!port || !port->status_register)
        return ls_reset_reason_from_masks(0u, NULL);
    return ls_reset_reason_from_masks(*port->status_register, &port->masks);
}

void ls_reset_mask_port_clear(ls_reset_mask_port_t *port) {
    if (port && port->clear_register && port->clear_mask != 0u)
        *port->clear_register = port->clear_mask;
}
