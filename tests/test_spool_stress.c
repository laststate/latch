#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "spool stress failed: %s:%d\n", #condition, __LINE__);                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

enum {
    SPOOL_HEADER_SIZE = 24u,
    SPOOL_RECORD_HEADER_SIZE = 28u,
    SPOOL_RECORD_LENGTH_OFFSET = 6u,
};

static uint8_t bytes[50000];
static uint8_t baseline[50000];
static unsigned delivered;
static int online;

static bool available(void *context) {
    (void)context;
    return online != 0;
}

static size_t mtu(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    (void)context;
    if (ls_envelope_validate(data, length, NULL) != LS_OK)
        return LS_ECORRUPT;
    delivered++;
    return LS_OK;
}

static int boot(ls_storage_sim_t *simulator) {
    static const ls_identity_t identity = {
        .project_id = "spool-stress",
        .device_id = "host-model",
        .firmware_build_id = "stress01",
    };
    static ls_storage_backend_t storage;
    static ls_transport_backend_t transport;
    ls_config_t config = {.identity = &identity};
    CHECK(ls_init(&config) == LS_OK);
    storage = (ls_storage_backend_t){
        .name = "nor-model",
        .context = simulator,
        .capacity = sizeof(bytes),
        .erase_size = 1u,
        .write_size = 1u,
        .read = ls_storage_sim_read,
        .write = ls_storage_sim_write,
        .erase = ls_storage_sim_erase,
        .sync = ls_storage_sim_sync,
    };
    transport = (ls_transport_backend_t){
        .name = "validated-sink",
        .priority = 1,
        .available = available,
        .send = send_data,
        .max_payload = mtu,
    };
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);
    return 0;
}

static uint16_t first_record_length(void) {
    size_t offset = SPOOL_HEADER_SIZE + SPOOL_RECORD_LENGTH_OFFSET;
    return (uint16_t)bytes[offset] | (uint16_t)((uint16_t)bytes[offset + 1u] << 8u);
}

static int payload_corruption_sweep(void) {
    memset(bytes, 0xff, sizeof(bytes));
    ls_storage_sim_t simulator = {.data = bytes, .size = sizeof(bytes)};
    online = 0;
    CHECK(boot(&simulator) == 0);
    ls_capture_message("payload corruption sentinel", LS_SEVERITY_ERROR);
    uint16_t length = first_record_length();
    CHECK(length > 0u && length <= LS_MAX_EVENT_SIZE);
    memcpy(baseline, bytes, sizeof(bytes));

    size_t payload = SPOOL_HEADER_SIZE + SPOOL_RECORD_HEADER_SIZE;
    for (size_t index = 0u; index < length; index++) {
        memcpy(bytes, baseline, sizeof(bytes));
        bytes[payload + index] ^= (uint8_t)(1u << (index & 7u));
        simulator = (ls_storage_sim_t){.data = bytes, .size = sizeof(bytes)};
        online = 1;
        delivered = 0u;
        CHECK(boot(&simulator) == 0);
        CHECK(ls_flush() == LS_OK);
        CHECK(delivered == 0u);
    }
    return 0;
}

static int interrupted_program_sweep(void) {
    static const size_t partials[] = {0u, 1u, 7u, 31u, 255u};
    for (size_t partial_index = 0u; partial_index < sizeof(partials) / sizeof(partials[0]);
         partial_index++) {
        for (uint32_t operation = 1u; operation <= 32u; operation++) {
            memset(bytes, 0xff, sizeof(bytes));
            ls_storage_sim_t simulator = {
                .data = bytes,
                .size = sizeof(bytes),
            };
            online = 0;
            CHECK(boot(&simulator) == 0);
            ls_storage_sim_fail_at(&simulator, operation, partials[partial_index]);
            ls_capture_message("interrupted append", LS_SEVERITY_ERROR);
            ls_storage_sim_reset_faults(&simulator);

            online = 1;
            delivered = 0u;
            CHECK(boot(&simulator) == 0);
            ls_result_t result = ls_flush();
            CHECK(result == LS_OK || result == LS_EAGAIN);
            CHECK(delivered <= 1u);
        }
    }
    return 0;
}

static int corrupt_record_does_not_hide_next(void) {
    memset(bytes, 0xff, sizeof(bytes));
    ls_storage_sim_t simulator = {.data = bytes, .size = sizeof(bytes)};
    online = 0;
    CHECK(boot(&simulator) == 0);
    ls_capture_message("corrupt me", LS_SEVERITY_ERROR);
    uint16_t first_length = first_record_length();
    CHECK(first_length > 0u);
    ls_capture_message("deliver me", LS_SEVERITY_ERROR);
    bytes[SPOOL_HEADER_SIZE + SPOOL_RECORD_HEADER_SIZE + first_length / 2u] ^= 0x80u;

    online = 1;
    delivered = 0u;
    CHECK(boot(&simulator) == 0);
    CHECK(ls_flush() == LS_OK);
    CHECK(delivered == 1u);
    return 0;
}

int main(void) {
    CHECK(payload_corruption_sweep() == 0);
    CHECK(interrupted_program_sweep() == 0);
    CHECK(corrupt_record_does_not_hide_next() == 0);
    return 0;
}
