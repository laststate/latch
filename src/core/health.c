#include "internal.h"
#include "laststate/blackbox.h"

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
        record->misses = 0u;
        record->max_lateness_ms = 0u;
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
ls_result_t ls_health_get(const char *name, ls_health_t *health) {
    if (!name || !health)
        return LS_EINVAL;
    uint32_t hash = ls_hash_string(name);
    for (size_t i = 0; i < ls_runtime.health_count; ++i) {
        if (hash == ls_hash_string(ls_runtime.health[i].name)) {
            ls_health_record_t *record = &ls_runtime.health[i];
            *health = (ls_health_t){record->name, record->deadline_ms, record->last_touch_ms,
                                    record->misses, record->max_lateness_ms, record->expired};
            return LS_OK;
        }
    }
    return LS_EAGAIN;
}
size_t ls_health_snapshot(ls_health_t *health, size_t capacity) {
    if (!health || !capacity)
        return 0u;
    size_t count = ls_runtime.health_count < capacity ? ls_runtime.health_count : capacity;
    for (size_t i = 0u; i < count; ++i) {
        ls_health_record_t *record = &ls_runtime.health[i];
        health[i] = (ls_health_t){record->name, record->deadline_ms, record->last_touch_ms,
                                  record->misses, record->max_lateness_ms, record->expired};
    }
    return count;
}
ls_result_t ls_health_poll(void) {
    uint32_t now = ls_uptime_ms();
    ls_result_t result = LS_OK;
    for (size_t i = 0; i < ls_runtime.health_count; i++) {
        ls_health_record_t *record = &ls_runtime.health[i];
        uint32_t age = now - record->last_touch_ms;
        if (age > record->deadline_ms) {
            uint32_t lateness = age - record->deadline_ms;
            if (lateness > record->max_lateness_ms)
                record->max_lateness_ms = lateness;
            if (!record->expired) {
                record->expired = true;
                if (record->misses != UINT32_MAX)
                    record->misses++;
                (void)ls_blackbox_record_values(LS_BLACKBOX_WATCHDOG, (uint16_t)i,
                                                LS_BLACKBOX_ERROR, (int32_t)record->deadline_ms,
                                                (int32_t)age, (int32_t)record->misses, 0);
                ls_error_t error = {"health", (int32_t)ls_hash_string(record->name),
                                    LS_SEVERITY_ERROR, record->name};
                ls_capture_error(&error);
                result = LS_EAGAIN;
            }
        }
    }
    return result;
}
void ls_health_set_active_task(const char *name) {
    ls_copy_string(ls_runtime.active_task, sizeof ls_runtime.active_task, name);
}
void ls_watchdog_fed(void) {
    ls_runtime.watchdog_last_feed = ls_uptime_ms();
    (void)ls_blackbox_record_values(LS_BLACKBOX_WATCHDOG, ls_runtime.watchdog_checkpoint, 0u,
                                    (int32_t)ls_runtime.watchdog_last_feed, 1, 0, 0);
}
uint32_t ls_watchdog_last_feed_ms(void) {
    return ls_runtime.watchdog_last_feed;
}
void ls_watchdog_checkpoint(uint16_t checkpoint_id) {
    ls_runtime.watchdog_checkpoint = checkpoint_id;
    (void)ls_blackbox_record_values(LS_BLACKBOX_WATCHDOG, checkpoint_id, 0u, 0, 0, 0, 0);
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
    (void)ls_blackbox_record_values(LS_BLACKBOX_POWER, 0u, 0u, value.vdd_mv, value.battery_mv,
                                    value.current_ma, value.temperature_c);
#else
    (void)sample;
#endif
}
size_t ls_power_sample_count(void) {
    return ls_runtime.power_count;
}
ls_result_t ls_power_get_summary(ls_power_summary_t *summary) {
    if (!summary)
        return LS_EINVAL;
    ls_memset(summary, 0, sizeof(*summary));
#if LS_ENABLE_POWER_SAMPLES
    if (!ls_runtime.power_count)
        return LS_EAGAIN;
    summary->minimum_vdd_mv = UINT16_MAX;
    summary->minimum_battery_mv = UINT16_MAX;
    for (size_t count = 0u; count < ls_runtime.power_count; ++count) {
        size_t index = (ls_runtime.power_next + LS_POWER_SAMPLE_CAPACITY - ls_runtime.power_count + count) %
                       LS_POWER_SAMPLE_CAPACITY;
        const ls_power_sample_t *sample = &ls_runtime.power_samples[index];
        if (sample->vdd_mv < summary->minimum_vdd_mv)
            summary->minimum_vdd_mv = sample->vdd_mv;
        if (sample->battery_mv < summary->minimum_battery_mv)
            summary->minimum_battery_mv = sample->battery_mv;
        if (!count || sample->temperature_c > summary->maximum_temperature_c)
            summary->maximum_temperature_c = sample->temperature_c;
        if (!count || sample->current_ma > summary->maximum_current_ma)
            summary->maximum_current_ma = sample->current_ma;
        summary->last = *sample;
    }
    summary->sample_count = (uint32_t)ls_runtime.power_count;
    return LS_OK;
#else
    return LS_ENOTSUP;
#endif
}
bool ls_power_brownout_suspected(uint16_t threshold_mv, uint32_t within_ms) {
#if LS_ENABLE_POWER_SAMPLES
    if (!threshold_mv || !ls_runtime.power_count)
        return false;
    uint32_t now = ls_uptime_ms();
    for (size_t count = 0u; count < ls_runtime.power_count; ++count) {
        size_t index = (ls_runtime.power_next + LS_POWER_SAMPLE_CAPACITY - 1u - count) %
                       LS_POWER_SAMPLE_CAPACITY;
        const ls_power_sample_t *sample = &ls_runtime.power_samples[index];
        if (within_ms && now - sample->timestamp_ms > within_ms)
            break;
        if (sample->vdd_mv && sample->vdd_mv < threshold_mv)
            return true;
    }
#else
    (void)threshold_mv;
    (void)within_ms;
#endif
    return false;
}
