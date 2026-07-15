#include "../core/internal.h"

static ls_transport_backend_t *busy_transports[LS_MAX_TRANSPORTS];

static bool has_fragment_sender(const ls_transport_backend_t *transport) {
    return transport && (transport->send_fragment || transport->send_fragment_v2);
}

static bool acquire_transport(ls_transport_backend_t *transport) {
    bool acquired = false;
    ls_enter_critical();
    for (size_t index = 0; index < LS_MAX_TRANSPORTS; ++index) {
        if (busy_transports[index] == transport) {
            break;
        }
        if (!busy_transports[index]) {
            busy_transports[index] = transport;
            acquired = true;
            break;
        }
    }
    ls_leave_critical();
    return acquired;
}

static void release_transport(ls_transport_backend_t *transport) {
    ls_enter_critical();
    for (size_t index = 0; index < LS_MAX_TRANSPORTS; ++index) {
        if (busy_transports[index] == transport) {
            busy_transports[index] = 0;
            break;
        }
    }
    ls_leave_critical();
}

static size_t transport_mtu(const ls_transport_backend_t *transport, size_t event_size) {
    return transport->max_payload ? transport->max_payload(transport->context) : event_size;
}

bool ls_transport_result_is_retryable(ls_result_t result) {
    return result == LS_EAGAIN || result == LS_EBUSY;
}

bool ls_transport_can_send(const ls_transport_backend_t *transport, size_t event_size) {
    if (!transport || (!transport->send && !has_fragment_sender(transport))) {
        return false;
    }
    if (transport->available && !transport->available(transport->context)) {
        return false;
    }
    size_t mtu = transport_mtu(transport, event_size);
    if (!mtu) {
        return false;
    }
    return event_size <= mtu ||
           ((transport->capabilities & LS_TRANSPORT_FRAGMENT) && has_fragment_sender(transport));
}

void ls_transport_register(ls_transport_backend_t *transport) {
    if (!transport || (!transport->send && !has_fragment_sender(transport))) {
        return;
    }

    ls_enter_critical();
    for (size_t index = 0; index < ls_runtime.transport_count; ++index) {
        if (ls_runtime.transports[index] == transport) {
            ls_leave_critical();
            return;
        }
    }
    if (ls_runtime.transport_count < LS_MAX_TRANSPORTS) {
        ls_runtime.transports[ls_runtime.transport_count++] = transport;
    }
    ls_leave_critical();
}

void ls_transport_clear(void) {
    ls_enter_critical();
    ls_runtime.transport_count = 0;
    ls_leave_critical();
}

ls_transport_backend_t *ls_transport_select(ls_priority_t priority, size_t event_size) {
    ls_transport_backend_t *transports[LS_MAX_TRANSPORTS];
    size_t transport_count = 0;
    ls_enter_critical();
    transport_count = ls_runtime.transport_count;
    if (transport_count > LS_MAX_TRANSPORTS) {
        transport_count = LS_MAX_TRANSPORTS;
    }
    for (size_t index = 0; index < transport_count; ++index) {
        transports[index] = ls_runtime.transports[index];
    }
    ls_leave_critical();

    ls_transport_backend_t *best = 0;
    int64_t best_score = INT64_MIN;
    for (size_t index = 0; index < transport_count; ++index) {
        ls_transport_backend_t *transport = transports[index];
        if (!ls_transport_can_send(transport, event_size)) {
            continue;
        }

        int64_t score = (int64_t)transport->priority * 1000;
        if (priority >= LS_PRIORITY_WARNING) {
            score -= transport->energy_cost;
            score -= (int64_t)transport->monetary_cost * 2;
        }
        if (transport->capabilities & LS_TRANSPORT_DURABLE_ACK) {
            score += 20;
        }
        if (score > best_score) {
            best = transport;
            best_score = score;
        }
    }
    return best;
}

static bool should_retry(ls_transport_backend_t *transport, uint8_t *attempt, ls_result_t result) {
    if (!ls_transport_result_is_retryable(result) || *attempt >= transport->retry_limit) {
        return false;
    }
    ++*attempt;
    return !transport->retry || transport->retry(transport->context, *attempt, result);
}

static ls_result_t send_complete(ls_transport_backend_t *transport, const uint8_t *data,
                                 size_t length) {
    if (!transport->send) {
        return LS_ENOTSUP;
    }

    uint8_t attempt = 0;
    ls_result_t result;
    do {
        result = transport->send(transport->context, data, length);
    } while (should_retry(transport, &attempt, result));
    return result;
}

static ls_result_t send_fragment(ls_transport_backend_t *transport,
                                 const ls_transport_fragment_t *fragment) {
    uint8_t attempt = 0;
    ls_result_t result;
    do {
        if (transport->send_fragment_v2) {
            result = transport->send_fragment_v2(transport->context, fragment);
        } else if (transport->send_fragment) {
            result = transport->send_fragment(
                transport->context, fragment->event_id, fragment->fragment_index,
                fragment->fragment_count, fragment->data, fragment->length, fragment->fragment_crc);
        } else {
            return LS_ENOTSUP;
        }
    } while (should_retry(transport, &attempt, result));
    return result;
}

ls_result_t ls_transport_send(ls_transport_backend_t *transport, uint32_t event_id,
                              const uint8_t *data, size_t length) {
    if (!transport || !data || !length) {
        return LS_EINVAL;
    }
    if (length > UINT32_MAX) {
        return LS_EOVERFLOW;
    }
    if (transport->available && !transport->available(transport->context)) {
        return LS_EAGAIN;
    }

    size_t mtu = transport_mtu(transport, length);
    if (!mtu) {
        return LS_EINVAL;
    }
    bool fragmented = length > mtu;
    if (fragmented &&
        (!(transport->capabilities & LS_TRANSPORT_FRAGMENT) || !has_fragment_sender(transport))) {
        return LS_ENOTSUP;
    }
    if (!fragmented && !transport->send &&
        (!(transport->capabilities & LS_TRANSPORT_FRAGMENT) || !has_fragment_sender(transport))) {
        return LS_ENOTSUP;
    }
    if (!acquire_transport(transport)) {
        return LS_EBUSY;
    }

    ls_result_t result = LS_OK;
    if (!fragmented && transport->send) {
        result = send_complete(transport, data, length);
    } else {
        size_t fragment_count = length / mtu;
        if (length % mtu) {
            ++fragment_count;
        }
        if (fragment_count > UINT16_MAX || mtu > UINT32_MAX) {
            result = LS_EOVERFLOW;
        } else {
            uint32_t envelope_crc = ls_crc32(data, length);
            for (size_t index = 0; index < fragment_count; ++index) {
                size_t offset = index * mtu;
                size_t fragment_length = length - offset;
                if (fragment_length > mtu) {
                    fragment_length = mtu;
                }
                ls_transport_fragment_t fragment = {
                    .event_id = event_id,
                    .envelope_crc = envelope_crc,
                    .total_length = (uint32_t)length,
                    .offset = (uint32_t)offset,
                    .fragment_capacity = (uint32_t)mtu,
                    .fragment_crc = ls_crc32(data + offset, fragment_length),
                    .fragment_index = (uint16_t)index,
                    .fragment_count = (uint16_t)fragment_count,
                    .data = data + offset,
                    .length = fragment_length,
                };
                result = send_fragment(transport, &fragment);
                if (result != LS_OK) {
                    break;
                }
            }
        }
    }

    release_transport(transport);
    return result;
}

void ls_transport_reassembly_init(ls_transport_reassembly_t *reassembly, uint8_t *data,
                                  size_t capacity, uint8_t *received, size_t received_capacity) {
    if (!reassembly) {
        return;
    }
    *reassembly = (ls_transport_reassembly_t){
        .data = data,
        .capacity = capacity,
        .received = received,
        .received_capacity = received_capacity,
    };
    ls_transport_reassembly_reset(reassembly);
}

void ls_transport_reassembly_reset(ls_transport_reassembly_t *reassembly) {
    if (!reassembly) {
        return;
    }
    if (reassembly->received && reassembly->received_capacity) {
        ls_memset(reassembly->received, 0, reassembly->received_capacity);
    }
    reassembly->event_id = 0;
    reassembly->envelope_crc = 0;
    reassembly->total_length = 0;
    reassembly->fragment_capacity = 0;
    reassembly->fragment_count = 0;
    reassembly->received_count = 0;
    reassembly->active = false;
    reassembly->complete = false;
}

static ls_result_t validate_fragment(const ls_transport_reassembly_t *reassembly,
                                     const ls_transport_fragment_t *fragment) {
    if (!reassembly || !fragment || !reassembly->data || !reassembly->received || !fragment->data ||
        !fragment->length || !fragment->total_length || !fragment->fragment_capacity ||
        !fragment->fragment_count || fragment->fragment_index >= fragment->fragment_count) {
        return LS_EINVAL;
    }
    if (fragment->total_length > reassembly->capacity ||
        fragment->fragment_count > reassembly->received_capacity * 8u) {
        return LS_ENOSPACE;
    }

    uint32_t expected_count = fragment->total_length / fragment->fragment_capacity;
    if (fragment->total_length % fragment->fragment_capacity) {
        ++expected_count;
    }
    if (!expected_count || expected_count > UINT16_MAX ||
        fragment->fragment_count != expected_count) {
        return LS_ECORRUPT;
    }

    uint64_t expected_offset = (uint64_t)fragment->fragment_index * fragment->fragment_capacity;
    if (expected_offset >= fragment->total_length || fragment->offset != expected_offset) {
        return LS_ECORRUPT;
    }
    uint32_t expected_length = fragment->total_length - (uint32_t)expected_offset;
    if (expected_length > fragment->fragment_capacity) {
        expected_length = fragment->fragment_capacity;
    }
    if (fragment->length != expected_length ||
        fragment->fragment_crc != ls_crc32(fragment->data, fragment->length)) {
        return LS_ECORRUPT;
    }
    return LS_OK;
}

ls_result_t ls_transport_reassembly_push(ls_transport_reassembly_t *reassembly,
                                         const ls_transport_fragment_t *fragment,
                                         size_t *envelope_length) {
    if (!envelope_length) {
        return LS_EINVAL;
    }
    *envelope_length = 0;
    ls_result_t result = validate_fragment(reassembly, fragment);
    if (result != LS_OK) {
        return result;
    }
    if (reassembly->complete) {
        return LS_EBUSY;
    }

    if (!reassembly->active) {
        reassembly->event_id = fragment->event_id;
        reassembly->envelope_crc = fragment->envelope_crc;
        reassembly->total_length = fragment->total_length;
        reassembly->fragment_capacity = fragment->fragment_capacity;
        reassembly->fragment_count = fragment->fragment_count;
        reassembly->received_count = 0;
        ls_memset(reassembly->received, 0, reassembly->received_capacity);
        reassembly->active = true;
    } else if (reassembly->event_id != fragment->event_id ||
               reassembly->envelope_crc != fragment->envelope_crc ||
               reassembly->total_length != fragment->total_length ||
               reassembly->fragment_capacity != fragment->fragment_capacity ||
               reassembly->fragment_count != fragment->fragment_count) {
        return LS_EBUSY;
    }

    size_t bitmap_index = fragment->fragment_index / 8u;
    uint8_t bit = (uint8_t)(1u << (fragment->fragment_index % 8u));
    if (reassembly->received[bitmap_index] & bit) {
        if (ls_memcmp(reassembly->data + fragment->offset, fragment->data, fragment->length) != 0) {
            ls_transport_reassembly_reset(reassembly);
            return LS_ECORRUPT;
        }
        return LS_OK;
    }

    ls_memcpy(reassembly->data + fragment->offset, fragment->data, fragment->length);
    reassembly->received[bitmap_index] |= bit;
    ++reassembly->received_count;
    if (reassembly->received_count != reassembly->fragment_count) {
        return LS_OK;
    }
    if (ls_crc32(reassembly->data, reassembly->total_length) != reassembly->envelope_crc) {
        ls_transport_reassembly_reset(reassembly);
        return LS_ECORRUPT;
    }

    reassembly->complete = true;
    *envelope_length = reassembly->total_length;
    return LS_OK;
}
