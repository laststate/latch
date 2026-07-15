#include "zephyr_can.h"
#include "laststate/config.h"
#if defined(__ZEPHYR__) && defined(CONFIG_CAN)
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
static uint8_t transfer_id(const uint8_t *data, size_t length) {
    uint8_t value = 0x5au;
    for (size_t i = 0; i < length; i++)
        value = (uint8_t)((value << 1) | (value >> 7)) ^ data[i];
    return value;
}
static bool available(void *context) {
    ls_zephyr_can_transport_t *transport = (ls_zephyr_can_transport_t *)context;
    return transport && transport->device &&
           device_is_ready((const struct device *)transport->device);
}
static size_t maximum_payload(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}
static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    ls_zephyr_can_transport_t *transport = (ls_zephyr_can_transport_t *)context;
    if (!available(context) || (!data && length))
        return LS_EINVAL;
    size_t frame_capacity = transport->can_fd ? 64u : 8u, payload_capacity = frame_capacity - 3u,
           count = (length + payload_capacity - 1u) / payload_capacity;
    if (count > 255u)
        return LS_ENOSPACE;
    uint8_t id = transfer_id(data, length);
    for (size_t index = 0; index < count; index++) {
        struct can_frame frame = {0};
        size_t offset = index * payload_capacity, take = length - offset;
        if (take > payload_capacity)
            take = payload_capacity;
        frame.id = transport->arbitration_id;
        frame.flags = transport->arbitration_id > CAN_STD_ID_MASK ? CAN_FRAME_IDE : 0;
        if (transport->can_fd) {
            frame.flags |= CAN_FRAME_FDF;
            if (transport->bitrate_switch)
                frame.flags |= CAN_FRAME_BRS;
        }
        frame.data[0] = id;
        frame.data[1] = (uint8_t)index;
        frame.data[2] = (uint8_t)count;
        for (size_t i = 0; i < take; i++)
            frame.data[3u + i] = data[offset + i];
        frame.dlc = can_bytes_to_dlc(take + 3u);
        int result = can_send((const struct device *)transport->device, &frame,
                              K_MSEC(transport->timeout_ms), NULL, NULL);
        if (result == -EAGAIN)
            return LS_EAGAIN;
        if (result < 0)
            return LS_EIO;
    }
    return LS_OK;
}
ls_result_t ls_zephyr_can_transport_init(ls_zephyr_can_transport_t *transport, const void *device,
                                         uint32_t arbitration_id, bool can_fd, bool bitrate_switch,
                                         uint32_t timeout_ms) {
    if (!transport || !device || arbitration_id > CAN_EXT_ID_MASK)
        return LS_EINVAL;
#if !defined(CONFIG_CAN_FD_MODE)
    if (can_fd || bitrate_switch)
        return LS_ENOTSUP;
#endif
    *transport = (ls_zephyr_can_transport_t){0};
    transport->device = device;
    transport->arbitration_id = arbitration_id;
    transport->can_fd = can_fd;
    transport->bitrate_switch = bitrate_switch;
    transport->timeout_ms = timeout_ms;
    transport->backend = (ls_transport_backend_t){.name = can_fd ? "zephyr-can-fd" : "zephyr-can",
                                                  .priority = 30,
                                                  .available = available,
                                                  .send = send_data,
                                                  .max_payload = maximum_payload,
                                                  .context = transport,
                                                  .capabilities = LS_TRANSPORT_FRAGMENT};
    return LS_OK;
}
#else
ls_result_t ls_zephyr_can_transport_init(ls_zephyr_can_transport_t *transport, const void *device,
                                         uint32_t arbitration_id, bool can_fd, bool bitrate_switch,
                                         uint32_t timeout_ms) {
    (void)transport;
    (void)device;
    (void)arbitration_id;
    (void)can_fd;
    (void)bitrate_switch;
    (void)timeout_ms;
    return LS_ENOTSUP;
}
#endif
