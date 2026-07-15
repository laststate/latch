#ifndef LASTSTATE_ZEPHYR_LORAWAN_H
#define LASTSTATE_ZEPHYR_LORAWAN_H
#include <stdbool.h>
#include <stdint.h>
#include "laststate/transport.h"
typedef struct {
    ls_transport_backend_t backend;
    uint8_t port, payload_max;
    bool confirmed, joined;
} ls_zephyr_lorawan_transport_t;
ls_result_t ls_zephyr_lorawan_transport_init(ls_zephyr_lorawan_transport_t *transport, uint8_t port,
                                             uint8_t payload_max, bool confirmed);
void ls_zephyr_lorawan_set_joined(ls_zephyr_lorawan_transport_t *transport, bool joined);
#endif
