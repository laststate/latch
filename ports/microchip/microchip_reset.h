#ifndef LASTSTATE_MICROCHIP_RESET_H
#define LASTSTATE_MICROCHIP_RESET_H

#include "laststate/event.h"

/* SAM D/E reset controller RCause is an 8-bit status register. */
typedef struct {
    volatile uint8_t *rcause_register;
} ls_microchip_samd_reset_port_t;

enum {
    LS_MICROCHIP_SAMD_RCAUSE_POR = 1u << 0,
    LS_MICROCHIP_SAMD_RCAUSE_BOD12 = 1u << 1,
    LS_MICROCHIP_SAMD_RCAUSE_BOD33 = 1u << 2,
    LS_MICROCHIP_SAMD_RCAUSE_EXT = 1u << 4,
    LS_MICROCHIP_SAMD_RCAUSE_WDT = 1u << 5,
    LS_MICROCHIP_SAMD_RCAUSE_SYST = 1u << 6,
    LS_MICROCHIP_SAMD_RCAUSE_BACKUP = 1u << 7
};

void ls_microchip_samd_reset_profile(ls_microchip_samd_reset_port_t *port,
                                     volatile uint8_t *rcause_register);
ls_reset_info_t ls_microchip_samd_reset_info(void *context);

/*
 * SAM3/SAM4/SAME reset controllers encode a single reset type in RSTTYP.
 * The profile below uses the common RSTC_SR layout and does not write the
 * status register, which is read-only on those devices.
 */
typedef struct {
    volatile uint32_t *status_register;
    uint32_t reset_type_mask;
    uint32_t user_reset_mask;
    uint8_t reset_type_shift;
} ls_microchip_sam_rsttyp_reset_port_t;

enum {
    LS_MICROCHIP_SAM_RSTTYP_GENERAL = 0u,
    LS_MICROCHIP_SAM_RSTTYP_BACKUP = 1u,
    LS_MICROCHIP_SAM_RSTTYP_WATCHDOG = 2u,
    LS_MICROCHIP_SAM_RSTTYP_SOFTWARE = 3u,
    LS_MICROCHIP_SAM_RSTTYP_USER = 4u
};

void ls_microchip_sam_rsttyp_reset_profile(ls_microchip_sam_rsttyp_reset_port_t *port,
                                           volatile uint32_t *status_register);
ls_reset_info_t ls_microchip_sam_rsttyp_reset_info(void *context);

#endif
