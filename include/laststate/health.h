#ifndef LASTSTATE_HEALTH_H
#define LASTSTATE_HEALTH_H

#include <stdbool.h>
#include <stdint.h>
#include "event.h"
typedef struct {
    const char *name;
    uint32_t deadline_ms;
    uint32_t last_touch_ms;
    bool expired;
} ls_health_t;
typedef struct {
    uint32_t timestamp_ms;
    uint16_t vdd_mv, battery_mv;
    int16_t current_ma, temperature_c;
    uint16_t charger_status, power_flags;
} ls_power_sample_t;

void ls_health_register(const char *name, uint32_t deadline_ms);
void ls_health_touch(const char *name);
ls_result_t ls_health_poll(void);
void ls_health_set_active_task(const char *name);
void ls_watchdog_fed(void);
uint32_t ls_watchdog_last_feed_ms(void);
void ls_watchdog_checkpoint(uint16_t checkpoint_id);
uint16_t ls_watchdog_last_checkpoint(void);
void ls_power_sample(const ls_power_sample_t *sample);
size_t ls_power_sample_count(void);
#endif
