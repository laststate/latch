#ifndef LASTSTATE_TRANSPORT_H
#define LASTSTATE_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "event.h"
enum {
    LS_TRANSPORT_ACK = 1u,
    LS_TRANSPORT_FRAGMENT = 2u,
    LS_TRANSPORT_STREAM = 4u,
    LS_TRANSPORT_SECURE = 8u,
    LS_TRANSPORT_EXPENSIVE = 16u,
    LS_TRANSPORT_LOW_POWER = 32u
};

#define LS_TRANSPORT_DURABLE_ACK LS_TRANSPORT_ACK

typedef struct {
    uint32_t event_id;
    uint32_t envelope_crc;
    uint32_t total_length;
    uint32_t offset;
    uint32_t fragment_capacity;
    uint32_t fragment_crc;
    uint16_t fragment_index;
    uint16_t fragment_count;
    const uint8_t *data;
    size_t length;
} ls_transport_fragment_t;

typedef ls_result_t (*ls_transport_send_fragment_v2_fn)(void *context,
                                                        const ls_transport_fragment_t *fragment);
typedef bool (*ls_transport_retry_fn)(void *context, uint8_t attempt, ls_result_t result);

typedef struct ls_transport_backend {
    const char *name;
    int priority;
    bool (*available)(void *context);
    ls_result_t (*send)(void *context, const uint8_t *data, size_t length);
    ls_result_t (*send_fragment)(void *context, uint32_t event_id, uint16_t fragment_index,
                                 uint16_t fragment_count, const uint8_t *data, size_t length,
                                 uint32_t crc);
    size_t (*max_payload)(void *context);
    void *context;
    uint32_t capabilities;
    uint16_t energy_cost;
    uint16_t monetary_cost;
    ls_transport_send_fragment_v2_fn send_fragment_v2;
    ls_transport_retry_fn retry;
    uint8_t retry_limit;
} ls_transport_backend_t;

typedef struct {
    uint8_t *data;
    size_t capacity;
    uint8_t *received;
    size_t received_capacity;
    uint32_t event_id;
    uint32_t envelope_crc;
    uint32_t total_length;
    uint32_t fragment_capacity;
    uint16_t fragment_count;
    uint16_t received_count;
    bool active;
    bool complete;
} ls_transport_reassembly_t;

/* A successful send on a durable-ACK transport means the receiver has made
   the envelope durable. Retry callbacks are called only for EAGAIN or EBUSY. */
void ls_transport_register(ls_transport_backend_t *transport);
void ls_transport_clear(void);
ls_transport_backend_t *ls_transport_select(ls_priority_t priority, size_t event_size);
bool ls_transport_can_send(const ls_transport_backend_t *transport, size_t event_size);
bool ls_transport_result_is_retryable(ls_result_t result);
ls_result_t ls_transport_send(ls_transport_backend_t *transport, uint32_t event_id,
                              const uint8_t *data, size_t length);

/* Reassembly accepts v2 fragments in any order. Accepted incomplete fragments
   return LS_OK with envelope_length set to zero. The caller retains the data
   and bitmap buffers, and resets the context after consuming a complete frame. */
void ls_transport_reassembly_init(ls_transport_reassembly_t *reassembly, uint8_t *data,
                                  size_t capacity, uint8_t *received, size_t received_capacity);
void ls_transport_reassembly_reset(ls_transport_reassembly_t *reassembly);
ls_result_t ls_transport_reassembly_push(ls_transport_reassembly_t *reassembly,
                                         const ls_transport_fragment_t *fragment,
                                         size_t *envelope_length);
#endif
