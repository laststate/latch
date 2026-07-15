#include "esp_idf.h"
#if defined(ESP_PLATFORM)
#include "esp_system.h"
ls_reset_info_t ls_esp_idf_reset_info(void *context) {
    (void)context;
    esp_reset_reason_t reason = esp_reset_reason();
    ls_reset_info_t info = {LS_RESET_UNKNOWN, (uint32_t)reason, 0, 0, false, false, false};
    switch (reason) {
    case ESP_RST_POWERON:
        info.reason = LS_RESET_POWER_ON;
        break;
    case ESP_RST_EXT:
        info.reason = LS_RESET_PIN;
        break;
    case ESP_RST_SW:
        info.reason = LS_RESET_SOFTWARE;
        break;
    case ESP_RST_PANIC:
        info.reason = LS_RESET_LOCKUP;
        break;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        info.reason = LS_RESET_WATCHDOG;
        break;
    case ESP_RST_BROWNOUT:
        info.reason = LS_RESET_BROWNOUT;
        break;
    case ESP_RST_DEEPSLEEP:
        info.reason = LS_RESET_LOW_POWER_WAKE;
        break;
    default:
        break;
    }
    return info;
}
#else
ls_reset_info_t ls_esp_idf_reset_info(void *context) {
    (void)context;
    return (ls_reset_info_t){LS_RESET_UNKNOWN, 0, 0, 0, false, false, false};
}
#endif
