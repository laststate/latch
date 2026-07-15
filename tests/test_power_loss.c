#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "power-loss check failed: %s:%d\n", #condition, __LINE__);             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint8_t bytes[50000];
static unsigned sent;
static unsigned erase_calls;

static bool available(void *context) {
    (void)context;
    return true;
}

static size_t mtu(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    (void)context;
    if (ls_envelope_validate(data, length, NULL) != LS_OK) {
        return LS_ECORRUPT;
    }

    sent++;
    return LS_OK;
}

static ls_result_t tracked_erase(void *context, size_t offset, size_t length) {
    erase_calls++;
    return ls_storage_sim_erase(context, offset, length);
}

static int boot_runtime(ls_storage_backend_t *storage, ls_transport_backend_t *transport) {
    static const ls_identity_t identity = {
        .project_id = "power",
        .device_id = "one",
        .firmware_build_id = "power001",
    };
    ls_config_t config = {.identity = &identity};
    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(storage);
    ls_transport_register(transport);
    CHECK(ls_boot() == LS_OK);
    return 0;
}

int main(void) {
    for (uint32_t failure = 1u; failure <= 18u; failure++) {
        memset(bytes, 0xff, sizeof(bytes));
        ls_storage_sim_t simulator = {
            .data = bytes,
            .size = sizeof(bytes),
        };
        ls_storage_backend_t storage = {
            .name = "sim",
            .context = &simulator,
            .capacity = sizeof(bytes),
            .erase_size = 1u,
            .write_size = 1u,
            .read = ls_storage_sim_read,
            .write = ls_storage_sim_write,
            .erase = tracked_erase,
            .sync = ls_storage_sim_sync,
        };
        ls_transport_backend_t transport = {
            .name = "sink",
            .priority = 1u,
            .available = available,
            .send = send_data,
            .max_payload = mtu,
        };

        erase_calls = 0u;
        CHECK(boot_runtime(&storage, &transport) == 0);
        CHECK(erase_calls == 0u);

        ls_storage_sim_fail_at(&simulator, failure, 7u);
        ls_capture_message("power cut", LS_SEVERITY_ERROR);
        CHECK(erase_calls == 0u);

        ls_storage_sim_reset_faults(&simulator);
        CHECK(boot_runtime(&storage, &transport) == 0);
        sent = 0u;
        ls_result_t flush = ls_flush();
        CHECK(flush == LS_OK || flush == LS_EAGAIN);
        CHECK(sent <= 1u);
    }

    memset(bytes, 0xff, sizeof(bytes));
    ls_storage_sim_t simulator = {
        .data = bytes,
        .size = sizeof(bytes),
    };
    ls_storage_backend_t storage = {
        .name = "sim",
        .context = &simulator,
        .capacity = sizeof(bytes),
        .erase_size = 1u,
        .write_size = 1u,
        .read = ls_storage_sim_read,
        .write = ls_storage_sim_write,
        .erase = tracked_erase,
        .sync = ls_storage_sim_sync,
    };
    ls_transport_backend_t transport = {
        .name = "sink",
        .priority = 1u,
        .available = available,
        .send = send_data,
        .max_payload = mtu,
    };
    erase_calls = 0u;
    CHECK(boot_runtime(&storage, &transport) == 0);
    ls_capture_message("recover retained record", LS_SEVERITY_ERROR);
    bytes[0] = 0u;
    erase_calls = 0u;
    CHECK(boot_runtime(&storage, &transport) == 0);
    CHECK(erase_calls == 0u);
    sent = 0u;
    CHECK(ls_flush() == LS_OK);
    CHECK(sent == 1u);

    return 0;
}
