#ifndef LASTSTATE_LINUX_H
#define LASTSTATE_LINUX_H

#include <stddef.h>
#include <stdint.h>

#include "laststate/event.h"

/*
 * The Linux fatal-signal path deliberately writes this fixed, native-endian
 * record instead of an LEP envelope.  It is intended for a pre-opened,
 * nonblocking pipe/FIFO crash-collector endpoint; normal code may later
 * validate and convert it.  It does not promise file-system durability.
 */
#define LS_LINUX_SIGNAL_RECORD_MAGIC UINT32_C(0x4c535247) /* "LSRG" */
#define LS_LINUX_SIGNAL_RECORD_VERSION UINT16_C(1)
#define LS_LINUX_SIGNAL_RECORD_COMMIT UINT32_C(0x4c53434d) /* "LSCM" */
#define LS_LINUX_SIGNAL_RECORD_REGISTER_CAPACITY 32u

typedef enum {
    LS_LINUX_RAW_ARCH_UNKNOWN = 0,
    LS_LINUX_RAW_ARCH_X86_64 = 1,
    LS_LINUX_RAW_ARCH_AARCH64 = 2
} ls_linux_raw_architecture_t;

/* Canonical x86_64 register slots.  The values match Linux/glibc's gregs
 * order, including the packed CSGSFS slot. */
typedef enum {
    LS_LINUX_X86_64_R8 = 0,
    LS_LINUX_X86_64_R9,
    LS_LINUX_X86_64_R10,
    LS_LINUX_X86_64_R11,
    LS_LINUX_X86_64_R12,
    LS_LINUX_X86_64_R13,
    LS_LINUX_X86_64_R14,
    LS_LINUX_X86_64_R15,
    LS_LINUX_X86_64_RDI,
    LS_LINUX_X86_64_RSI,
    LS_LINUX_X86_64_RBP,
    LS_LINUX_X86_64_RBX,
    LS_LINUX_X86_64_RDX,
    LS_LINUX_X86_64_RAX,
    LS_LINUX_X86_64_RCX,
    LS_LINUX_X86_64_RSP,
    LS_LINUX_X86_64_RIP,
    LS_LINUX_X86_64_EFLAGS,
    LS_LINUX_X86_64_CSGSFS,
    LS_LINUX_X86_64_ERR,
    LS_LINUX_X86_64_TRAPNO,
    LS_LINUX_X86_64_OLDMASK,
    LS_LINUX_X86_64_CR2,
    LS_LINUX_X86_64_REGISTER_COUNT
} ls_linux_x86_64_register_t;

/* AArch64 slots 0..30 are x0..x30; slot 31 is PSTATE. */
#define LS_LINUX_AARCH64_PSTATE 31u
#define LS_LINUX_AARCH64_REGISTER_COUNT 32u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t architecture;
    int32_t signal_number;
    int32_t signal_code;
    uint32_t register_count;
    uint64_t fault_address;
    uint64_t pc;
    uint64_t sp;
    uint64_t fp;
    /* Address of this record while the handler ran.  It lets a collector
     * verify that SA_ONSTACK selected the registered alternate stack. */
    uint64_t handler_stack_pointer;
    uint64_t registers[LS_LINUX_SIGNAL_RECORD_REGISTER_CAPACITY];
    uint32_t crc32;
    uint32_t commit;
} ls_linux_signal_record_t;

typedef struct {
    /* Caller-owned, already-open O_NONBLOCK pipe or FIFO write descriptor.
     * A raw record fits in PIPE_BUF, which makes the fatal handler's one write
     * all-or-nothing rather than a partial file/socket record. */
    int record_fd;
    /* Caller-owned max_align_t-aligned storage installed for the calling
     * thread only. */
    void *alternate_stack;
    size_t alternate_stack_size;
} ls_linux_signal_config_t;

/* Installs an alternate signal stack for the calling thread.  Every thread
 * that can receive a handled fatal signal must register its own stack before
 * it does work that can fault.  This function is normal-runtime only. */
ls_result_t ls_linux_signal_register_alt_stack(void *alternate_stack, size_t alternate_stack_size);

/* Configures the process-wide crash descriptor and the calling thread's
 * alternate stack.  The descriptor must be a nonblocking pipe/FIFO write end
 * so a fatal handler cannot wait for a collector or make a partial record.
 * Call before ls_linux_install_signal_handlers; lifecycle/configuration calls
 * must be serialized with signal delivery. */
ls_result_t ls_linux_signal_configure(const ls_linux_signal_config_t *config);

/* Validates the version, committed marker, size, and CRC of a raw record.
 * This is normal-runtime code; it is not called by the fatal-signal path. */
bool ls_linux_signal_record_validate(const ls_linux_signal_record_t *record);

/* Installs SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGABRT handlers with SA_ONSTACK.
 * It fails until ls_linux_signal_configure has supplied a descriptor and the
 * calling thread has an enabled alternate signal stack.  The old API no
 * longer routes through the Latch runtime from a signal handler. */
ls_result_t ls_linux_install_signal_handlers(void);
void ls_linux_uninstall_signal_handlers(void);

#endif
