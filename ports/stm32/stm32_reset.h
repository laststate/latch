#ifndef LASTSTATE_STM32_RESET_H
#define LASTSTATE_STM32_RESET_H
#include <stdint.h>
#include "laststate/event.h"
typedef struct {
    volatile uint32_t *status_register;
    volatile uint32_t *clear_register;
    uint32_t clear_mask, power_on_mask, pin_mask, software_mask, watchdog_mask,
        window_watchdog_mask, brownout_mask, low_power_mask, clock_failure_mask;
} ls_stm32_reset_port_t;
ls_reset_info_t ls_stm32_reset_info(void *context);
void ls_stm32_reset_clear(ls_stm32_reset_port_t *port);
void ls_stm32_reset_profile_f4(ls_stm32_reset_port_t *port, volatile uint32_t *rcc_csr);
#define LS_STM32_RESET_PORT_FROM_RCC_RSR(port, register_pointer, clear_bit, power_on_bit, pin_bit, \
                                         software_bit, independent_watchdog_bit,                   \
                                         window_watchdog_bit, brownout_bit, low_power_bit)         \
    do {                                                                                           \
        (port) = (ls_stm32_reset_port_t){(register_pointer),                                       \
                                         (register_pointer),                                       \
                                         (clear_bit),                                              \
                                         (power_on_bit),                                           \
                                         (pin_bit),                                                \
                                         (software_bit),                                           \
                                         (independent_watchdog_bit),                               \
                                         (window_watchdog_bit),                                    \
                                         (brownout_bit),                                           \
                                         (low_power_bit),                                          \
                                         0u};                                                      \
    } while (0)
#endif
