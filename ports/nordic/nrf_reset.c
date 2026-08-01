#include "nrf_reset.h"

static void ls_nrf_reset_profile_common(ls_nrf_reset_port_t *port, volatile uint32_t *resetreas) {
    if (!port)
        return;
    *port = (ls_nrf_reset_port_t){
        .status_register = resetreas,
        .clear_register = resetreas,
        .clear_mask = LS_NRF_RESETREAS_RESETPIN | LS_NRF_RESETREAS_DOG | LS_NRF_RESETREAS_SREQ |
                      LS_NRF_RESETREAS_LOCKUP | LS_NRF_RESETREAS_OFF | LS_NRF_RESETREAS_LPCOMP,
        .masks =
            {
                .pin_mask = LS_NRF_RESETREAS_RESETPIN,
                .software_mask = LS_NRF_RESETREAS_SREQ,
                .watchdog_mask = LS_NRF_RESETREAS_DOG,
                .lockup_mask = LS_NRF_RESETREAS_LOCKUP,
                .low_power_mask = LS_NRF_RESETREAS_OFF | LS_NRF_RESETREAS_LPCOMP,
            },
    };
}

static void ls_nrf_reset_profile_nrf53_common(ls_nrf_reset_port_t *port,
                                              volatile uint32_t *resetreas) {
    if (!port)
        return;
    *port = (ls_nrf_reset_port_t){
        .status_register = resetreas,
        .clear_register = resetreas,
        .clear_mask = LS_NRF53_RESETREAS_RESETPIN | LS_NRF53_RESETREAS_DOG0 |
                      LS_NRF53_RESETREAS_CTRLAP | LS_NRF53_RESETREAS_SREQ |
                      LS_NRF53_RESETREAS_LOCKUP | LS_NRF53_RESETREAS_OFF |
                      LS_NRF53_RESETREAS_LPCOMP | LS_NRF53_RESETREAS_DIF |
                      LS_NRF53_RESETREAS_LSREQ | LS_NRF53_RESETREAS_LLOCKUP |
                      LS_NRF53_RESETREAS_LDOG | LS_NRF53_RESETREAS_MFORCEOFF |
                      LS_NRF53_RESETREAS_NFC | LS_NRF53_RESETREAS_DOG1 |
                      LS_NRF53_RESETREAS_VBUS | LS_NRF53_RESETREAS_LCTRLAP,
        .masks =
            {
                .pin_mask = LS_NRF53_RESETREAS_RESETPIN,
                .software_mask = LS_NRF53_RESETREAS_SREQ | LS_NRF53_RESETREAS_LSREQ,
                .watchdog_mask = LS_NRF53_RESETREAS_DOG0 | LS_NRF53_RESETREAS_LDOG |
                                  LS_NRF53_RESETREAS_DOG1,
                .lockup_mask = LS_NRF53_RESETREAS_LOCKUP | LS_NRF53_RESETREAS_LLOCKUP,
                .security_mask = LS_NRF53_RESETREAS_CTRLAP | LS_NRF53_RESETREAS_LCTRLAP,
                .low_power_mask = LS_NRF53_RESETREAS_OFF | LS_NRF53_RESETREAS_LPCOMP |
                                  LS_NRF53_RESETREAS_DIF | LS_NRF53_RESETREAS_NFC |
                                  LS_NRF53_RESETREAS_VBUS,
            },
    };
}

ls_reset_info_t ls_nrf_reset_info(void *context) {
    return ls_reset_mask_port_info(context);
}

void ls_nrf_reset_clear(ls_nrf_reset_port_t *port) {
    ls_reset_mask_port_clear(port);
}

void ls_nrf_reset_profile_nrf52(ls_nrf_reset_port_t *port, volatile uint32_t *resetreas) {
    ls_nrf_reset_profile_common(port, resetreas);
}

void ls_nrf_reset_profile_nrf53(ls_nrf_reset_port_t *port, volatile uint32_t *resetreas) {
    ls_nrf_reset_profile_nrf53_common(port, resetreas);
}
