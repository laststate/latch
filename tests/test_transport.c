#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "transport check failed: %s:%d\n", #x, __LINE__);                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint8_t retained[50000], envelope[LS_MAX_EVENT_SIZE], stream_bytes[LS_MAX_EVENT_SIZE + 32],
    retry_stream_bytes[LS_MAX_EVENT_SIZE * 2u + 32u];
static size_t envelope_length, stream_length, retry_stream_length;
static uint32_t ack_event;
static unsigned mqtt_calls, http_calls, retry_ack_calls;
static uint32_t test_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xffffffffu;
    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (unsigned bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1u) ^ (0xedb88320u & (uint32_t) - (int32_t)(crc & 1u));
    }
    return ~crc;
}
static bool yes(void *x) {
    (void)x;
    return true;
}
static size_t large(void *x) {
    (void)x;
    return LS_MAX_EVENT_SIZE;
}
static ls_result_t capture_send(void *x, const uint8_t *data, size_t length) {
    (void)x;
    memcpy(envelope, data, length);
    envelope_length = length;
    return LS_OK;
}
static ls_result_t write_stream(void *x, const uint8_t *data, size_t length) {
    (void)x;
    if (length > sizeof stream_bytes - stream_length)
        return LS_ENOSPACE;
    memcpy(stream_bytes + stream_length, data, length);
    stream_length += length;
    return LS_OK;
}
static ls_result_t wait_ack(void *x, uint32_t event_id, uint32_t timeout) {
    (void)x;
    if (timeout != 100)
        return LS_EINVAL;
    ack_event = event_id;
    return LS_OK;
}
static ls_result_t write_retry_stream(void *x, const uint8_t *data, size_t length) {
    (void)x;
    if (length > sizeof retry_stream_bytes - retry_stream_length)
        return LS_ENOSPACE;
    memcpy(retry_stream_bytes + retry_stream_length, data, length);
    retry_stream_length += length;
    return LS_OK;
}
static ls_result_t wait_ack_after_retry(void *x, uint32_t event_id, uint32_t timeout) {
    (void)x;
    (void)event_id;
    if (timeout != 100)
        return LS_EINVAL;
    return retry_ack_calls++ == 0u ? LS_EAGAIN : LS_OK;
}
static ls_result_t post(void *x, const char *endpoint, const char *type, const uint8_t *body,
                        size_t length, uint16_t *status) {
    (void)x;
    if (strcmp(endpoint, "https://ingest") || strcmp(type, "application/vnd.laststate.lep") ||
        !body || !length)
        return LS_EINVAL;
    http_calls++;
    *status = 204;
    return LS_OK;
}
static ls_result_t publish(void *x, const char *topic, const uint8_t *payload, size_t length,
                           uint8_t qos, bool retain_flag) {
    (void)x;
    if (strcmp(topic, "latch/events") || !payload || !length || qos != 1 || retain_flag)
        return LS_EINVAL;
    mqtt_calls++;
    return LS_OK;
}
int main(void) {
    ls_identity_t identity = {
        .project_id = "transport", .device_id = "one", .firmware_build_id = "trans001"};
    ls_config_t config = {.identity = &identity};
    ls_memory_storage_t memory = {retained, sizeof retained};
    ls_storage_backend_t storage = {.name = "memory",
                                    .context = &memory,
                                    .capacity = sizeof retained,
                                    .read = ls_memory_storage_read,
                                    .write = ls_memory_storage_write,
                                    .erase = ls_memory_storage_erase};
    ls_transport_backend_t sink = {.name = "sink",
                                   .priority = 1,
                                   .available = yes,
                                   .send = capture_send,
                                   .max_payload = large};
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&sink);
    CHECK(ls_boot() == LS_OK);
    ls_capture_message("transport", LS_SEVERITY_ERROR);
    CHECK(ls_flush() == LS_OK && envelope_length > 0);
    ls_envelope_info_t info;
    CHECK(ls_envelope_validate(envelope, envelope_length, &info) == LS_OK);
    ls_stream_transport_t uninitialized = {.write = write_stream,
                                           .maximum_envelope = LS_MAX_EVENT_SIZE};
    CHECK(ls_stream_transport_send(&uninitialized, envelope, envelope_length) == LS_EINVAL);
    ls_stream_transport_t stream = {.write = write_stream,
                                    .wait_ack = wait_ack,
                                    .maximum_envelope = LS_MAX_EVENT_SIZE,
                                    .ack_timeout_ms = 100};
    CHECK(ls_stream_transport_init(&stream) == LS_OK);
    CHECK(ls_stream_transport_send(&stream, envelope, envelope_length) == LS_OK);
    CHECK(stream_length == envelope_length + 12 && stream_bytes[0] == 'L' &&
          stream_bytes[1] == 'S' && ack_event == info.event_id);
    ls_stream_frame_t parsed_frame;
    CHECK(ls_stream_frame_parse(stream_bytes, stream_length, LS_MAX_EVENT_SIZE, &parsed_frame) ==
          LS_OK);
    CHECK(ls_stream_frame_parse(stream_bytes, stream_length, 0, &parsed_frame) == LS_OK);
    CHECK(ls_stream_frame_parse(0, stream_length, LS_MAX_EVENT_SIZE, &parsed_frame) == LS_EINVAL);
    CHECK(ls_stream_frame_parse(stream_bytes, stream_length, LS_MAX_EVENT_SIZE, 0) == LS_EINVAL);
    CHECK(ls_stream_frame_parse(stream_bytes, 4, LS_MAX_EVENT_SIZE, &parsed_frame) == LS_EINVAL);
    CHECK(parsed_frame.envelope_length == envelope_length &&
          !memcmp(parsed_frame.envelope, envelope, envelope_length));
    CHECK(ls_stream_frame_parse(stream_bytes, stream_length, envelope_length - 1u, &parsed_frame) ==
          LS_ENOSPACE);
    stream_bytes[stream_length - 1u] ^= 1u;
    CHECK(ls_stream_frame_parse(stream_bytes, stream_length, LS_MAX_EVENT_SIZE, &parsed_frame) ==
          LS_ECORRUPT);
    stream_bytes[stream_length - 1u] ^= 1u;
    stream_bytes[0] = 'X';
    CHECK(ls_stream_frame_parse(stream_bytes, stream_length, LS_MAX_EVENT_SIZE, &parsed_frame) ==
          LS_ECORRUPT);
    stream_bytes[0] = 'L';
    stream_bytes[1] = 'X';
    CHECK(ls_stream_frame_parse(stream_bytes, stream_length, LS_MAX_EVENT_SIZE, &parsed_frame) ==
          LS_ECORRUPT);
    stream_bytes[1] = 'S';
    stream_bytes[2] = 0xffu;
    CHECK(ls_stream_frame_parse(stream_bytes, stream_length, LS_MAX_EVENT_SIZE, &parsed_frame) ==
          LS_ENOTSUP);
    stream_bytes[2] = LS_STREAM_TRANSPORT_VERSION;
    stream_bytes[3] = 1u;
    CHECK(ls_stream_frame_parse(stream_bytes, stream_length, LS_MAX_EVENT_SIZE, &parsed_frame) ==
          LS_ENOTSUP);
    stream_bytes[3] = 0u;
    CHECK(ls_stream_frame_parse(stream_bytes, stream_length - 1u, LS_MAX_EVENT_SIZE,
                                &parsed_frame) == LS_ECORRUPT);
    stream_bytes[LS_STREAM_TRANSPORT_HEADER_SIZE] ^= 1u;
    uint32_t damaged_crc =
        test_crc32(stream_bytes + LS_STREAM_TRANSPORT_HEADER_SIZE, envelope_length);
    for (unsigned index = 0; index < 4u; ++index)
        stream_bytes[LS_STREAM_TRANSPORT_HEADER_SIZE + envelope_length + index] =
            (uint8_t)(damaged_crc >> (index * 8u));
    CHECK(ls_stream_frame_parse(stream_bytes, stream_length, LS_MAX_EVENT_SIZE, &parsed_frame) ==
          LS_ECORRUPT);
    stream_bytes[LS_STREAM_TRANSPORT_HEADER_SIZE] ^= 1u;
    uint32_t restored_crc =
        test_crc32(stream_bytes + LS_STREAM_TRANSPORT_HEADER_SIZE, envelope_length);
    for (unsigned index = 0; index < 4u; ++index)
        stream_bytes[LS_STREAM_TRANSPORT_HEADER_SIZE + envelope_length + index] =
            (uint8_t)(restored_crc >> (index * 8u));
    ls_stream_transport_t retry_stream = {.write = write_retry_stream,
                                          .wait_ack = wait_ack_after_retry,
                                          .maximum_envelope = LS_MAX_EVENT_SIZE,
                                          .ack_timeout_ms = 100};
    CHECK(ls_stream_transport_init(&retry_stream) == LS_OK);
    CHECK(ls_stream_transport_send(&retry_stream, envelope, envelope_length) == LS_EAGAIN);
    CHECK(retry_stream.pending && !retry_stream.awaiting_ack && retry_stream.header_offset == 0 &&
          retry_stream.payload_offset == 0 && retry_stream.trailer_offset == 0);
    CHECK(ls_stream_transport_send(&retry_stream, envelope, envelope_length) == LS_OK);
    CHECK(retry_ack_calls == 2 && retry_stream_length == (envelope_length + 12u) * 2u &&
          retry_stream_bytes[0] == 'L' && retry_stream_bytes[envelope_length + 12u] == 'L');
    ls_http_transport_t http = {.endpoint = "https://ingest", .post = post};
    CHECK(ls_http_transport_send(&http, envelope, envelope_length) == LS_OK && http_calls == 1);
    ls_mqtt_transport_t mqtt = {.topic = "latch/events", .publish = publish, .qos = 1};
    CHECK(ls_mqtt_transport_send(&mqtt, envelope, envelope_length) == LS_OK && mqtt_calls == 1);
    uint8_t beacon[32];
    size_t beacon_length;
    CHECK(ls_incident_beacon_encode(envelope, envelope_length, 0x1234, 3700, 25, beacon,
                                    sizeof beacon, &beacon_length) == LS_OK &&
          beacon_length == 32);
    uint8_t ack[LS_LSAK_SIZE] = {'L', 'S', 'A', 'K', LS_LSAK_VERSION, LS_LSAK_ACK_STORED, 0, 0,
                                 9,   0,   0,   0};
    ls_lsak_t parsed_ack;
    CHECK(ls_lsak_parse(ack, sizeof ack, &parsed_ack) == LS_OK && parsed_ack.event_id == 9u);
    CHECK(ls_lsak_is_success(parsed_ack.status));
    CHECK(ls_lsak_parse(0, sizeof ack, &parsed_ack) == LS_EINVAL);
    CHECK(ls_lsak_parse(ack, sizeof ack, 0) == LS_EINVAL);
    CHECK(ls_lsak_parse(ack, sizeof ack - 1u, &parsed_ack) == LS_EINVAL);
    CHECK(ls_lsak_parse(ack, sizeof ack + 1u, &parsed_ack) == LS_EINVAL);
    ack[0] = 'X';
    CHECK(ls_lsak_parse(ack, sizeof ack, &parsed_ack) == LS_ECORRUPT);
    ack[0] = 'L';
    ack[7] = 1u;
    CHECK(ls_lsak_parse(ack, sizeof ack, &parsed_ack) == LS_ECORRUPT);
    ack[7] = 0u;
    ack[6] = 1u;
    CHECK(ls_lsak_parse(ack, sizeof ack, &parsed_ack) == LS_ECORRUPT);
    ack[6] = 0u;
    ack[5] = 0u;
    CHECK(ls_lsak_parse(ack, sizeof ack, &parsed_ack) == LS_ECORRUPT);
    ack[5] = 0xffu;
    CHECK(ls_lsak_parse(ack, sizeof ack, &parsed_ack) == LS_ECORRUPT);
    ack[5] = LS_LSAK_ACK_STORED;
    ack[4] = 0xffu;
    CHECK(ls_lsak_parse(ack, sizeof ack, &parsed_ack) == LS_ENOTSUP);
    return 0;
}
