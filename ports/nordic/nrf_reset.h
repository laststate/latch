#ifndef LASTSTATE_NRF_RESET_H
#define LASTSTATE_NRF_RESET_H

#include "reset_reason.h"

/*
 * This adapter intentionally does not include nrfx/CMSIS headers. nRF52 and
 * nRF53 RESETREAS layouts are similar but not interchangeable: nRF53 inserts
 * CTRLAP before SREQ/LOCKUP and adds application/network-core causes. Select
 * the matching profile rather than copying raw bits across the families.
 */
typedef ls_reset_mask_port_t ls_nrf_reset_port_t;

enum {
    LS_NRF_RESETREAS_RESETPIN = 1u << 0,
    LS_NRF_RESETREAS_DOG = 1u << 1,
    LS_NRF_RESETREAS_SREQ = 1u << 2,
    LS_NRF_RESETREAS_LOCKUP = 1u << 3,
    LS_NRF_RESETREAS_OFF = 1u << 16,
    LS_NRF_RESETREAS_LPCOMP = 1u << 17
};

/* nRF53 RESETREAS, as used by nRF5340. Bits 19..26 additionally appear when
 * the network-core register is used; unsupported portable causes remain in
 * raw_reason instead of being guessed. */
enum {
    LS_NRF53_RESETREAS_RESETPIN = 1u << 0,
    LS_NRF53_RESETREAS_DOG0 = 1u << 1,
    LS_NRF53_RESETREAS_CTRLAP = 1u << 2,
    LS_NRF53_RESETREAS_SREQ = 1u << 3,
    LS_NRF53_RESETREAS_LOCKUP = 1u << 4,
    LS_NRF53_RESETREAS_OFF = 1u << 16,
    LS_NRF53_RESETREAS_LPCOMP = 1u << 17,
    LS_NRF53_RESETREAS_DIF = 1u << 18,
    LS_NRF53_RESETREAS_LSREQ = 1u << 19,
    LS_NRF53_RESETREAS_LLOCKUP = 1u << 20,
    LS_NRF53_RESETREAS_LDOG = 1u << 21,
    LS_NRF53_RESETREAS_MFORCEOFF = 1u << 22,
    LS_NRF53_RESETREAS_NFC = 1u << 23,
    LS_NRF53_RESETREAS_DOG1 = 1u << 24,
    LS_NRF53_RESETREAS_VBUS = 1u << 25,
    LS_NRF53_RESETREAS_LCTRLAP = 1u << 26
};

ls_reset_info_t ls_nrf_reset_info(void *context);
void ls_nrf_reset_clear(ls_nrf_reset_port_t *port);
void ls_nrf_reset_profile_nrf52(ls_nrf_reset_port_t *port, volatile uint32_t *resetreas);
void ls_nrf_reset_profile_nrf53(ls_nrf_reset_port_t *port, volatile uint32_t *resetreas);

#endif
