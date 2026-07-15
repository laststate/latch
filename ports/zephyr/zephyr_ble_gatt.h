#ifndef LASTSTATE_ZEPHYR_BLE_GATT_H
#define LASTSTATE_ZEPHYR_BLE_GATT_H
#include <stddef.h>
#include <stdint.h>
#include "laststate/transport.h"
typedef void (*ls_zephyr_ble_control_fn)(void *context, const uint8_t *data, size_t length);
typedef struct {
    ls_transport_backend_t backend;
    void *connection;
    ls_zephyr_ble_control_fn control;
    void *control_context;
    uint16_t mtu;
    uint8_t notifications_enabled;
} ls_zephyr_ble_transport_t;
ls_result_t ls_zephyr_ble_transport_init(ls_zephyr_ble_transport_t *transport, void *connection,
                                         ls_zephyr_ble_control_fn control, void *control_context);
void ls_zephyr_ble_set_connection(ls_zephyr_ble_transport_t *transport, void *connection);
#endif
