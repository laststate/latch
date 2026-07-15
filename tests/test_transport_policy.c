#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "transport policy failed: %s:%d\n", #condition, __LINE__);             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct {
    bool online;
    size_t mtu;
} transport_context_t;

static unsigned fragments;
static unsigned retry_attempts;
static unsigned retry_callbacks;
static unsigned v2_count;
static ls_transport_fragment_t v2_fragments[3];
static ls_transport_reassembly_t *forward_reassembly;
static size_t forward_length;

static bool online(void *context) {
    return ((transport_context_t *)context)->online;
}

static ls_result_t send_ok(void *context, const uint8_t *data, size_t length) {
    (void)context;
    (void)data;
    (void)length;
    return LS_OK;
}

static ls_result_t send_retry(void *context, const uint8_t *data, size_t length) {
    (void)context;
    (void)data;
    (void)length;
    ++retry_attempts;
    return retry_attempts == 1u ? LS_EAGAIN : LS_OK;
}

static bool retry(void *context, uint8_t attempt, ls_result_t result) {
    (void)context;
    ++retry_callbacks;
    return attempt == 1u && result == LS_EAGAIN;
}

static size_t mtu(void *context) {
    return ((transport_context_t *)context)->mtu;
}

static ls_result_t fragment(void *context, uint32_t event_id, uint16_t index, uint16_t count,
                            const uint8_t *data, size_t length, uint32_t crc) {
    (void)context;
    (void)event_id;
    (void)index;
    (void)count;
    CHECK(data && length && crc);
    ++fragments;
    return LS_OK;
}

static ls_result_t fragment_v2(void *context, const ls_transport_fragment_t *fragment) {
    (void)context;
    if (!fragment || v2_count >= sizeof(v2_fragments) / sizeof(v2_fragments[0])) {
        return LS_EINVAL;
    }
    v2_fragments[v2_count++] = *fragment;
    if (forward_reassembly) {
        return ls_transport_reassembly_push(forward_reassembly, fragment, &forward_length);
    }
    return LS_OK;
}

int main(void) {
    ls_identity_t identity = {.firmware_build_id = "policy001"};
    ls_config_t config = {.identity = &identity};
    CHECK(ls_init(&config) == LS_OK);

    transport_context_t unavailable_context = {false, 100};
    transport_context_t small = {true, 4};
    transport_context_t large = {true, 100};
    ls_transport_backend_t unavailable = {
        .name = "off",
        .priority = 100,
        .available = online,
        .send = send_ok,
        .context = &unavailable_context,
    };
    ls_transport_backend_t cheap = {
        .name = "cheap",
        .priority = 5,
        .available = online,
        .send = send_ok,
        .max_payload = mtu,
        .context = &large,
        .energy_cost = 1,
    };
    ls_transport_backend_t costly = {
        .name = "costly",
        .priority = 5,
        .available = online,
        .send = send_ok,
        .max_payload = mtu,
        .context = &large,
        .energy_cost = 100,
        .capabilities = LS_TRANSPORT_DURABLE_ACK,
    };

    ls_transport_clear();
    ls_transport_register(&unavailable);
    ls_transport_register(&costly);
    ls_transport_register(&cheap);
    CHECK(ls_transport_select(LS_PRIORITY_DIAGNOSTIC, 10) == &cheap);
    CHECK(ls_transport_select(LS_PRIORITY_CRITICAL, 10) == &costly);
    CHECK(!ls_transport_can_send(&unavailable, 10));

    ls_transport_clear();
    ls_transport_backend_t fragmented = {
        .name = "fragment",
        .priority = 1,
        .available = online,
        .send = send_ok,
        .send_fragment = fragment,
        .max_payload = mtu,
        .context = &small,
        .capabilities = LS_TRANSPORT_FRAGMENT,
    };
    ls_transport_register(&fragmented);
    uint8_t data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    CHECK(ls_transport_send(&fragmented, 9, data, sizeof(data)) == LS_OK);
    CHECK(fragments == 3);
    fragmented.capabilities = 0;
    CHECK(ls_transport_send(&fragmented, 9, data, sizeof(data)) == LS_ENOTSUP);

    ls_transport_backend_t retrying = {
        .name = "retry",
        .send = send_retry,
        .context = &large,
        .retry = retry,
        .retry_limit = 1,
    };
    CHECK(ls_transport_send(&retrying, 10, data, sizeof(data)) == LS_OK);
    CHECK(retry_attempts == 2 && retry_callbacks == 1);

    uint8_t forwarded[sizeof(data)] = {0};
    uint8_t forwarded_received[1] = {0};
    ls_transport_reassembly_t forwarding;
    ls_transport_reassembly_init(&forwarding, forwarded, sizeof(forwarded), forwarded_received,
                                 sizeof(forwarded_received));
    forward_reassembly = &forwarding;

    ls_transport_backend_t v2 = {
        .name = "fragment-v2",
        .send_fragment_v2 = fragment_v2,
        .max_payload = mtu,
        .context = &small,
        .capabilities = LS_TRANSPORT_FRAGMENT,
    };
    CHECK(ls_transport_send(&v2, 11, data, sizeof(data)) == LS_OK);
    CHECK(v2_count == 3);
    CHECK(forward_length == sizeof(data));
    CHECK(memcmp(forwarded, data, sizeof(data)) == 0);
    forward_reassembly = 0;

    uint8_t reassembled[sizeof(data)] = {0};
    uint8_t received[1] = {0};
    ls_transport_reassembly_t reassembly;
    ls_transport_reassembly_init(&reassembly, reassembled, sizeof(reassembled), received,
                                 sizeof(received));
    size_t envelope_length = 0;
    CHECK(ls_transport_reassembly_push(&reassembly, &v2_fragments[2], &envelope_length) == LS_OK);
    CHECK(envelope_length == 0);
    CHECK(ls_transport_reassembly_push(&reassembly, &v2_fragments[0], &envelope_length) == LS_OK);
    CHECK(envelope_length == 0);
    CHECK(ls_transport_reassembly_push(&reassembly, &v2_fragments[1], &envelope_length) == LS_OK);
    CHECK(envelope_length == sizeof(data));
    CHECK(memcmp(reassembled, data, sizeof(data)) == 0);
    CHECK(ls_transport_reassembly_push(&reassembly, &v2_fragments[1], &envelope_length) ==
          LS_EBUSY);
    ls_transport_reassembly_reset(&reassembly);

    CHECK(ls_transport_send(0, 0, data, sizeof(data)) == LS_EINVAL);
    return 0;
}
