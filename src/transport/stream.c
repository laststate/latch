#include "../core/internal.h"
#include "laststate/envelope.h"
#include "laststate/stream_transport.h"

static void write_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void clear_pending(ls_stream_transport_t *stream) {
    stream->pending_event_id = 0;
    stream->pending_crc = 0;
    stream->pending_length = 0;
    stream->header_offset = 0;
    stream->payload_offset = 0;
    stream->trailer_offset = 0;
    stream->pending = false;
    stream->awaiting_ack = false;
}

static ls_result_t acquire_stream(ls_stream_transport_t *stream, const ls_envelope_info_t *info,
                                  size_t length, uint32_t crc) {
    ls_result_t result = LS_OK;
    ls_enter_critical();
    if (stream->state_magic != LS_STREAM_TRANSPORT_STATE_MAGIC) {
        result = LS_EINVAL;
    } else if (stream->sending) {
        result = LS_EBUSY;
    } else if (stream->pending &&
               (stream->pending_event_id != info->event_id || stream->pending_length != length ||
                stream->pending_crc != crc)) {
        result = LS_EBUSY;
    } else {
        if (!stream->pending) {
            stream->pending_event_id = info->event_id;
            stream->pending_crc = crc;
            stream->pending_length = length;
            stream->header_offset = 0;
            stream->payload_offset = 0;
            stream->trailer_offset = 0;
            stream->pending = true;
            stream->awaiting_ack = false;
        }
        stream->sending = true;
    }
    ls_leave_critical();
    return result;
}

ls_result_t ls_stream_transport_init(ls_stream_transport_t *stream) {
    if (!stream) {
        return LS_EINVAL;
    }
    ls_enter_critical();
    stream->pending_event_id = 0;
    stream->pending_crc = 0;
    stream->pending_length = 0;
    stream->header_offset = 0;
    stream->payload_offset = 0;
    stream->trailer_offset = 0;
    stream->pending = false;
    stream->awaiting_ack = false;
    stream->sending = false;
    stream->state_magic = LS_STREAM_TRANSPORT_STATE_MAGIC;
    ls_leave_critical();
    return LS_OK;
}

static void release_stream(ls_stream_transport_t *stream, ls_result_t result) {
    ls_enter_critical();
    if (ls_transport_result_is_retryable(result) && stream->awaiting_ack) {
        stream->header_offset = 0;
        stream->payload_offset = 0;
        stream->trailer_offset = 0;
        stream->awaiting_ack = false;
    } else if (result == LS_OK || !ls_transport_result_is_retryable(result)) {
        clear_pending(stream);
    }
    stream->sending = false;
    ls_leave_critical();
}

ls_result_t ls_stream_transport_reset(ls_stream_transport_t *stream) {
    if (!stream) {
        return LS_EINVAL;
    }
    ls_enter_critical();
    if (stream->state_magic != LS_STREAM_TRANSPORT_STATE_MAGIC) {
        ls_leave_critical();
        return LS_EINVAL;
    }
    if (stream->sending) {
        ls_leave_critical();
        return LS_EBUSY;
    }
    clear_pending(stream);
    ls_leave_critical();
    return LS_OK;
}

ls_result_t ls_stream_transport_send(void *context, const uint8_t *data, size_t length) {
    ls_stream_transport_t *stream = (ls_stream_transport_t *)context;
    if (!stream || !stream->write || !data || length > UINT32_MAX) {
        return LS_EINVAL;
    }
    size_t maximum = stream->maximum_envelope ? stream->maximum_envelope : LS_MAX_EVENT_SIZE;
    if (length > maximum) {
        return LS_ENOSPACE;
    }

    ls_envelope_info_t info;
    ls_result_t result = ls_envelope_validate(data, length, &info);
    if (result != LS_OK) {
        return result;
    }
    uint32_t crc = ls_crc32(data, length);
    result = acquire_stream(stream, &info, length, crc);
    if (result != LS_OK) {
        return result;
    }

    uint8_t header[LS_STREAM_TRANSPORT_HEADER_SIZE] = {
        'L',
        'S',
        LS_STREAM_TRANSPORT_VERSION,
        0,
        (uint8_t)length,
        (uint8_t)(length >> 8),
        (uint8_t)(length >> 16),
        (uint8_t)(length >> 24),
    };
    uint8_t trailer[LS_STREAM_TRANSPORT_TRAILER_SIZE];
    write_u32(trailer, crc);

    if (!stream->awaiting_ack && stream->header_offset < LS_STREAM_TRANSPORT_HEADER_SIZE) {
        size_t remaining = LS_STREAM_TRANSPORT_HEADER_SIZE - stream->header_offset;
        result = stream->write(stream->context, header + stream->header_offset, remaining);
        if (result == LS_OK) {
            stream->header_offset = LS_STREAM_TRANSPORT_HEADER_SIZE;
        }
    }
    if (result == LS_OK && !stream->awaiting_ack && stream->payload_offset < length) {
        size_t remaining = length - stream->payload_offset;
        result = stream->write(stream->context, data + stream->payload_offset, remaining);
        if (result == LS_OK) {
            stream->payload_offset = length;
        }
    }
    if (result == LS_OK && !stream->awaiting_ack &&
        stream->trailer_offset < LS_STREAM_TRANSPORT_TRAILER_SIZE) {
        size_t remaining = LS_STREAM_TRANSPORT_TRAILER_SIZE - stream->trailer_offset;
        result = stream->write(stream->context, trailer + stream->trailer_offset, remaining);
        if (result == LS_OK) {
            stream->trailer_offset = LS_STREAM_TRANSPORT_TRAILER_SIZE;
        }
    }
    if (result == LS_OK && stream->wait_ack) {
        stream->awaiting_ack = true;
        result = stream->wait_ack(stream->context, info.event_id, stream->ack_timeout_ms);
    }

    release_stream(stream, result);
    return result;
}

size_t ls_stream_transport_max_payload(void *context) {
    const ls_stream_transport_t *stream = (const ls_stream_transport_t *)context;
    if (!stream) {
        return 0;
    }
    return stream->maximum_envelope ? stream->maximum_envelope : LS_MAX_EVENT_SIZE;
}

ls_result_t ls_lsak_parse(const uint8_t *data, size_t length, ls_lsak_t *out) {
    if (!data || !out || length != LS_LSAK_SIZE) {
        return LS_EINVAL;
    }
    if (data[0] != (uint8_t)'L' || data[1] != (uint8_t)'S' || data[2] != (uint8_t)'A' ||
        data[3] != (uint8_t)'K') {
        return LS_ECORRUPT;
    }
    if (data[4] != LS_LSAK_VERSION) {
        return LS_ENOTSUP;
    }
    if (data[5] < LS_LSAK_ACK_STORED || data[5] > LS_LSAK_NACK_INTERNAL || data[6] != 0u ||
        data[7] != 0u) {
        return LS_ECORRUPT;
    }
    out->version = data[4];
    out->status = data[5];
    out->event_id = (uint32_t)data[8] | ((uint32_t)data[9] << 8) | ((uint32_t)data[10] << 16) |
                    ((uint32_t)data[11] << 24);
    return LS_OK;
}

bool ls_lsak_is_success(uint8_t status) {
    return status == LS_LSAK_ACK_STORED || status == LS_LSAK_ACK_DUPLICATE;
}

ls_result_t ls_stream_frame_parse(const uint8_t *data, size_t length, size_t maximum_envelope,
                                  ls_stream_frame_t *out) {
    if (!data || !out ||
        length < LS_STREAM_TRANSPORT_HEADER_SIZE + LS_STREAM_TRANSPORT_TRAILER_SIZE) {
        return LS_EINVAL;
    }
    if (data[0] != (uint8_t)'L' || data[1] != (uint8_t)'S') {
        return LS_ECORRUPT;
    }
    if (data[2] != LS_STREAM_TRANSPORT_VERSION || data[3] != 0u) {
        return LS_ENOTSUP;
    }
    uint32_t encoded_length = read_u32(data + 4u);
    size_t maximum = maximum_envelope ? maximum_envelope : LS_MAX_EVENT_SIZE;
    if ((size_t)encoded_length > maximum) {
        return LS_ENOSPACE;
    }
    size_t overhead = LS_STREAM_TRANSPORT_HEADER_SIZE + LS_STREAM_TRANSPORT_TRAILER_SIZE;
    if ((size_t)encoded_length != length - overhead) {
        return LS_ECORRUPT;
    }
    const uint8_t *envelope = data + LS_STREAM_TRANSPORT_HEADER_SIZE;
    uint32_t crc = read_u32(envelope + encoded_length);
    if (crc != ls_crc32(envelope, encoded_length)) {
        return LS_ECORRUPT;
    }
    ls_result_t result = ls_envelope_validate(envelope, encoded_length, 0);
    if (result != LS_OK) {
        return result;
    }
    out->envelope = envelope;
    out->envelope_length = encoded_length;
    out->envelope_crc = crc;
    return LS_OK;
}
