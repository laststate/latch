#include "../core/internal.h"
#include "laststate/envelope.h"
#include "laststate/network_transport.h"

ls_result_t ls_http_transport_send(void *context, const uint8_t *data, size_t length) {
    ls_http_transport_t *http = (ls_http_transport_t *)context;
    if (!http || !http->post || !http->endpoint || !data || !length) {
        return LS_EINVAL;
    }
    if (length > ls_http_transport_max_payload(http)) {
        return LS_ENOSPACE;
    }

    ls_result_t result = ls_envelope_validate(data, length, 0);
    if (result != LS_OK) {
        return result;
    }

    uint16_t status = 0;
    result = http->post(http->context, http->endpoint, "application/vnd.laststate.lep", data,
                        length, &status);
    if (result != LS_OK) {
        return result;
    }
    if (status >= 200u && status < 300u) {
        return LS_OK;
    }
    if (status == 408u || status == 425u || status == 429u || status >= 500u) {
        return LS_EAGAIN;
    }
    return LS_EIO;
}

size_t ls_http_transport_max_payload(void *context) {
    const ls_http_transport_t *http = (const ls_http_transport_t *)context;
    return http && http->maximum_payload ? http->maximum_payload : LS_MAX_EVENT_SIZE;
}

ls_result_t ls_mqtt_transport_send(void *context, const uint8_t *data, size_t length) {
    ls_mqtt_transport_t *mqtt = (ls_mqtt_transport_t *)context;
    if (!mqtt || !mqtt->publish || !mqtt->topic || !data || !length || mqtt->qos > 2u) {
        return LS_EINVAL;
    }
    if (length > ls_mqtt_transport_max_payload(mqtt)) {
        return LS_ENOSPACE;
    }

    ls_result_t result = ls_envelope_validate(data, length, 0);
    if (result != LS_OK) {
        return result;
    }
    return mqtt->publish(mqtt->context, mqtt->topic, data, length, mqtt->qos, false);
}

size_t ls_mqtt_transport_max_payload(void *context) {
    const ls_mqtt_transport_t *mqtt = (const ls_mqtt_transport_t *)context;
    return mqtt && mqtt->maximum_payload ? mqtt->maximum_payload : LS_MAX_EVENT_SIZE;
}

ls_result_t ls_incident_beacon_encode(const uint8_t *envelope, size_t envelope_length,
                                      uint32_t device_hash, uint16_t battery_mv,
                                      int16_t temperature_c, uint8_t *output, size_t capacity,
                                      size_t *written) {
    if (!envelope || !output || !written || capacity < 32u) {
        return LS_EINVAL;
    }

    ls_envelope_info_t info;
    ls_result_t result = ls_envelope_validate(envelope, envelope_length, &info);
    if (result != LS_OK) {
        return result;
    }

    ls_writer_t writer = {output, capacity, 0};
    result = ls_writer_u32(&writer, 0x4249534cu);
    if (result == LS_OK) {
        result = ls_writer_u8(&writer, 1u);
    }
    if (result == LS_OK) {
        result = ls_writer_u8(&writer, info.type);
    }
    if (result == LS_OK) {
        result = ls_writer_u8(&writer, info.architecture);
    }
    if (result == LS_OK) {
        result = ls_writer_u8(&writer, info.flags);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&writer, info.event_id);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&writer, device_hash);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&writer, ls_hash_string(ls_build_id()));
    }
    if (result == LS_OK) {
        result = ls_writer_u16(&writer, battery_mv);
    }
    if (result == LS_OK) {
        result = ls_writer_u16(&writer, (uint16_t)temperature_c);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&writer, info.sequence);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&writer, ls_crc32(output, writer.length));
    }
    if (result != LS_OK) {
        return result;
    }

    *written = writer.length;
    return LS_OK;
}
