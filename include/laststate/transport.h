#ifndef LASTSTATE_TRANSPORT_H
#define LASTSTATE_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "event.h"
enum {
    LS_TRANSPORT_ACK=1u, LS_TRANSPORT_FRAGMENT=2u, LS_TRANSPORT_STREAM=4u,
    LS_TRANSPORT_SECURE=8u, LS_TRANSPORT_EXPENSIVE=16u, LS_TRANSPORT_LOW_POWER=32u
};
typedef struct ls_transport_backend {
    const char *name;
    int priority;
    bool (*available)(void *context);
    ls_result_t (*send)(void *context, const uint8_t *data, size_t length);
    ls_result_t (*send_fragment)(void *context, uint32_t event_id, uint16_t fragment_index, uint16_t fragment_count, const uint8_t *data, size_t length, uint32_t crc);
    size_t (*max_payload)(void *context);
    void *context;
    uint32_t capabilities;
    uint16_t energy_cost;
    uint16_t monetary_cost;
} ls_transport_backend_t;
void ls_transport_register(ls_transport_backend_t *transport);
void ls_transport_clear(void);
ls_transport_backend_t *ls_transport_select(ls_priority_t priority, size_t event_size);
ls_result_t ls_transport_send(ls_transport_backend_t *transport, uint32_t event_id, const uint8_t *data, size_t length);
#endif
