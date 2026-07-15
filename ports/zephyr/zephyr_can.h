#ifndef LASTSTATE_ZEPHYR_CAN_H
#define LASTSTATE_ZEPHYR_CAN_H
#include <stdbool.h>
#include <stdint.h>
#include "laststate/transport.h"
typedef struct {
    ls_transport_backend_t backend;
    const void *device;
    uint32_t arbitration_id, timeout_ms;
    bool can_fd, bitrate_switch;
} ls_zephyr_can_transport_t;
ls_result_t ls_zephyr_can_transport_init(ls_zephyr_can_transport_t *transport, const void *device,
                                         uint32_t arbitration_id, bool can_fd, bool bitrate_switch,
                                         uint32_t timeout_ms);
#endif
