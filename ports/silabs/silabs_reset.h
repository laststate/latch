#ifndef LASTSTATE_SILABS_RESET_H
#define LASTSTATE_SILABS_RESET_H

#include "reset_reason.h"

/*
 * Silicon Labs RMU/EMU reset layouts vary by series.  The generic adapter
 * accepts the exact masks from the selected Gecko SDK device header.  The
 * EFM32 Series 0 profile below covers the stable legacy RMU layout.
 */
typedef ls_reset_mask_port_t ls_silabs_reset_port_t;

enum {
    LS_SILABS_EFM32_SERIES0_PORST = 1u << 0,
    LS_SILABS_EFM32_SERIES0_BODUNREGRST = 1u << 1,
    LS_SILABS_EFM32_SERIES0_BODREGRST = 1u << 2,
    LS_SILABS_EFM32_SERIES0_EXTRST = 1u << 3,
    LS_SILABS_EFM32_SERIES0_WDOGRST = 1u << 4,
    LS_SILABS_EFM32_SERIES0_LOCKUPRST = 1u << 5,
    LS_SILABS_EFM32_SERIES0_SYSREQRST = 1u << 6
};

/* EFR32/EFM32 Series 2 EMU RSTCAUSE layout (for example MG21/BG21). */
enum {
    LS_SILABS_SERIES2_POR = 1u << 0,
    LS_SILABS_SERIES2_PIN = 1u << 1,
    LS_SILABS_SERIES2_EM4 = 1u << 2,
    LS_SILABS_SERIES2_WDOG0 = 1u << 3,
    LS_SILABS_SERIES2_WDOG1 = 1u << 4,
    LS_SILABS_SERIES2_LOCKUP = 1u << 5,
    LS_SILABS_SERIES2_SYSREQ = 1u << 6,
    LS_SILABS_SERIES2_DVDDBOD = 1u << 7,
    LS_SILABS_SERIES2_DVDDLEBOD = 1u << 8,
    LS_SILABS_SERIES2_DECBOD = 1u << 9,
    LS_SILABS_SERIES2_AVDDBOD = 1u << 10,
    LS_SILABS_SERIES2_IOVDD0BOD = 1u << 11,
    LS_SILABS_SERIES2_SETAMPER = 1u << 13,
    LS_SILABS_SERIES2_SESYSREQ = 1u << 14,
    LS_SILABS_SERIES2_SELOCKUP = 1u << 15
};

ls_reset_info_t ls_silabs_reset_info(void *context);
void ls_silabs_reset_clear(ls_silabs_reset_port_t *port);
void ls_silabs_efm32_series0_reset_profile(ls_silabs_reset_port_t *port,
                                           volatile uint32_t *rstcause_register,
                                           volatile uint32_t *command_register,
                                           uint32_t clear_command);
void ls_silabs_series2_reset_profile(ls_silabs_reset_port_t *port,
                                     volatile uint32_t *rstcause_register,
                                     volatile uint32_t *command_register, uint32_t clear_command);

#endif
