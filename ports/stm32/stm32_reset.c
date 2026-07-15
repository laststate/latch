#include "stm32_reset.h"
ls_reset_info_t ls_stm32_reset_info(void *context) {
    ls_stm32_reset_port_t *port = (ls_stm32_reset_port_t *)context;
    ls_reset_info_t info = {LS_RESET_UNKNOWN, 0, 0, 0, false, false, false};
    if (!port || !port->status_register)
        return info;
    uint32_t raw = *port->status_register;
    info.raw_reason = raw;
    if (raw & port->brownout_mask)
        info.reason = LS_RESET_BROWNOUT;
    else if (raw & port->watchdog_mask)
        info.reason = LS_RESET_INDEPENDENT_WATCHDOG;
    else if (raw & port->window_watchdog_mask)
        info.reason = LS_RESET_WINDOW_WATCHDOG;
    else if (raw & port->software_mask)
        info.reason = LS_RESET_SOFTWARE;
    else if (raw & port->pin_mask)
        info.reason = LS_RESET_PIN;
    else if (raw & port->power_on_mask)
        info.reason = LS_RESET_POWER_ON;
    else if (raw & port->low_power_mask)
        info.reason = LS_RESET_LOW_POWER_WAKE;
    else if (raw & port->clock_failure_mask)
        info.reason = LS_RESET_CLOCK_FAILURE;
    return info;
}
void ls_stm32_reset_clear(ls_stm32_reset_port_t *port) {
    if (port && port->clear_register)
        *port->clear_register = port->clear_mask;
}
void ls_stm32_reset_profile_f4(ls_stm32_reset_port_t *port, volatile uint32_t *rcc_csr) {
    if (!port)
        return;
    *port = (ls_stm32_reset_port_t){rcc_csr,  rcc_csr,  1u << 24, 1u << 27, 1u << 26, 1u << 28,
                                    1u << 29, 1u << 30, 1u << 25, 1u << 31, 0u};
}
