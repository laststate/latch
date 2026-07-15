#include <stdio.h>
#include "stm32_reset.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "stm32 reset failed: %s:%d\n", #x, __LINE__);                          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    uint32_t csr = 0;
    ls_stm32_reset_port_t port;
    ls_stm32_reset_profile_f4(&port, &csr);
    csr = 1u << 25;
    CHECK(ls_stm32_reset_info(&port).reason == LS_RESET_BROWNOUT);
    csr = 1u << 29;
    CHECK(ls_stm32_reset_info(&port).reason == LS_RESET_INDEPENDENT_WATCHDOG);
    csr = 1u << 30;
    CHECK(ls_stm32_reset_info(&port).reason == LS_RESET_WINDOW_WATCHDOG);
    csr = 1u << 28;
    CHECK(ls_stm32_reset_info(&port).reason == LS_RESET_SOFTWARE);
    csr = 1u << 26;
    CHECK(ls_stm32_reset_info(&port).reason == LS_RESET_PIN);
    csr = 1u << 27;
    CHECK(ls_stm32_reset_info(&port).reason == LS_RESET_POWER_ON);
    csr = 0;
    ls_stm32_reset_clear(&port);
    CHECK(csr == (1u << 24));
    return 0;
}
