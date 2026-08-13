#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"
#include "../src/core/internal.h"

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "stream edges failed: %s:%d\n", #x, __LINE__);                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct {
    unsigned calls;
    ls_result_t first_result;
} writer_mock_t;

static ls_result_t write_mock(void *context, const uint8_t *data, size_t length) {
    writer_mock_t *mock = (writer_mock_t *)context;
    CHECK(data != NULL && length != 0u);
    ++mock->calls;
    if (mock->calls == 1u && mock->first_result != LS_OK)
        return mock->first_result;
    return LS_OK;
}

static ls_result_t ack_ok(void *context, uint32_t event_id, uint32_t timeout_ms) {
    (void)context;
    return event_id != 0u && timeout_ms == 9u ? LS_OK : LS_EINVAL;
}

static int make_envelope(uint8_t *out, size_t capacity, size_t *length) {
    ls_event_t event = {.type = LS_EVENT_MESSAGE,
                        .priority = LS_PRIORITY_WARNING,
                        .timestamp_ms = 1u,
                        .domain = "stream",
                        .severity = LS_SEVERITY_WARNING,
                        .message = "edge",
                        .capture_level = LS_CAPTURE_METADATA};
    return ls_envelope_encode(&event, out, capacity, 1u, length) == LS_OK ? 0 : 1;
}

int main(void) {
    const ls_identity_t identity = {
        .project_id = "stream", .device_id = "host", .firmware_build_id = "strmedge"};
    ls_config_t config = {.identity = &identity};
    CHECK(ls_init(&config) == LS_OK);

    uint8_t envelope[LS_MAX_EVENT_SIZE];
    size_t envelope_length = 0u;
    CHECK(make_envelope(envelope, sizeof envelope, &envelope_length) == 0);
    ls_envelope_info_t info;
    CHECK(ls_envelope_validate(envelope, envelope_length, &info) == LS_OK);

    CHECK(ls_stream_transport_init(NULL) == LS_EINVAL);
    CHECK(ls_stream_transport_reset(NULL) == LS_EINVAL);
    CHECK(ls_stream_transport_max_payload(NULL) == 0u);

    writer_mock_t mock = {0};
    ls_stream_transport_t stream = {.context = &mock, .write = write_mock, .ack_timeout_ms = 9u};
    CHECK(ls_stream_transport_init(&stream) == LS_OK);
    CHECK(ls_stream_transport_max_payload(&stream) == LS_MAX_EVENT_SIZE);
    stream.maximum_envelope = envelope_length;
    CHECK(ls_stream_transport_max_payload(&stream) == envelope_length);

    CHECK(ls_stream_transport_send(NULL, envelope, envelope_length) == LS_EINVAL);
    ls_stream_transport_t no_write = {0};
    CHECK(ls_stream_transport_init(&no_write) == LS_OK);
    CHECK(ls_stream_transport_send(&no_write, envelope, envelope_length) == LS_EINVAL);
    CHECK(ls_stream_transport_send(&stream, NULL, envelope_length) == LS_EINVAL);
    stream.maximum_envelope = envelope_length - 1u;
    CHECK(ls_stream_transport_send(&stream, envelope, envelope_length) == LS_ENOSPACE);
    stream.maximum_envelope = 0u;

    stream.sending = true;
    CHECK(ls_stream_transport_send(&stream, envelope, envelope_length) == LS_EBUSY);
    CHECK(ls_stream_transport_reset(&stream) == LS_EBUSY);
    stream.sending = false;

    stream.state_magic = 0u;
    CHECK(ls_stream_transport_reset(&stream) == LS_EINVAL);
    stream.state_magic = LS_STREAM_TRANSPORT_STATE_MAGIC;

    stream.pending = true;
    stream.pending_event_id = info.event_id + 1u;
    stream.pending_length = envelope_length;
    stream.pending_crc = ls_crc32(envelope, envelope_length);
    CHECK(ls_stream_transport_send(&stream, envelope, envelope_length) == LS_EBUSY);
    stream.pending_event_id = info.event_id;
    stream.pending_length = envelope_length + 1u;
    CHECK(ls_stream_transport_send(&stream, envelope, envelope_length) == LS_EBUSY);
    stream.pending_length = envelope_length;
    stream.pending_crc ^= 1u;
    CHECK(ls_stream_transport_send(&stream, envelope, envelope_length) == LS_EBUSY);
    CHECK(ls_stream_transport_reset(&stream) == LS_OK);
    CHECK(!stream.pending && !stream.awaiting_ack);

    /* Retryable write errors preserve the exact pending envelope for resumption. */
    mock.calls = 0u;
    mock.first_result = LS_EAGAIN;
    CHECK(ls_stream_transport_send(&stream, envelope, envelope_length) == LS_EAGAIN);
    CHECK(stream.pending && !stream.awaiting_ack && stream.header_offset == 0u);
    mock.first_result = LS_OK;
    CHECK(ls_stream_transport_send(&stream, envelope, envelope_length) == LS_OK);
    CHECK(!stream.pending);

    /* Non-retryable failures must clear pending state to avoid wedging the stream. */
    mock.calls = 0u;
    mock.first_result = LS_EIO;
    CHECK(ls_stream_transport_send(&stream, envelope, envelope_length) == LS_EIO);
    CHECK(!stream.pending && !stream.sending);

    mock.calls = 0u;
    mock.first_result = LS_OK;
    stream.wait_ack = ack_ok;
    CHECK(ls_stream_transport_send(&stream, envelope, envelope_length) == LS_OK);
    CHECK(!stream.pending && !stream.awaiting_ack);

    uint8_t ack[LS_LSAK_SIZE] = {'L', 'S', 'A', 'K', LS_LSAK_VERSION, LS_LSAK_ACK_DUPLICATE, 0, 0,
                                 1,   2,   3,   4};
    ls_lsak_t parsed;
    CHECK(ls_lsak_parse(ack, sizeof ack, &parsed) == LS_OK);
    CHECK(ls_lsak_is_success(LS_LSAK_ACK_STORED));
    CHECK(ls_lsak_is_success(LS_LSAK_ACK_DUPLICATE));
    CHECK(!ls_lsak_is_success(LS_LSAK_NACK_BUSY));
    for (size_t i = 0; i < 4u; ++i) {
        uint8_t saved = ack[i];
        ack[i] ^= 0x20u;
        CHECK(ls_lsak_parse(ack, sizeof ack, &parsed) == LS_ECORRUPT);
        ack[i] = saved;
    }

    /* Frame parser: exercise each independent header discriminator and defaults. */
    uint8_t frame[LS_MAX_EVENT_SIZE + LS_STREAM_TRANSPORT_HEADER_SIZE +
                  LS_STREAM_TRANSPORT_TRAILER_SIZE];
    size_t frame_length = 0u;
    mock.calls = 0u;
    stream.wait_ack = NULL;
    stream.context = &mock;
    /* Capture the exact stream bytes using a local all-at-once sink. */
    frame[0] = 'L';
    frame[1] = 'S';
    frame[2] = LS_STREAM_TRANSPORT_VERSION;
    frame[3] = 0u;
    uint32_t n = (uint32_t)envelope_length;
    frame[4] = (uint8_t)n;
    frame[5] = (uint8_t)(n >> 8);
    frame[6] = (uint8_t)(n >> 16);
    frame[7] = (uint8_t)(n >> 24);
    memcpy(frame + LS_STREAM_TRANSPORT_HEADER_SIZE, envelope, envelope_length);
    uint32_t crc = ls_crc32(envelope, envelope_length);
    size_t t = LS_STREAM_TRANSPORT_HEADER_SIZE + envelope_length;
    frame[t] = (uint8_t)crc;
    frame[t + 1] = (uint8_t)(crc >> 8);
    frame[t + 2] = (uint8_t)(crc >> 16);
    frame[t + 3] = (uint8_t)(crc >> 24);
    frame_length = t + 4u;
    ls_stream_frame_t parsed_frame;
    CHECK(ls_stream_frame_parse(frame, frame_length, 0u, &parsed_frame) == LS_OK);
    frame[0] = 'X';
    CHECK(ls_stream_frame_parse(frame, frame_length, 0u, &parsed_frame) == LS_ECORRUPT);
    frame[0] = 'L';
    frame[1] = 'X';
    CHECK(ls_stream_frame_parse(frame, frame_length, 0u, &parsed_frame) == LS_ECORRUPT);
    frame[1] = 'S';
    frame[2] = 2u;
    CHECK(ls_stream_frame_parse(frame, frame_length, 0u, &parsed_frame) == LS_ENOTSUP);
    frame[2] = LS_STREAM_TRANSPORT_VERSION;
    frame[3] = 1u;
    CHECK(ls_stream_frame_parse(frame, frame_length, 0u, &parsed_frame) == LS_ENOTSUP);
    frame[3] = 0u;

    return 0;
}
