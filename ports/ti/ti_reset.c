#include "ti_reset.h"

static void ls_ti_resc_profile(ls_ti_reset_port_t *port, volatile uint32_t *resc_register) {
    if (!port)
        return;
    *port = (ls_ti_reset_port_t){
        .status_register = resc_register,
        .masks =
            {
                .power_on_mask = LS_TI_RESC_POR,
                .pin_mask = LS_TI_RESC_EXT,
                .software_mask = LS_TI_RESC_SW,
                .watchdog_mask = LS_TI_RESC_WDT0 | LS_TI_RESC_WDT1,
                .brownout_mask = LS_TI_RESC_BOR,
                .low_power_mask = LS_TI_RESC_HIB,
                .clock_failure_mask = LS_TI_RESC_MOSCFAIL,
            },
    };
}

ls_reset_info_t ls_ti_reset_info(void *context) {
    return ls_reset_mask_port_info(context);
}

void ls_ti_reset_clear(ls_ti_reset_port_t *port) {
    ls_reset_mask_port_clear(port);
}

void ls_ti_tiva_reset_profile(ls_ti_reset_port_t *port, volatile uint32_t *resc_register) {
    ls_ti_resc_profile(port, resc_register);
}

void ls_ti_msp432e4_reset_profile(ls_ti_reset_port_t *port, volatile uint32_t *resc_register) {
    ls_ti_resc_profile(port, resc_register);
}
