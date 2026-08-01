#ifndef LS_INTERNAL_H
#define LS_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "laststate/latch.h"

typedef struct {
    uint32_t at_ms;
    uint16_t message_id;
    uint8_t severity, value_count;
    uint32_t category_hash, message_hash;
#if LS_STORE_STRINGS
    char category[LS_BREADCRUMB_CATEGORY_MAX];
    char message[LS_BREADCRUMB_MESSAGE_MAX];
#endif
    ls_breadcrumb_kv_t values[LS_BREADCRUMB_KV_MAX];
} ls_breadcrumb_record_t;

typedef struct {
    uint32_t name_hash;
#if LS_STORE_STRINGS
    char name[LS_METRIC_NAME_MAX];
#endif
    int32_t value, previous, minimum, maximum;
    int64_t sum;
    uint32_t count;
    int32_t window[LS_METRIC_WINDOW_SIZE];
    uint8_t window_next, window_count, type;
} ls_metric_record_t;

typedef struct {
    char name[LS_BREADCRUMB_CATEGORY_MAX];
    uint32_t deadline_ms, last_touch_ms;
    bool expired;
} ls_health_record_t;
typedef struct {
    uint16_t id;
    uint32_t started_ms, elapsed_ms;
    bool active;
} ls_span_record_t;
typedef struct {
    const uint8_t *address;
    size_t length;
    ls_redaction_mode_t mode;
} ls_redaction_t;
typedef struct {
    const char *expression, *file, *message;
    int32_t line;
} ls_assert_info_t;
typedef struct {
    uint16_t message_id, format_id;
    uint8_t argument_count;
    uint32_t arguments[LS_LOG_ARG_MAX];
} ls_log_info_t;
typedef struct {
    uint32_t domain_hash;
    uint16_t sampling_permyriad, rate_maximum, rate_count;
    uint32_t rate_period_ms, window_started_ms, seen;
} ls_policy_record_t;
typedef struct {
    uint32_t fingerprint, count, first_seen_ms, last_seen_ms;
} ls_dedup_record_t;

typedef struct {
    ls_event_type_t type;
    ls_priority_t priority;
    uint32_t timestamp_ms, fingerprint;
    const char *domain;
    int32_t code;
    ls_severity_t severity;
    const char *message;
    const ls_arch_context_t *cpu;
    const ls_assert_info_t *assertion;
    const ls_peripheral_fault_t *peripheral;
    const ls_log_info_t *log;
    ls_capture_level_t capture_level;
    uint32_t repeat_count, first_seen_ms, last_seen_ms;
} ls_event_t;

typedef struct {
    uint8_t *data;
    size_t capacity, length;
} ls_writer_t;

typedef struct {
    uint32_t magic, version, boot_count, previous_uptime_ms;
    uint32_t consecutive_failures, first_failure_ms, last_sequence;
    uint32_t pending_crash_id, release_hash;
    uint8_t crash_pending, expected_reset, boot_successful, release_state;
    uint32_t crc;
} ls_persistent_boot_t;

typedef struct {
    ls_config_t config;
    bool initialized, capturing, previous_crashed, boot_loop, safe_mode;
    uint32_t boot_started_ms, sequence, boot_count;
    ls_reset_info_t reset_info;
    ls_persistent_boot_t persistent;
    ls_storage_backend_t *storage;
    ls_transport_backend_t *transports[LS_MAX_TRANSPORTS];
    size_t transport_count;
    ls_breadcrumb_record_t breadcrumbs[LS_BREADCRUMB_CAPACITY];
    size_t breadcrumb_next, breadcrumb_count;
    ls_severity_t breadcrumb_min_level;
    ls_breadcrumb_policy_t breadcrumb_policy;
    ls_metric_record_t metrics[LS_METRIC_CAPACITY];
    size_t metric_count;
    ls_power_sample_t power_samples[LS_POWER_SAMPLE_CAPACITY];
    size_t power_next, power_count;
    ls_health_record_t health[LS_HEALTH_CAPACITY];
    size_t health_count;
    char active_task[LS_BREADCRUMB_CATEGORY_MAX];
    uint32_t watchdog_last_feed;
    uint16_t watchdog_checkpoint;
    ls_span_record_t spans[LS_SPAN_CAPACITY];
    ls_dump_region_t dump_regions[LS_MAX_DUMP_REGIONS];
    size_t dump_region_count;
    ls_redaction_t redactions[LS_MAX_REDACTIONS];
    size_t redaction_count;
    const uint8_t *stack_lower, *stack_upper;
    ls_heap_stats_t heap_stats;
    ls_assert_policy_t assert_policy;
    ls_assert_callback_t assert_callback;
    ls_log_stream_fn log_stream;
    void *log_stream_context;
    ls_severity_t log_min_level;
    ls_policy_record_t policies[LS_POLICY_CAPACITY];
    size_t policy_count;
    ls_dedup_record_t dedup[LS_DEDUP_CAPACITY];
    size_t dedup_count, dedup_next;
    uint32_t random_state;
    uint8_t security_key[LS_SECURITY_KEY_SIZE];
    size_t security_key_length;
    ls_security_policy_t security_policy;
    ls_crypto_random_fn crypto_random;
    void *crypto_random_context;
    uint8_t last_envelope_nonce[LS_XCHACHA20_NONCE_SIZE];
    bool has_last_envelope_nonce;
    uint32_t replay_highest;
    uint64_t replay_bitmap;
} ls_runtime_t;
extern ls_runtime_t ls_runtime;

void *ls_memcpy(void *destination, const void *source, size_t length);
void *ls_memset(void *destination, int value, size_t length);
int ls_memcmp(const void *left, const void *right, size_t length);
size_t ls_string_length(const char *text);
uint32_t ls_crc32(const void *data, size_t length);
uint32_t ls_hash_string(const char *text);
void ls_copy_string(char *dst, size_t size, const char *src);
ls_result_t ls_writer_write(ls_writer_t *writer, const void *data, size_t size);
ls_result_t ls_writer_u8(ls_writer_t *writer, uint8_t value);
ls_result_t ls_writer_u16(ls_writer_t *writer, uint16_t value);
ls_result_t ls_writer_u32(ls_writer_t *writer, uint32_t value);
ls_result_t ls_writer_u64(ls_writer_t *writer, uint64_t value);
ls_result_t ls_writer_tlv(ls_writer_t *writer, uint16_t type, const void *value, uint16_t length);
ls_result_t ls_envelope_encode(const ls_event_t *event, uint8_t *out, size_t capacity,
                               size_t *length);
ls_result_t ls_spool_init(void);
ls_result_t ls_spool_append(const uint8_t *data, size_t length, ls_priority_t priority);
ls_result_t ls_spool_flush(void);
size_t ls_spool_storage_size(void);
ls_result_t ls_boot_state_load(void);
ls_result_t ls_boot_state_save(void);
void ls_boot_state_mark_crash(uint32_t event_id);
ls_result_t ls_capture_event(const ls_event_t *event);
bool ls_minimal_snapshot_validate(ls_minimal_snapshot_t *snapshot);
bool ls_policy_apply(ls_event_t *event);
ls_redaction_mode_t ls_redaction_for(const void *address, size_t length, bool *matched);
void ls_enter_critical(void);
void ls_leave_critical(void);
#endif
