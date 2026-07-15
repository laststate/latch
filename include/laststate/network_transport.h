#ifndef LASTSTATE_NETWORK_TRANSPORT_H
#define LASTSTATE_NETWORK_TRANSPORT_H
#include <stddef.h>
#include <stdint.h>
#include "transport.h"
typedef ls_result_t (*ls_http_post_fn)(void *context, const char *endpoint,
                                       const char *content_type, const uint8_t *body, size_t length,
                                       uint16_t *status);
/* An ACK-capable HTTP backend must return success only after its endpoint has
   accepted the LEP
 * envelope for durable processing. */
typedef struct {
    void *context;
    const char *endpoint;
    ls_http_post_fn post;
    size_t maximum_payload;
} ls_http_transport_t;
ls_result_t ls_http_transport_send(void *context, const uint8_t *data, size_t length);
size_t ls_http_transport_max_payload(void *context);
typedef ls_result_t (*ls_mqtt_publish_fn)(void *context, const char *topic, const uint8_t *payload,
                                          size_t length, uint8_t qos, bool retain);
/* QoS 1 or 2 can be advertised as a durable ACK only when publish waits for
   the broker
 * acknowledgement and the receiver deduplicates event IDs. */
typedef struct {
    void *context;
    const char *topic;
    ls_mqtt_publish_fn publish;
    size_t maximum_payload;
    uint8_t qos;
} ls_mqtt_transport_t;
ls_result_t ls_mqtt_transport_send(void *context, const uint8_t *data, size_t length);
size_t ls_mqtt_transport_max_payload(void *context);
ls_result_t ls_incident_beacon_encode(const uint8_t *envelope, size_t envelope_length,
                                      uint32_t device_hash, uint16_t battery_mv,
                                      int16_t temperature_c, uint8_t *output, size_t capacity,
                                      size_t *written);
#endif
