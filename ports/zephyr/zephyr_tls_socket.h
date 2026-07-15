#ifndef LASTSTATE_ZEPHYR_TLS_SOCKET_H
#define LASTSTATE_ZEPHYR_TLS_SOCKET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "laststate/transport.h"
typedef struct {
    ls_transport_backend_t backend;
    const char *hostname, *port;
    int socket_fd, security_tag;
    size_t maximum_payload;
    bool verify_peer, connected;
} ls_zephyr_tls_socket_transport_t;
ls_result_t ls_zephyr_tls_credential_add(int security_tag, const uint8_t *certificate,
                                         size_t length);
ls_result_t ls_zephyr_tls_socket_connect(ls_zephyr_tls_socket_transport_t *transport,
                                         const char *hostname, const char *port, int security_tag,
                                         bool verify_peer, size_t maximum_payload);
void ls_zephyr_tls_socket_close(ls_zephyr_tls_socket_transport_t *transport);
#endif
