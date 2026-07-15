#include "rp_reset.h"
ls_reset_info_t ls_rp_reset_info(void *context) {
    ls_rp_reset_port_t *port = (ls_rp_reset_port_t *)context;
    ls_reset_info_t info = {LS_RESET_UNKNOWN, 0, 0, 0, false, false, false};
    if (!port || !port->reason_register)
        return info;
    info.raw_reason = *port->reason_register;
    if (info.raw_reason & port->watchdog_mask)
        info.reason = LS_RESET_WATCHDOG;
    else if (info.raw_reason & port->forced_mask)
        info.reason = LS_RESET_SOFTWARE;
    else if (info.raw_reason & port->brownout_mask)
        info.reason = LS_RESET_BROWNOUT;
    else if (info.raw_reason & port->power_on_mask)
        info.reason = LS_RESET_POWER_ON;
    return info;
}
