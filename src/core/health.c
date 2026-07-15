#include "internal.h"

void ls_health_register(const char *name, uint32_t deadline_ms) {
    if (!name || !deadline_ms)
        return;
    for (size_t i = 0; i < ls_runtime.health_count; i++)
        if (ls_hash_string(name) == ls_hash_string(ls_runtime.health[i].name)) {
            ls_runtime.health[i].deadline_ms = deadline_ms;
            return;
        }
    if (ls_runtime.health_count < LS_HEALTH_CAPACITY) {
        ls_health_record_t *record = &ls_runtime.health[ls_runtime.health_count++];
        ls_copy_string(record->name, sizeof record->name, name);
        record->deadline_ms = deadline_ms;
        record->last_touch_ms = ls_uptime_ms();
        record->expired = false;
    }
}
void ls_health_touch(const char *name) {
    for (size_t i = 0; i < ls_runtime.health_count; i++)
        if (ls_hash_string(name) == ls_hash_string(ls_runtime.health[i].name)) {
            ls_runtime.health[i].last_touch_ms = ls_uptime_ms();
            ls_runtime.health[i].expired = false;
            return;
        }
}
ls_result_t ls_health_poll(void) {
    uint32_t now = ls_uptime_ms();
    ls_result_t result = LS_OK;
    for (size_t i = 0; i < ls_runtime.health_count; i++) {
        ls_health_record_t *record = &ls_runtime.health[i];
        if (!record->expired && now - record->last_touch_ms > record->deadline_ms) {
            record->expired = true;
            ls_error_t error = {"health", (int32_t)ls_hash_string(record->name), LS_SEVERITY_ERROR,
                                record->name};
            ls_capture_error(&error);
            result = LS_EAGAIN;
        }
    }
    return result;
}
void ls_health_set_active_task(const char *name) {
    ls_copy_string(ls_runtime.active_task, sizeof ls_runtime.active_task, name);
}
void ls_watchdog_fed(void) {
    ls_runtime.watchdog_last_feed = ls_uptime_ms();
}
uint32_t ls_watchdog_last_feed_ms(void) {
    return ls_runtime.watchdog_last_feed;
}
void ls_watchdog_checkpoint(uint16_t checkpoint_id) {
    ls_runtime.watchdog_checkpoint = checkpoint_id;
}
uint16_t ls_watchdog_last_checkpoint(void) {
    return ls_runtime.watchdog_checkpoint;
}
void ls_power_sample(const ls_power_sample_t *sample) {
#if LS_ENABLE_POWER_SAMPLES
    if (!sample)
        return;
    ls_power_sample_t value = *sample;
    if (!value.timestamp_ms)
        value.timestamp_ms = ls_uptime_ms();
    ls_runtime.power_samples[ls_runtime.power_next] = value;
    ls_runtime.power_next = (ls_runtime.power_next + 1u) % LS_POWER_SAMPLE_CAPACITY;
    if (ls_runtime.power_count < LS_POWER_SAMPLE_CAPACITY)
        ls_runtime.power_count++;
    ls_metric_u32("vdd_mv", value.vdd_mv);
    ls_metric_u32("battery_mv", value.battery_mv);
    ls_metric_i32("current_ma", value.current_ma);
    ls_metric_i32("temperature_c", value.temperature_c);
#else
    (void)sample;
#endif
}
size_t ls_power_sample_count(void) {
    return ls_runtime.power_count;
}
