#include "zephyr_lorawan.h"
#include "laststate/config.h"
#if defined(__ZEPHYR__) && defined(CONFIG_LORAWAN)
#include <errno.h>
#include <zephyr/lorawan/lorawan.h>
static uint16_t checksum(const uint8_t *data, size_t length) {
    uint16_t value = 0xffffu;
    for (size_t i = 0; i < length; i++)
        value = (uint16_t)((value << 5) | (value >> 11)) ^ data[i];
    return value;
}
static bool available(void *context) {
    ls_zephyr_lorawan_transport_t *transport = (ls_zephyr_lorawan_transport_t *)context;
    return transport && transport->joined;
}
static size_t maximum_payload(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}
static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    ls_zephyr_lorawan_transport_t *transport = (ls_zephyr_lorawan_transport_t *)context;
    if (!available(context) || (!data && length) || transport->payload_max <= 4u)
        return LS_EINVAL;
    size_t capacity = transport->payload_max - 4u, count = (length + capacity - 1u) / capacity;
    if (count > 255u)
        return LS_ENOSPACE;
    uint16_t crc = checksum(data, length);
    for (size_t index = 0; index < count; index++) {
        uint8_t frame[255];
        size_t offset = index * capacity, take = length - offset;
        if (take > capacity)
            take = capacity;
        frame[0] = (uint8_t)crc;
        frame[1] = (uint8_t)(crc >> 8);
        frame[2] = (uint8_t)index;
        frame[3] = (uint8_t)count;
        for (size_t i = 0; i < take; i++)
            frame[4u + i] = data[offset + i];
        int result =
            lorawan_send(transport->port, frame, (uint8_t)(take + 4u),
                         transport->confirmed ? LORAWAN_MSG_CONFIRMED : LORAWAN_MSG_UNCONFIRMED);
        if (result == -EAGAIN || result == -EBUSY)
            return LS_EAGAIN;
        if (result < 0)
            return LS_EIO;
    }
    return LS_OK;
}
ls_result_t ls_zephyr_lorawan_transport_init(ls_zephyr_lorawan_transport_t *transport, uint8_t port,
                                             uint8_t payload_max, bool confirmed) {
    if (!transport || !port || payload_max <= 4u)
        return LS_EINVAL;
    *transport = (ls_zephyr_lorawan_transport_t){0};
    transport->port = port;
    transport->payload_max = payload_max;
    transport->confirmed = confirmed;
    transport->backend = (ls_transport_backend_t){
        .name = "zephyr-lorawan",
        .priority = 50,
        .available = available,
        .send = send_data,
        .max_payload = maximum_payload,
        .context = transport,
        .capabilities = LS_TRANSPORT_FRAGMENT | LS_TRANSPORT_EXPENSIVE | LS_TRANSPORT_LOW_POWER};
    return LS_OK;
}
void ls_zephyr_lorawan_set_joined(ls_zephyr_lorawan_transport_t *transport, bool joined) {
    if (transport)
        transport->joined = joined;
}
#else
ls_result_t ls_zephyr_lorawan_transport_init(ls_zephyr_lorawan_transport_t *transport, uint8_t port,
                                             uint8_t payload_max, bool confirmed) {
    (void)transport;
    (void)port;
    (void)payload_max;
    (void)confirmed;
    return LS_ENOTSUP;
}
void ls_zephyr_lorawan_set_joined(ls_zephyr_lorawan_transport_t *transport, bool joined) {
    (void)transport;
    (void)joined;
}
#endif
