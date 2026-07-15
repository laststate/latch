#ifndef LASTSTATE_MBEDTLS_TRANSPORT_H
#define LASTSTATE_MBEDTLS_TRANSPORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <mbedtls/ssl.h>
#include "laststate/transport.h"
typedef struct {
    ls_transport_backend_t backend;
    mbedtls_ssl_context *ssl;
    size_t configured_mtu, pending_length, pending_offset;
    uint32_t pending_crc;
    bool require_verified_peer;
} ls_mbedtls_transport_t;
ls_result_t ls_mbedtls_transport_init(ls_mbedtls_transport_t *transport, mbedtls_ssl_context *ssl,
                                      size_t maximum_payload, bool require_verified_peer);
#endif
