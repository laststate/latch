// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// examples/reference-designs/critical-redaction/main.c
//
// Latch runtime source. Part of the heap-free, deterministic,
// embedded failure-capture runtime that ships fault state over LEP
// to the Relay.
//
// Heap-free, bounded, deterministic.

#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

static uint8_t spool_bytes[50000];
static uint8_t secret_patient_or_process_data[64];
static uint32_t safe_control_state[8];
static unsigned encrypted_acks;

static ls_result_t demo_random(void *context, uint8_t *output, size_t length) {
    uint32_t *state = (uint32_t *)context;
    for (size_t index = 0u; index < length; index++) {
        *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
        output[index] = (uint8_t)(*state >> 24u);
    }
    return LS_OK;
}

static bool collector_available(void *context) {
    (void)context;
    return true;
}

static size_t collector_mtu(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static ls_result_t collector_send(void *context, const uint8_t *data, size_t length) {
    (void)context;
    ls_envelope_info_t info;
    if (ls_envelope_validate(data, length, &info) != LS_OK ||
        (info.flags & (LS_ENVELOPE_ENCRYPTED | LS_ENVELOPE_AEAD)) !=
            (LS_ENVELOPE_ENCRYPTED | LS_ENVELOPE_AEAD))
        return LS_EAUTH;
    encrypted_acks++;
    return LS_OK;
}

int main(void) {
    static const ls_identity_t identity = {
        .project_id = "reference-critical",
        .device_id = "controller-001",
        .product = "Critical redaction reference",
        .firmware_version = "1.0.0",
        .firmware_build_id = "critical-reference-0001",
    };
    uint8_t demo_key[LS_SECURITY_KEY_SIZE];
    memset(demo_key, 0x42, sizeof(demo_key));
    uint32_t random_state = UINT32_C(0x9e3779b9);
    ls_memory_storage_t memory = {spool_bytes, sizeof(spool_bytes)};
    ls_storage_backend_t storage = {
        .name = "replace-with-secure-atomic-flash",
        .context = &memory,
        .capacity = sizeof(spool_bytes),
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    ls_transport_backend_t collector = {
        .name = "authenticated-durable-collector",
        .priority = 1,
        .available = collector_available,
        .send = collector_send,
        .max_payload = collector_mtu,
        .capabilities = LS_TRANSPORT_DURABLE_ACK | LS_TRANSPORT_SECURE,
    };
    ls_config_t config = {.identity = &identity};
    ls_security_policy_t policy = {
        .algorithm = LS_SECURITY_XCHACHA20_POLY1305,
        .key_id = 7u,
        .reject_plaintext = true,
    };

    if (ls_init(&config) != LS_OK || sizeof(spool_bytes) < ls_storage_required_size())
        return 1;
    ls_storage_register(&storage);
    ls_transport_register(&collector);
    if (ls_security_set_key(demo_key, sizeof(demo_key)) != LS_OK ||
        ls_security_set_policy(&policy) != LS_OK)
        return 2;
    ls_security_set_random_provider(demo_random, &random_state);
    ls_secure_zero(demo_key, sizeof(demo_key));
    if (ls_boot() != LS_OK)
        return 3;

    memset(secret_patient_or_process_data, 0x5a, sizeof(secret_patient_or_process_data));
    safe_control_state[0] = 125u;
    if (ls_dump_region_register("control-state", safe_control_state, sizeof(safe_control_state),
                                LS_DUMP_SAFE) != LS_OK ||
        ls_memory_redact(secret_patient_or_process_data, sizeof(secret_patient_or_process_data),
                         LS_REDACT_EXCLUDE) != LS_OK)
        return 4;

    ls_watchdog_checkpoint(0x120u);
    if (ls_capture_coredump(LS_CAPTURE_SELECTIVE) != LS_OK || ls_flush() != LS_OK ||
        encrypted_acks != 1u)
        return 5;
    puts("Selective redacted evidence received as an AEAD envelope");
    return 0;
}
