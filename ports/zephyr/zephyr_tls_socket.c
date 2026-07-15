#include "zephyr_tls_socket.h"
#if defined(__ZEPHYR__) && defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#include <errno.h>
#include <string.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
static bool available(void *context) {
    ls_zephyr_tls_socket_transport_t *transport = (ls_zephyr_tls_socket_transport_t *)context;
    return transport && transport->connected && transport->socket_fd >= 0;
}
static size_t maximum_payload(void *context) {
    return ((ls_zephyr_tls_socket_transport_t *)context)->maximum_payload;
}
static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    ls_zephyr_tls_socket_transport_t *transport = (ls_zephyr_tls_socket_transport_t *)context;
    if (!available(context) || (!data && length))
        return LS_EINVAL;
    size_t sent = 0;
    while (sent < length) {
        ssize_t result = zsock_send(transport->socket_fd, data + sent, length - sent, 0);
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return LS_EAGAIN;
        if (result <= 0) {
            transport->connected = false;
            return LS_EIO;
        }
        sent += (size_t)result;
    }
    return LS_OK;
}
ls_result_t ls_zephyr_tls_credential_add(int security_tag, const uint8_t *certificate,
                                         size_t length) {
    if (security_tag < 0 || !certificate || !length)
        return LS_EINVAL;
    int result =
        tls_credential_add(security_tag, TLS_CREDENTIAL_CA_CERTIFICATE, certificate, length);
    return result == 0 || result == -EEXIST ? LS_OK : LS_EIO;
}
ls_result_t ls_zephyr_tls_socket_connect(ls_zephyr_tls_socket_transport_t *transport,
                                         const char *hostname, const char *port, int security_tag,
                                         bool verify_peer, size_t maximum_payload_size) {
    if (!transport || !hostname || !port || security_tag < 0 || !maximum_payload_size)
        return LS_EINVAL;
    *transport = (ls_zephyr_tls_socket_transport_t){0};
    transport->socket_fd = -1;
    transport->hostname = hostname;
    transport->port = port;
    transport->security_tag = security_tag;
    transport->verify_peer = verify_peer;
    transport->maximum_payload = maximum_payload_size;
    struct zsock_addrinfo hints = {0}, *addresses = 0;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int result = zsock_getaddrinfo(hostname, port, &hints, &addresses);
    if (result != 0 || !addresses)
        return LS_EIO;
    transport->socket_fd = zsock_socket(addresses->ai_family, SOCK_STREAM, IPPROTO_TLS_1_2);
    if (transport->socket_fd < 0) {
        zsock_freeaddrinfo(addresses);
        return LS_EIO;
    }
    sec_tag_t tags[] = {(sec_tag_t)security_tag};
    int verify = verify_peer ? TLS_PEER_VERIFY_REQUIRED : TLS_PEER_VERIFY_NONE;
    result = zsock_setsockopt(transport->socket_fd, SOL_TLS, TLS_SEC_TAG_LIST, tags, sizeof tags);
    if (result == 0)
        result = zsock_setsockopt(transport->socket_fd, SOL_TLS, TLS_HOSTNAME, hostname,
                                  strlen(hostname) + 1u);
    if (result == 0)
        result = zsock_setsockopt(transport->socket_fd, SOL_TLS, TLS_PEER_VERIFY, &verify,
                                  sizeof verify);
    if (result == 0)
        result = zsock_connect(transport->socket_fd, addresses->ai_addr, addresses->ai_addrlen);
    zsock_freeaddrinfo(addresses);
    if (result < 0) {
        zsock_close(transport->socket_fd);
        transport->socket_fd = -1;
        return LS_EIO;
    }
    transport->connected = true;
    transport->backend = (ls_transport_backend_t){
        .name = "zephyr-tls-cellular-socket",
        .priority = 5,
        .available = available,
        .send = send_data,
        .max_payload = maximum_payload,
        .context = transport,
        .capabilities = (verify_peer ? LS_TRANSPORT_SECURE : 0u) | LS_TRANSPORT_EXPENSIVE};
    return LS_OK;
}
void ls_zephyr_tls_socket_close(ls_zephyr_tls_socket_transport_t *transport) {
    if (transport && transport->socket_fd >= 0)
        zsock_close(transport->socket_fd);
    if (transport) {
        transport->socket_fd = -1;
        transport->connected = false;
    }
}
#else
ls_result_t ls_zephyr_tls_credential_add(int security_tag, const uint8_t *certificate,
                                         size_t length) {
    (void)security_tag;
    (void)certificate;
    (void)length;
    return LS_ENOTSUP;
}
ls_result_t ls_zephyr_tls_socket_connect(ls_zephyr_tls_socket_transport_t *transport,
                                         const char *hostname, const char *port, int security_tag,
                                         bool verify_peer, size_t maximum_payload) {
    (void)transport;
    (void)hostname;
    (void)port;
    (void)security_tag;
    (void)verify_peer;
    (void)maximum_payload;
    return LS_ENOTSUP;
}
void ls_zephyr_tls_socket_close(ls_zephyr_tls_socket_transport_t *transport) {
    (void)transport;
}
#endif
