#include "internal.h"
#include "laststate/supervisor.h"
#include "laststate/blackbox.h"

static uint32_t power_alarms(const ls_supervisor_config_t *config) {
    if (!ls_runtime.power_count) {
        return 0u;
    }
    size_t index =
        (ls_runtime.power_next + LS_POWER_SAMPLE_CAPACITY - 1u) % LS_POWER_SAMPLE_CAPACITY;
    const ls_power_sample_t *sample = &ls_runtime.power_samples[index];
    uint32_t alarms = 0u;
    if (config->minimum_vdd_mv && sample->vdd_mv < config->minimum_vdd_mv) {
        alarms |= LS_SUPERVISOR_VDD_LOW;
    }
    if (config->minimum_battery_mv && sample->battery_mv < config->minimum_battery_mv) {
        alarms |= LS_SUPERVISOR_BATTERY_LOW;
    }
    if (config->maximum_temperature_c && sample->temperature_c > config->maximum_temperature_c) {
        alarms |= LS_SUPERVISOR_TEMPERATURE_HIGH;
    }
    return alarms;
}

ls_result_t ls_supervisor_configure(const ls_supervisor_config_t *config) {
    if (!config || config->maximum_spool_percent > 100u) {
        return LS_EINVAL;
    }
    ls_runtime.supervisor_config = *config;
    ls_runtime.supervisor_status.configured = true;
    return LS_OK;
}

ls_result_t ls_supervisor_poll(void) {
    if (!ls_runtime.supervisor_status.configured) {
        return LS_ENOTSUP;
    }
    uint32_t alarms = power_alarms(&ls_runtime.supervisor_config);
    uint32_t now = ls_uptime_ms();
    if (ls_runtime.supervisor_config.watchdog_max_stale_ms && ls_runtime.watchdog_last_feed &&
        now - ls_runtime.watchdog_last_feed > ls_runtime.supervisor_config.watchdog_max_stale_ms) {
        alarms |= LS_SUPERVISOR_WATCHDOG_STALE;
    }
    if (ls_runtime.supervisor_config.minimum_heap_free_bytes &&
        ls_runtime.heap_stats.free_bytes < ls_runtime.supervisor_config.minimum_heap_free_bytes) {
        alarms |= LS_SUPERVISOR_HEAP_LOW;
    }
    if (ls_runtime.boot_loop) {
        alarms |= LS_SUPERVISOR_BOOT_LOOP;
    }
    (void)ls_health_poll();
    for (size_t index = 0u; index < ls_runtime.health_count; ++index) {
        if (ls_runtime.health[index].expired) {
            alarms |= LS_SUPERVISOR_HEALTH_DEADLINE;
            break;
        }
    }
    if (ls_runtime.environment.sample_count) {
        const ls_environment_sample_t *environment = &ls_runtime.environment.last;
        if (environment->flags & (LS_ENV_LEAK_DETECTED | LS_ENV_WATER_INGRESS)) {
            alarms |= LS_SUPERVISOR_ENVIRONMENT_LEAK;
        }
        if (environment->flags & LS_ENV_PRESSURE_SENSOR_FAULT) {
            alarms |= LS_SUPERVISOR_ENV_SENSOR_FAULT;
        }
        if ((environment->flags & LS_ENV_VIBRATION_LIMIT) ||
            (ls_runtime.supervisor_config.maximum_vibration_mg_rms &&
             environment->vibration_mg_rms >
                 ls_runtime.supervisor_config.maximum_vibration_mg_rms)) {
            alarms |= LS_SUPERVISOR_VIBRATION_HIGH;
        }
    }
    if (ls_runtime.supervisor_config.maximum_spool_percent) {
        ls_spool_stats_t stats;
        if (ls_spool_get_stats(&stats) == LS_OK && stats.capacity &&
            stats.committed * 100u >=
                stats.capacity * ls_runtime.supervisor_config.maximum_spool_percent) {
            alarms |= LS_SUPERVISOR_SPOOL_HIGH;
        }
    }

    uint32_t previous = ls_runtime.supervisor_status.active_alarms;
    ls_runtime.supervisor_status.previous_alarms = previous;
    ls_runtime.supervisor_status.active_alarms = alarms;
    if (previous != alarms) {
        if (ls_runtime.supervisor_status.transitions != UINT32_MAX) {
            ls_runtime.supervisor_status.transitions++;
        }
        ls_runtime.supervisor_status.last_change_ms = now;
        (void)ls_blackbox_record_values(LS_BLACKBOX_STATE, 0xff00u,
                                        alarms ? LS_BLACKBOX_ERROR : LS_BLACKBOX_IMPORTANT,
                                        (int32_t)previous, (int32_t)alarms, 0, 0);
        if (alarms && ls_runtime.supervisor_config.anomaly_hold_ms) {
            ls_blackbox_anomaly_begin(ls_runtime.supervisor_config.anomaly_hold_ms);
        }
        if (alarms & ~previous) {
            ls_error_t error = {"supervisor", (int32_t)(alarms & ~previous), LS_SEVERITY_ERROR,
                                "health_alarm"};
            ls_capture_error(&error);
        }
    }
    ls_blackbox_poll();
    return alarms ? LS_EAGAIN : LS_OK;
}

ls_supervisor_status_t ls_supervisor_get_status(void) {
    return ls_runtime.supervisor_status;
}
