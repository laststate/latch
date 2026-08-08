#include "nxp_reset.h"

ls_reset_info_t ls_nxp_reset_info(void *context) {
    return ls_reset_mask_port_info(context);
}

void ls_nxp_reset_clear(ls_nxp_reset_port_t *port) {
    ls_reset_mask_port_clear(port);
}

void ls_nxp_reset_port_init(ls_nxp_reset_port_t *port, volatile uint32_t *status_register,
                            volatile uint32_t *clear_register, uint32_t clear_mask,
                            ls_reset_reason_masks_t masks) {
    if (!port)
        return;
    *port = (ls_nxp_reset_port_t){
        .status_register = status_register,
        .clear_register = clear_register,
        .clear_mask = clear_mask,
        .masks = masks,
    };
}

void ls_nxp_imxrt105x_reset_profile(ls_nxp_reset_port_t *port, volatile uint32_t *srsr_register) {
    if (!port)
        return;
    *port = (ls_nxp_reset_port_t){
        .status_register = srsr_register,
        .clear_register = srsr_register,
        .clear_mask = LS_NXP_IMXRT105X_SRSR_IPP_RESET_B | LS_NXP_IMXRT105X_SRSR_LOCKUP_SYSRESETREQ |
                      LS_NXP_IMXRT105X_SRSR_CSU_RESET_B | LS_NXP_IMXRT105X_SRSR_IPP_USER_RESET_B |
                      LS_NXP_IMXRT105X_SRSR_WDOG_RST_B | LS_NXP_IMXRT105X_SRSR_JTAG_RST_B |
                      LS_NXP_IMXRT105X_SRSR_JTAG_SW_RST | LS_NXP_IMXRT105X_SRSR_WDOG3_RST_B,
        .masks =
            {
                .pin_mask = LS_NXP_IMXRT105X_SRSR_IPP_RESET_B |
                            LS_NXP_IMXRT105X_SRSR_IPP_USER_RESET_B |
                            LS_NXP_IMXRT105X_SRSR_JTAG_RST_B,
                .software_mask = LS_NXP_IMXRT105X_SRSR_JTAG_SW_RST,
                .watchdog_mask =
                    LS_NXP_IMXRT105X_SRSR_WDOG_RST_B | LS_NXP_IMXRT105X_SRSR_WDOG3_RST_B,
                .security_mask = LS_NXP_IMXRT105X_SRSR_CSU_RESET_B,
            },
    };
}

void ls_nxp_kinetis_reset_profile(ls_nxp_kinetis_reset_port_t *port,
                                  volatile uint8_t *srs0_register,
                                  volatile uint8_t *srs1_register) {
    if (!port)
        return;
    *port = (ls_nxp_kinetis_reset_port_t){
        .srs0_register = srs0_register,
        .srs1_register = srs1_register,
    };
}

ls_reset_info_t ls_nxp_kinetis_reset_info(void *context) {
    ls_nxp_kinetis_reset_port_t *port = (ls_nxp_kinetis_reset_port_t *)context;
    static const ls_reset_reason_masks_t masks = {
        .power_on_mask = LS_NXP_KINETIS_SRS0_POR,
        .pin_mask = LS_NXP_KINETIS_SRS0_PIN,
        .software_mask = (uint32_t)LS_NXP_KINETIS_SRS1_SW << 8u,
        .watchdog_mask = LS_NXP_KINETIS_SRS0_WDOG,
        .brownout_mask = LS_NXP_KINETIS_SRS0_LVD,
        .lockup_mask = (uint32_t)LS_NXP_KINETIS_SRS1_LOCKUP << 8u,
        .clock_failure_mask = LS_NXP_KINETIS_SRS0_LOC | LS_NXP_KINETIS_SRS0_LOL,
    };
    uint32_t raw_reason = 0u;

    if (!port)
        return ls_reset_reason_from_masks(raw_reason, NULL);
    if (port->srs0_register)
        raw_reason = (uint32_t)*port->srs0_register;
    if (port->srs1_register)
        raw_reason |= (uint32_t)*port->srs1_register << 8u;
    return ls_reset_reason_from_masks(raw_reason, &masks);
}
