#include "../core/internal.h"
#include "laststate/envelope.h"
#include "laststate/stream_transport.h"

static void write_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
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
