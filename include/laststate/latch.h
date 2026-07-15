#ifndef LASTSTATE_LATCH_H
#define LASTSTATE_LATCH_H

#include "config.h"
#include "identity.h"
#include "event.h"
#include "breadcrumb.h"
#include "metrics.h"
#include "storage.h"
#include "storage_sim.h"
#include "flash_storage.h"
#include "secure_storage.h"
#include "transport.h"
#include "stream_transport.h"
#include "network_transport.h"
#include "health.h"
#include "memory.h"
#include "peripheral.h"
#include "performance.h"
#include "log.h"
#include "assert.h"
#include "build_id.h"
#include "boot.h"
#include "envelope.h"
#include "capture.h"
#include "policy.h"
#include "compression.h"
#include "security.h"
#include "secure_element.h"
#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t (*ls_timestamp_fn)(void *context);
typedef void (*ls_reset_fn)(void *context);
typedef void (*ls_critical_fn)(void *context);
typedef struct {
    const ls_identity_t *identity;
    ls_architecture_t architecture;
    const char *rtos;
    const char *region;
    const char *device_group;
    ls_timestamp_fn timestamp_ms;
    void *timestamp_context;
    ls_reset_fn reset;
    void *reset_context;
    ls_reset_info_t (*reset_info)(void *context);
    void *reset_info_context;
    ls_critical_fn enter_critical;
    ls_critical_fn leave_critical;
    void *critical_context;
} ls_config_t;
ls_result_t ls_init(const ls_config_t *config);
ls_result_t ls_boot(void);
ls_result_t ls_flush(void);
void ls_capture_error(const ls_error_t *error);
void ls_capture_message(const char *message, ls_severity_t severity);
/* Normal-runtime only; not safe in an ISR or fault handler. */
ls_result_t ls_capture_cpu_context(const ls_arch_context_t *context);
bool ls_previous_boot_crashed(void);
bool ls_boot_loop_detected(void);
ls_reset_info_t ls_get_reset_info(void);
uint32_t ls_uptime_ms(void);
#if LS_ENABLE_ASSERTS
#define LS_ASSERT(expr)                                                                            \
    do {                                                                                           \
        if (!(expr))                                                                               \
            ls_assert_failed(#expr, __FILE__, __LINE__, 0);                                        \
    } while (0)
#define LS_ASSERT_MSG(expr, msg)                                                                   \
    do {                                                                                           \
        if (!(expr))                                                                               \
            ls_assert_failed(#expr, __FILE__, __LINE__, (msg));                                    \
    } while (0)
#else
#define LS_ASSERT(expr) ((void)0)
#define LS_ASSERT_MSG(expr, msg) ((void)0)
#endif
#ifdef __cplusplus
}
#endif
#endif
