#include "microchip_reset.h"

void ls_microchip_samd_reset_profile(ls_microchip_samd_reset_port_t *port,
                                     volatile uint8_t *rcause_register) {
    if (port)
        *port = (ls_microchip_samd_reset_port_t){.rcause_register = rcause_register};
}

ls_reset_info_t ls_microchip_samd_reset_info(void *context) {
    ls_microchip_samd_reset_port_t *port = (ls_microchip_samd_reset_port_t *)context;
    ls_reset_info_t info = {LS_RESET_UNKNOWN, 0u, 0u, 0u, false, false, false};
    uint32_t raw_reason;

    if (!port || !port->rcause_register)
        return info;
    raw_reason = (uint32_t)*port->rcause_register;
    info.raw_reason = raw_reason;
    if ((raw_reason & (LS_MICROCHIP_SAMD_RCAUSE_BOD12 | LS_MICROCHIP_SAMD_RCAUSE_BOD33)) != 0u)
        info.reason = LS_RESET_BROWNOUT;
    else if ((raw_reason & LS_MICROCHIP_SAMD_RCAUSE_WDT) != 0u)
        info.reason = LS_RESET_WATCHDOG;
    else if ((raw_reason & LS_MICROCHIP_SAMD_RCAUSE_SYST) != 0u)
        info.reason = LS_RESET_SOFTWARE;
    else if ((raw_reason & LS_MICROCHIP_SAMD_RCAUSE_EXT) != 0u)
        info.reason = LS_RESET_PIN;
    else if ((raw_reason & LS_MICROCHIP_SAMD_RCAUSE_POR) != 0u)
        info.reason = LS_RESET_POWER_ON;
    else if ((raw_reason & LS_MICROCHIP_SAMD_RCAUSE_BACKUP) != 0u)
        info.reason = LS_RESET_LOW_POWER_WAKE;
    return info;
}

void ls_microchip_sam_rsttyp_reset_profile(ls_microchip_sam_rsttyp_reset_port_t *port,
                                           volatile uint32_t *status_register) {
    if (!port)
        return;
    *port = (ls_microchip_sam_rsttyp_reset_port_t){
        .status_register = status_register,
        .reset_type_mask = 7u << 8,
        .user_reset_mask = 1u << 0,
        .reset_type_shift = 8u,
    };
}

ls_reset_info_t ls_microchip_sam_rsttyp_reset_info(void *context) {
    ls_microchip_sam_rsttyp_reset_port_t *port = (ls_microchip_sam_rsttyp_reset_port_t *)context;
    ls_reset_info_t info = {LS_RESET_UNKNOWN, 0u, 0u, 0u, false, false, false};
    uint32_t reset_type;

    if (!port || !port->status_register)
        return info;
    info.raw_reason = *port->status_register;
    if (port->reset_type_mask == 0u || port->reset_type_shift >= 32u)
        return info;
    reset_type = (info.raw_reason & port->reset_type_mask) >> port->reset_type_shift;
    switch (reset_type) {
    case LS_MICROCHIP_SAM_RSTTYP_BACKUP:
        info.reason = LS_RESET_LOW_POWER_WAKE;
        break;
    case LS_MICROCHIP_SAM_RSTTYP_WATCHDOG:
        info.reason = LS_RESET_WATCHDOG;
        break;
    case LS_MICROCHIP_SAM_RSTTYP_SOFTWARE:
        info.reason = LS_RESET_SOFTWARE;
        break;
    case LS_MICROCHIP_SAM_RSTTYP_USER:
        info.reason = LS_RESET_PIN;
        break;
    case LS_MICROCHIP_SAM_RSTTYP_GENERAL:
        if ((info.raw_reason & port->user_reset_mask) != 0u)
            info.reason = LS_RESET_PIN;
        break;
    default:
        break;
    }
    return info;
}
