#ifndef LASTSTATE_CORTEX_M_HIL_H
#define LASTSTATE_CORTEX_M_HIL_H
#include <stddef.h>
#include <stdint.h>
#include "laststate/storage.h"
typedef struct {
    void *context;
    void (*start_watchdog)(void *context, uint32_t timeout_ms);
    void (*emit)(void *context, const char *line);
} ls_hil_platform_t;
void ls_hil_arm_brownout(const ls_hil_platform_t *platform);
void ls_hil_trigger_watchdog(const ls_hil_platform_t *platform, uint32_t timeout_ms);
ls_result_t ls_hil_trigger_mpu(uintptr_t protected_address, uint8_t region);
ls_result_t ls_hil_trigger_trustzone(uintptr_t secure_address);
void ls_hil_trigger_fpu_lazy_fault(void);
ls_result_t ls_hil_test_flash(ls_storage_backend_t *flash, size_t offset, size_t erase_length,
                              uint32_t pattern);
#endif
