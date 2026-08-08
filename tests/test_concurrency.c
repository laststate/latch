#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "concurrency failed: %s:%d\n", #x, __LINE__);                          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
#define THREADS 4
#define RECORDS 1000
#if defined(_WIN32)
static CRITICAL_SECTION lock;
static void enter(void *c) {
    (void)c;
    EnterCriticalSection(&lock);
}
static void leave(void *c) {
    (void)c;
    LeaveCriticalSection(&lock);
}
static DWORD WINAPI producer(LPVOID arg) {
    intptr_t id = (intptr_t)arg;
    for (int i = 0; i < RECORDS; i++) {
        ls_result_t r =
            ls_blackbox_record_values(LS_BLACKBOX_USER, (uint16_t)id, 0, (int32_t)id, i, 0, 0);
        if (r != LS_OK) {
            return (DWORD)r;
        }
    }
    return 0u;
}
#else
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static void enter(void *c) {
    (void)c;
    int r = pthread_mutex_lock(&lock);
    (void)r;
}
static void leave(void *c) {
    (void)c;
    int r = pthread_mutex_unlock(&lock);
    (void)r;
}
static void *producer(void *arg) {
    intptr_t id = (intptr_t)arg;
    for (int i = 0; i < RECORDS; i++) {
        ls_result_t r =
            ls_blackbox_record_values(LS_BLACKBOX_USER, (uint16_t)id, 0, (int32_t)id, i, 0, 0);
        if (r != LS_OK) {
            return (void *)(intptr_t)r;
        }
    }
    return NULL;
}
#endif
int main(void) {
#if defined(_WIN32)
    InitializeCriticalSection(&lock);
#endif
    static const ls_identity_t id = {.project_id = "concurrency",
                                     .device_id = "host",
                                     .firmware_build_id = "concurrent-build-01"};
    ls_config_t config = {.identity = &id, .enter_critical = enter, .leave_critical = leave};
    CHECK(ls_init(&config) == LS_OK);
    ls_blackbox_clear();
#if defined(_WIN32)
    HANDLE threads[THREADS];
    for (int i = 0; i < THREADS; i++) {
        threads[i] = CreateThread(NULL, 0, producer, (LPVOID)(intptr_t)(i + 1), 0, NULL);
        CHECK(threads[i] != NULL);
    }
    for (int i = 0; i < THREADS; i++) {
        DWORD result = 0u;
        CHECK(WaitForSingleObject(threads[i], INFINITE) == WAIT_OBJECT_0);
        CHECK(GetExitCodeThread(threads[i], &result) != 0);
        CHECK(result == 0u);
        CHECK(CloseHandle(threads[i]) != 0);
    }
    DeleteCriticalSection(&lock);
#else
    pthread_t threads[THREADS];
    for (int i = 0; i < THREADS; i++) {
        CHECK(pthread_create(&threads[i], NULL, producer, (void *)(intptr_t)(i + 1)) == 0);
    }
    for (int i = 0; i < THREADS; i++) {
        void *result = NULL;
        CHECK(pthread_join(threads[i], &result) == 0);
        CHECK(result == NULL);
    }
#endif
    ls_blackbox_stats_t stats;
    CHECK(ls_blackbox_get_stats(&stats) == LS_OK);
    CHECK(stats.total_records == THREADS * RECORDS);
    CHECK(stats.count == LS_BLACKBOX_CAPACITY);
    CHECK(stats.overwritten_records == THREADS * RECORDS - LS_BLACKBOX_CAPACITY);
    ls_blackbox_record_t records[LS_BLACKBOX_CAPACITY];
    size_t count = 0;
    CHECK(ls_blackbox_copy(records, LS_BLACKBOX_CAPACITY, &count) == LS_OK);
    CHECK(count == LS_BLACKBOX_CAPACITY);
    return 0;
}
