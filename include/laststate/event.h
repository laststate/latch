#ifndef LASTSTATE_EVENT_H
#define LASTSTATE_EVENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    LS_OK = 0,
    LS_EINVAL = -1,
    LS_ENOSPACE = -2,
    LS_EIO = -3,
    LS_ECORRUPT = -4,
    LS_EAGAIN = -5,
    LS_ENOTSUP = -6,
    LS_EBUSY = -7,
    LS_EAUTH = -8,
    LS_EOVERFLOW = -9
} ls_result_t;
typedef enum {
    LS_PRIORITY_EMERGENCY,
    LS_PRIORITY_CRITICAL,
    LS_PRIORITY_ERROR,
    LS_PRIORITY_WARNING,
    LS_PRIORITY_DIAGNOSTIC
} ls_priority_t;
typedef enum {
    LS_SEVERITY_DEBUG,
    LS_SEVERITY_INFO,
    LS_SEVERITY_WARNING,
    LS_SEVERITY_ERROR,
    LS_SEVERITY_FATAL
} ls_severity_t;
typedef enum {
    LS_EVENT_CRASH = 1,
    LS_EVENT_ERROR,
    LS_EVENT_MESSAGE,
    LS_EVENT_HEALTH,
    LS_EVENT_RESET,
    LS_EVENT_LOG,
    LS_EVENT_PERIPHERAL,
    LS_EVENT_COREDUMP
} ls_event_type_t;
typedef enum {
    LS_ARCH_UNKNOWN,
    LS_ARCH_CORTEX_M,
    LS_ARCH_RISCV,
    LS_ARCH_XTENSA,
    LS_ARCH_LINUX
} ls_architecture_t;
typedef enum {
    LS_FAULT_UNKNOWN,
    LS_FAULT_HARD,
    LS_FAULT_MEMMANAGE,
    LS_FAULT_BUS,
    LS_FAULT_USAGE,
    LS_FAULT_NMI,
    LS_FAULT_SECURE,
    LS_FAULT_STACK,
    LS_FAULT_ASSERT,
    LS_FAULT_SIGNAL,
    LS_FAULT_TRAP
} ls_fault_kind_t;
typedef enum {
    LS_RESET_UNKNOWN,
    LS_RESET_POWER_ON,
    LS_RESET_PIN,
    LS_RESET_SOFTWARE,
    LS_RESET_WATCHDOG,
    LS_RESET_INDEPENDENT_WATCHDOG,
    LS_RESET_WINDOW_WATCHDOG,
    LS_RESET_BROWNOUT,
    LS_RESET_LOW_POWER_WAKE,
    LS_RESET_LOCKUP,
    LS_RESET_SECURITY,
    LS_RESET_BOOTLOADER,
    LS_RESET_FIRMWARE_UPDATE,
    LS_RESET_CLOCK_FAILURE
} ls_reset_reason_t;

typedef struct {
    ls_reset_reason_t reason;
    uint32_t raw_reason;
    uint32_t boot_count;
    uint32_t previous_uptime_ms;
    bool expected;
    bool crash_pending;
    bool boot_loop;
} ls_reset_info_t;
typedef struct {
    const char *domain;
    int32_t code;
    ls_severity_t severity;
    const char *message;
} ls_error_t;

typedef struct {
    ls_architecture_t architecture;
    ls_fault_kind_t fault;
    uint32_t registers[32];
    uint32_t r[13];
    uint32_t lr, pc, xpsr, msp, psp, control, primask, basepri, faultmask, exc_return;
    uint32_t cfsr, hfsr, dfsr, afsr, mmfar, bfar, shcsr, icsr, vtor, sfsr, sfar;
    uint32_t s[16], fpscr;
    uint32_t mcause, mtval, mstatus, mepc;
    uint32_t exccause, excvaddr, ps, sar;
    uintptr_t fault_address;
    int32_t signal_number;
    bool has_fpu;
    bool fpu_lazy;
} ls_arch_context_t;

typedef struct {
    const char *name;
    const void *address;
    size_t length;
    uint32_t flags;
} ls_dump_region_t;
enum { LS_DUMP_SAFE = 1u, LS_DUMP_HASH = 2u, LS_DUMP_VOLATILE = 4u, LS_DUMP_SENSITIVE = 8u };
#endif
