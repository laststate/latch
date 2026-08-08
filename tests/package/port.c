#include "nrf_reset.h"

int main(void) {
    volatile uint32_t reset_reason = LS_NRF_RESETREAS_SREQ;
    ls_nrf_reset_port_t port;
    ls_nrf_reset_profile_nrf52(&port, &reset_reason);
    return ls_nrf_reset_info(&port).reason == LS_RESET_SOFTWARE ? 0 : 1;
}
