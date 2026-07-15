#include <stdio.h>
#include <string.h>
#include <time.h>
#include "laststate/latch.h"
static uint8_t retained[50000];
static size_t bytes_sent, events_sent;
static bool yes(void *x) {
    (void)x;
    return true;
}
static size_t mtu(void *x) {
    (void)x;
    return LS_MAX_EVENT_SIZE;
}
static ls_result_t send_data(void *x, const uint8_t *data, size_t length) {
    (void)x;
    (void)data;
    bytes_sent += length;
    events_sent++;
    return LS_OK;
}
int main(void) {
    ls_identity_t identity = {
        .project_id = "benchmark", .device_id = "host", .firmware_build_id = "benchmark1"};
    ls_config_t config = {.identity = &identity};
    ls_memory_storage_t memory = {retained, sizeof retained};
    ls_storage_backend_t storage = {.name = "memory",
                                    .context = &memory,
                                    .capacity = sizeof retained,
                                    .read = ls_memory_storage_read,
                                    .write = ls_memory_storage_write,
                                    .erase = ls_memory_storage_erase};
    ls_transport_backend_t transport = {
        .name = "sink", .priority = 1, .available = yes, .send = send_data, .max_payload = mtu};
    if (ls_init(&config) != LS_OK)
        return 1;
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    if (ls_boot() != LS_OK)
        return 1;
    clock_t start = clock();
    const unsigned iterations = 10000;
    for (unsigned i = 0; i < iterations; i++) {
        ls_metric_u32("iteration", i);
        ls_breadcrumb("benchmark");
        ls_error_t error = {"bench", (int32_t)i, LS_SEVERITY_ERROR, "event"};
        ls_capture_error(&error);
        if ((i % LS_SPOOL_MAX_RECORDS) == LS_SPOOL_MAX_RECORDS - 1u)
            (void)ls_flush();
    }
    (void)ls_flush();
    double seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("{\"iterations\":%u,\"events\":%zu,\"bytes\":%zu,\"seconds\":%.6f,\"events_per_second\":"
           "%.2f}\n",
           iterations, events_sent, bytes_sent, seconds, seconds ? events_sent / seconds : 0.0);
    return 0;
}
