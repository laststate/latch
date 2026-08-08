#include "silabs_reset.h"

ls_reset_info_t ls_silabs_reset_info(void *context) {
    return ls_reset_mask_port_info(context);
}

void ls_silabs_reset_clear(ls_silabs_reset_port_t *port) {
    ls_reset_mask_port_clear(port);
}

void ls_silabs_efm32_series0_reset_profile(ls_silabs_reset_port_t *port,
                                           volatile uint32_t *rstcause_register,
                                           volatile uint32_t *command_register,
                                           uint32_t clear_command) {
    if (!port)
        return;
    *port = (ls_silabs_reset_port_t){
        .status_register = rstcause_register,
        .clear_register = command_register,
        .clear_mask = clear_command,
        .masks =
            {
                .power_on_mask = LS_SILABS_EFM32_SERIES0_PORST,
                .pin_mask = LS_SILABS_EFM32_SERIES0_EXTRST,
                .software_mask = LS_SILABS_EFM32_SERIES0_SYSREQRST,
                .watchdog_mask = LS_SILABS_EFM32_SERIES0_WDOGRST,
                .brownout_mask =
                    LS_SILABS_EFM32_SERIES0_BODUNREGRST | LS_SILABS_EFM32_SERIES0_BODREGRST,
                .lockup_mask = LS_SILABS_EFM32_SERIES0_LOCKUPRST,
            },
    };
}

void ls_silabs_series2_reset_profile(ls_silabs_reset_port_t *port,
                                     volatile uint32_t *rstcause_register,
                                     volatile uint32_t *command_register, uint32_t clear_command) {
    if (!port)
        return;
    *port = (ls_silabs_reset_port_t){
        .status_register = rstcause_register,
        .clear_register = command_register,
        .clear_mask = clear_command,
        .masks =
            {
                .power_on_mask = LS_SILABS_SERIES2_POR,
                .pin_mask = LS_SILABS_SERIES2_PIN,
                .software_mask = LS_SILABS_SERIES2_SYSREQ,
                .watchdog_mask = LS_SILABS_SERIES2_WDOG0 | LS_SILABS_SERIES2_WDOG1,
                .brownout_mask = LS_SILABS_SERIES2_DVDDBOD | LS_SILABS_SERIES2_DVDDLEBOD |
                                 LS_SILABS_SERIES2_DECBOD | LS_SILABS_SERIES2_AVDDBOD |
                                 LS_SILABS_SERIES2_IOVDD0BOD,
                .low_power_mask = LS_SILABS_SERIES2_EM4,
                .lockup_mask = LS_SILABS_SERIES2_LOCKUP,
                .security_mask = LS_SILABS_SERIES2_SETAMPER | LS_SILABS_SERIES2_SESYSREQ |
                                 LS_SILABS_SERIES2_SELOCKUP,
            },
    };
}
