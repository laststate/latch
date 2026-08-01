#ifndef LASTSTATE_NXP_RESET_H
#define LASTSTATE_NXP_RESET_H

#include "reset_reason.h"

/*
 * i.MX RT reset-source registers differ across SoCs.  The generic adapter
 * accepts the device-header masks supplied by the application without taking
 * an MCUXpresso SDK dependency.
 */
typedef ls_reset_mask_port_t ls_nxp_reset_port_t;

enum {
    LS_NXP_IMXRT105X_SRSR_IPP_RESET_B = 1u << 0,
    LS_NXP_IMXRT105X_SRSR_LOCKUP_SYSRESETREQ = 1u << 1,
    LS_NXP_IMXRT105X_SRSR_CSU_RESET_B = 1u << 2,
    LS_NXP_IMXRT105X_SRSR_IPP_USER_RESET_B = 1u << 3,
    LS_NXP_IMXRT105X_SRSR_WDOG_RST_B = 1u << 4,
    LS_NXP_IMXRT105X_SRSR_JTAG_RST_B = 1u << 5,
    LS_NXP_IMXRT105X_SRSR_JTAG_SW_RST = 1u << 6,
    LS_NXP_IMXRT105X_SRSR_WDOG3_RST_B = 1u << 7,
    LS_NXP_IMXRT105X_SRSR_TEMPSENSE_RST_B = 1u << 8
};

/* LOCKUP_SYSRESETREQ is intentionally retained in raw_reason but not mapped
 * to LS_RESET_LOCKUP: NXP documents the same bit for either a CPU lockup or
 * a normal ARM SYSRESETREQ software reset, so the register alone cannot
 * distinguish the portable cause. */

ls_reset_info_t ls_nxp_reset_info(void *context);
void ls_nxp_reset_clear(ls_nxp_reset_port_t *port);
void ls_nxp_reset_port_init(ls_nxp_reset_port_t *port, volatile uint32_t *status_register,
                            volatile uint32_t *clear_register, uint32_t clear_mask,
                            ls_reset_reason_masks_t masks);
void ls_nxp_imxrt105x_reset_profile(ls_nxp_reset_port_t *port, volatile uint32_t *srsr_register);

/*
 * Kinetis RCM exposes reset sources as two 8-bit registers.  raw_reason
 * packs SRS0 in bits 0..7 and SRS1 in bits 8..15, preserving both values for
 * the envelope without requiring a vendor header.
 */
typedef struct {
    volatile uint8_t *srs0_register;
    volatile uint8_t *srs1_register;
} ls_nxp_kinetis_reset_port_t;

enum {
    LS_NXP_KINETIS_SRS0_LVD = 1u << 1,
    LS_NXP_KINETIS_SRS0_LOC = 1u << 2,
    LS_NXP_KINETIS_SRS0_LOL = 1u << 3,
    LS_NXP_KINETIS_SRS0_WDOG = 1u << 5,
    LS_NXP_KINETIS_SRS0_PIN = 1u << 6,
    LS_NXP_KINETIS_SRS0_POR = 1u << 7,
    LS_NXP_KINETIS_SRS1_LOCKUP = 1u << 1,
    LS_NXP_KINETIS_SRS1_SW = 1u << 2
};

void ls_nxp_kinetis_reset_profile(ls_nxp_kinetis_reset_port_t *port,
                                  volatile uint8_t *srs0_register, volatile uint8_t *srs1_register);
ls_reset_info_t ls_nxp_kinetis_reset_info(void *context);

#endif
