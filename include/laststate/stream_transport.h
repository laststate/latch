#ifndef LASTSTATE_STREAM_TRANSPORT_H
#define LASTSTATE_STREAM_TRANSPORT_H
#include <stddef.h>
#include <stdint.h>
#include "transport.h"
typedef ls_result_t (*ls_stream_write_fn)(void *context,const uint8_t *data,size_t length);
typedef ls_result_t (*ls_stream_ack_fn)(void *context,uint32_t event_id,uint32_t timeout_ms);
typedef struct {void *context;ls_stream_write_fn write;ls_stream_ack_fn wait_ack;size_t maximum_envelope;uint32_t ack_timeout_ms;} ls_stream_transport_t;
ls_result_t ls_stream_transport_send(void *context,const uint8_t *data,size_t length);
size_t ls_stream_transport_max_payload(void *context);
#endif
