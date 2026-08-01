#include "mbedtls_transport.h"
#include <mbedtls/x509.h>
static uint32_t fingerprint(const uint8_t *data, size_t length) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((uint32_t) - (int32_t)(crc & 1u) & 0xedb88320u);
    }
    return ~crc;
}
static bool available(void *context) {
    ls_mbedtls_transport_t *transport = (ls_mbedtls_transport_t *)context;
    if (!transport || !transport->ssl || !mbedtls_ssl_get_ciphersuite(transport->ssl))
        return false;
    return !transport->require_verified_peer || mbedtls_ssl_get_verify_result(transport->ssl) == 0;
}
static size_t maximum_payload(void *context) {
    ls_mbedtls_transport_t *transport = (ls_mbedtls_transport_t *)context;
    return transport->configured_mtu;
}
static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    ls_mbedtls_transport_t *transport = (ls_mbedtls_transport_t *)context;
    if (!transport || !data || !length || !available(context))
        return LS_EINVAL;
    uint32_t crc = fingerprint(data, length);
    if (transport->pending_length) {
        if (transport->pending_length != length || transport->pending_crc != crc)
            return LS_EBUSY;
    } else {
        transport->pending_length = length;
        transport->pending_crc = crc;
        transport->pending_offset = 0;
    }
    while (transport->pending_offset < length) {
        int written = mbedtls_ssl_write(transport->ssl, data + transport->pending_offset,
                                        length - transport->pending_offset);
        if (written == MBEDTLS_ERR_SSL_WANT_READ || written == MBEDTLS_ERR_SSL_WANT_WRITE)
            return LS_EAGAIN;
        if (written <= 0) {
            transport->pending_length = 0;
            transport->pending_offset = 0;
            return LS_EIO;
        }
        transport->pending_offset += (size_t)written;
    }
    transport->pending_length = 0;
    transport->pending_offset = 0;
    transport->pending_crc = 0;
    return LS_OK;
}
ls_result_t ls_mbedtls_transport_init(ls_mbedtls_transport_t *transport, mbedtls_ssl_context *ssl,
                                      size_t maximum_payload_size, bool require_verified_peer) {
    if (!transport || !ssl || !maximum_payload_size)
        return LS_EINVAL;
    *transport = (ls_mbedtls_transport_t){0};
    transport->ssl = ssl;
    transport->configured_mtu = maximum_payload_size;
    transport->require_verified_peer = require_verified_peer;
    transport->backend =
        (ls_transport_backend_t){.name = "mbedtls",
                                 .priority = 10,
                                 .available = available,
                                 .send = send_data,
                                 .max_payload = maximum_payload,
                                 .context = transport,
                                 .capabilities = require_verified_peer ? LS_TRANSPORT_SECURE : 0u};
    return LS_OK;
}
