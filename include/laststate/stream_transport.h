#ifndef LASTSTATE_STREAM_TRANSPORT_H
#define LASTSTATE_STREAM_TRANSPORT_H
#include <stddef.h>
#include <stdint.h>
#include "transport.h"
#define LS_STREAM_TRANSPORT_VERSION 1u
#define LS_STREAM_TRANSPORT_HEADER_SIZE 8u
#define LS_STREAM_TRANSPORT_TRAILER_SIZE 4u
#define LS_STREAM_TRANSPORT_STATE_MAGIC 0x4c535354u

/* LSAK is a 12-byte durable-delivery control message ("LSAK" magic). */
#define LS_LSAK_SIZE 12u
#define LS_LSAK_VERSION 1u
enum {
    LS_LSAK_ACK_STORED = 1,
    LS_LSAK_ACK_DUPLICATE = 2,
    LS_LSAK_NACK_CORRUPT = 3,
    LS_LSAK_NACK_UNSUPPORTED = 4,
    LS_LSAK_NACK_BUSY = 5,
    LS_LSAK_NACK_TOO_LARGE = 6,
    LS_LSAK_NACK_UNAUTHORIZED = 7,
    LS_LSAK_NACK_INTERNAL = 8
};
typedef struct {
    uint8_t version;
    uint8_t status;
    uint32_t event_id;
} ls_lsak_t;

typedef struct {
    const uint8_t *envelope;
    size_t envelope_length;
    uint32_t envelope_crc;
} ls_stream_frame_t;

typedef ls_result_t (*ls_stream_write_fn)(void *context, const uint8_t *data, size_t length);
typedef ls_result_t (*ls_stream_ack_fn)(void *context, uint32_t event_id, uint32_t timeout_ms);
typedef struct {
    void *context;
    ls_stream_write_fn write;
    ls_stream_ack_fn wait_ack;
    size_t maximum_envelope;
    uint32_t ack_timeout_ms;
    uint32_t pending_event_id;
    uint32_t pending_crc;
    size_t pending_length;
    size_t header_offset;
    size_t payload_offset;
    size_t trailer_offset;
    bool pending;
    bool awaiting_ack;
    bool sending;
    uint32_t state_magic;
} ls_stream_transport_t;

/* Set the legacy configuration fields, then initialize before first use. write
   must either consume its complete buffer or return an error. A pending frame
   is resumed only with the same LEP event after EAGAIN or EBUSY; an ACK retry
   retransmits the complete frame, so receivers must deduplicate event IDs. */
ls_result_t ls_stream_transport_init(ls_stream_transport_t *stream);
ls_result_t ls_stream_transport_send(void *context, const uint8_t *data, size_t length);
size_t ls_stream_transport_max_payload(void *context);
ls_result_t ls_stream_transport_reset(ls_stream_transport_t *stream);

ls_result_t ls_lsak_parse(const uint8_t *data, size_t length, ls_lsak_t *out);
bool ls_lsak_is_success(uint8_t status);

/* Validate one complete LS frame, including its bounded length, outer CRC and
   embedded LEP
 * envelope. maximum_envelope=0 selects LS_MAX_EVENT_SIZE. */
ls_result_t ls_stream_frame_parse(const uint8_t *data, size_t length, size_t maximum_envelope,
                                  ls_stream_frame_t *out);
#endif
