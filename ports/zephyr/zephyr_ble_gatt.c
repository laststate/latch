#include "zephyr_ble_gatt.h"
#if defined(__ZEPHYR__) && defined(CONFIG_BT)
#include <errno.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
static ls_zephyr_ble_transport_t *active_transport;
static struct bt_uuid_128 service_uuid =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x8e42f15a, 0x7b34, 0x4d91, 0xa270, 0x11f77a7c0001));
static struct bt_uuid_128 data_uuid =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x8e42f15a, 0x7b34, 0x4d91, 0xa270, 0x11f77a7c0002));
static struct bt_uuid_128 control_uuid =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x8e42f15a, 0x7b34, 0x4d91, 0xa270, 0x11f77a7c0003));
static void ccc_changed(const struct bt_gatt_attr *attribute, uint16_t value) {
    (void)attribute;
    if (active_transport)
        active_transport->notifications_enabled = (uint8_t)(value == BT_GATT_CCC_NOTIFY);
}
static ssize_t control_write(struct bt_conn *connection, const struct bt_gatt_attr *attribute,
                             const void *buffer, uint16_t length, uint16_t offset, uint8_t flags) {
    (void)connection;
    (void)attribute;
    (void)flags;
    if (offset != 0u)
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    if (active_transport && active_transport->control)
        active_transport->control(active_transport->control_context, (const uint8_t *)buffer,
                                  length);
    return length;
}
BT_GATT_SERVICE_DEFINE(latch_service, BT_GATT_PRIMARY_SERVICE(&service_uuid),
                       BT_GATT_CHARACTERISTIC(&data_uuid.uuid, BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_NONE, NULL, NULL, NULL),
                       BT_GATT_CCC(ccc_changed,
                                   BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
                       BT_GATT_CHARACTERISTIC(&control_uuid.uuid, BT_GATT_CHRC_WRITE,
                                              BT_GATT_PERM_WRITE_ENCRYPT, NULL, control_write,
                                              NULL));
static bool available(void *context) {
    ls_zephyr_ble_transport_t *transport = (ls_zephyr_ble_transport_t *)context;
    return transport && transport->connection && transport->notifications_enabled;
}
static size_t maximum_payload(void *context) {
    ls_zephyr_ble_transport_t *transport = (ls_zephyr_ble_transport_t *)context;
    struct bt_conn *connection = (struct bt_conn *)transport->connection;
    uint16_t mtu = connection ? bt_gatt_get_mtu(connection) : 23u;
    return mtu > 3u ? (size_t)(mtu - 3u) : 0u;
}
static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    ls_zephyr_ble_transport_t *transport = (ls_zephyr_ble_transport_t *)context;
    if (!available(context) || (!data && length))
        return LS_EINVAL;
    size_t chunk = maximum_payload(context);
    while (length) {
        size_t take = length > chunk ? chunk : length;
        int result = bt_gatt_notify((struct bt_conn *)transport->connection,
                                    &latch_service.attrs[2], data, (uint16_t)take);
        if (result == -ENOMEM || result == -EAGAIN)
            return LS_EAGAIN;
        if (result < 0)
            return LS_EIO;
        data += take;
        length -= take;
    }
    return LS_OK;
}
ls_result_t ls_zephyr_ble_transport_init(ls_zephyr_ble_transport_t *transport, void *connection,
                                         ls_zephyr_ble_control_fn control, void *control_context) {
    if (!transport)
        return LS_EINVAL;
    *transport = (ls_zephyr_ble_transport_t){0};
    transport->connection = connection;
    transport->control = control;
    transport->control_context = control_context;
    transport->backend = (ls_transport_backend_t){
        .name = "zephyr-ble-gatt",
        .priority = 40,
        .available = available,
        .send = send_data,
        .max_payload = maximum_payload,
        .context = transport,
        .capabilities = LS_TRANSPORT_FRAGMENT | LS_TRANSPORT_SECURE | LS_TRANSPORT_LOW_POWER};
    active_transport = transport;
    return LS_OK;
}
void ls_zephyr_ble_set_connection(ls_zephyr_ble_transport_t *transport, void *connection) {
    if (transport) {
        transport->connection = connection;
        if (!connection)
            transport->notifications_enabled = 0;
    }
}
#else
ls_result_t ls_zephyr_ble_transport_init(ls_zephyr_ble_transport_t *transport, void *connection,
                                         ls_zephyr_ble_control_fn control, void *control_context) {
    (void)transport;
    (void)connection;
    (void)control;
    (void)control_context;
    return LS_ENOTSUP;
}
void ls_zephyr_ble_set_connection(ls_zephyr_ble_transport_t *transport, void *connection) {
    (void)transport;
    (void)connection;
}
#endif
