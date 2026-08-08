#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "linux.h"

#include <stddef.h>

#if defined(__clang__)
#define LS_LINUX_SIGNAL_SAFE                                                                       \
    __attribute__((noinline, no_instrument_function, no_profile_instrument_function,               \
                   no_stack_protector, no_sanitize("address"), no_sanitize("thread"),              \
                   no_sanitize("undefined")))
#elif defined(__GNUC__)
#define LS_LINUX_SIGNAL_SAFE                                                                       \
    __attribute__((noinline, no_instrument_function, no_profile_instrument_function,               \
                   no_stack_protector, no_sanitize_address, no_sanitize_undefined))
#else
#define LS_LINUX_SIGNAL_SAFE
#endif

_Static_assert(sizeof(ls_linux_signal_record_t) == 328u,
               "Linux raw signal record ABI unexpectedly changed");

/* The CRC is intentionally local to this port: the fatal-signal handler must
 * not call the normal runtime, storage, callback, or envelope paths. */
static LS_LINUX_SIGNAL_SAFE uint32_t signal_record_crc32(const volatile uint8_t *bytes,
                                                         size_t length) {
    uint32_t crc = UINT32_C(0xffffffff);
    for (size_t index = 0; index < length; ++index) {
        crc ^= bytes[index];
        for (unsigned bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1u) ^ (UINT32_C(0xedb88320) & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

bool ls_linux_signal_record_validate(const ls_linux_signal_record_t *record) {
    if (!record || record->magic != LS_LINUX_SIGNAL_RECORD_MAGIC ||
        record->version != LS_LINUX_SIGNAL_RECORD_VERSION || record->size != sizeof(*record) ||
        record->commit != LS_LINUX_SIGNAL_RECORD_COMMIT ||
        record->register_count > LS_LINUX_SIGNAL_RECORD_REGISTER_CAPACITY)
        return false;
    return record->crc32 == signal_record_crc32((const volatile uint8_t *)(const void *)record,
                                                offsetof(ls_linux_signal_record_t, crc32));
}

#if defined(__linux__)

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <ucontext.h>
#include <unistd.h>

static struct sigaction previous[5];
static const int handled_signals[5] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
/* sig_atomic_t lets the handler read the descriptor without invoking locks or
 * atomics that may not be lock-free.  Configuration is serialized by API
 * contract and rejected after installation. */
static volatile sig_atomic_t configured_record_fd = -1;
static volatile sig_atomic_t handlers_installed = 0;

#if defined(PIPE_BUF)
_Static_assert(sizeof(ls_linux_signal_record_t) <= PIPE_BUF,
               "raw signal record must fit in a single atomic pipe write");
#endif

static LS_LINUX_SIGNAL_SAFE void signal_record_zero(volatile ls_linux_signal_record_t *record) {
    record->magic = LS_LINUX_SIGNAL_RECORD_MAGIC;
    record->version = LS_LINUX_SIGNAL_RECORD_VERSION;
    record->size = (uint16_t)sizeof(*record);
    record->architecture = LS_LINUX_RAW_ARCH_UNKNOWN;
    record->signal_number = 0;
    record->signal_code = 0;
    record->register_count = 0u;
    record->fault_address = 0u;
    record->pc = 0u;
    record->sp = 0u;
    record->fp = 0u;
    record->handler_stack_pointer = (uint64_t)(uintptr_t)record;
    for (unsigned index = 0; index < LS_LINUX_SIGNAL_RECORD_REGISTER_CAPACITY; ++index)
        record->registers[index] = 0u;
    record->crc32 = 0u;
    record->commit = 0u;
}

static bool alt_stack_size_valid(size_t size) {
    long minimum = MINSIGSTKSZ;
    return minimum > 0 && size >= (size_t)minimum;
}

#if defined(__x86_64__)
static LS_LINUX_SIGNAL_SAFE void signal_record_x86_64(volatile ls_linux_signal_record_t *record,
                                                      const ucontext_t *machine) {
    const greg_t *registers = machine->uc_mcontext.gregs;
    record->architecture = LS_LINUX_RAW_ARCH_X86_64;
    record->register_count = LS_LINUX_X86_64_REGISTER_COUNT;
    record->registers[LS_LINUX_X86_64_R8] = (uint64_t)registers[REG_R8];
    record->registers[LS_LINUX_X86_64_R9] = (uint64_t)registers[REG_R9];
    record->registers[LS_LINUX_X86_64_R10] = (uint64_t)registers[REG_R10];
    record->registers[LS_LINUX_X86_64_R11] = (uint64_t)registers[REG_R11];
    record->registers[LS_LINUX_X86_64_R12] = (uint64_t)registers[REG_R12];
    record->registers[LS_LINUX_X86_64_R13] = (uint64_t)registers[REG_R13];
    record->registers[LS_LINUX_X86_64_R14] = (uint64_t)registers[REG_R14];
    record->registers[LS_LINUX_X86_64_R15] = (uint64_t)registers[REG_R15];
    record->registers[LS_LINUX_X86_64_RDI] = (uint64_t)registers[REG_RDI];
    record->registers[LS_LINUX_X86_64_RSI] = (uint64_t)registers[REG_RSI];
    record->registers[LS_LINUX_X86_64_RBP] = (uint64_t)registers[REG_RBP];
    record->registers[LS_LINUX_X86_64_RBX] = (uint64_t)registers[REG_RBX];
    record->registers[LS_LINUX_X86_64_RDX] = (uint64_t)registers[REG_RDX];
    record->registers[LS_LINUX_X86_64_RAX] = (uint64_t)registers[REG_RAX];
    record->registers[LS_LINUX_X86_64_RCX] = (uint64_t)registers[REG_RCX];
    record->registers[LS_LINUX_X86_64_RSP] = (uint64_t)registers[REG_RSP];
    record->registers[LS_LINUX_X86_64_RIP] = (uint64_t)registers[REG_RIP];
    record->registers[LS_LINUX_X86_64_EFLAGS] = (uint64_t)registers[REG_EFL];
    record->registers[LS_LINUX_X86_64_CSGSFS] = (uint64_t)registers[REG_CSGSFS];
    record->registers[LS_LINUX_X86_64_ERR] = (uint64_t)registers[REG_ERR];
    record->registers[LS_LINUX_X86_64_TRAPNO] = (uint64_t)registers[REG_TRAPNO];
    record->registers[LS_LINUX_X86_64_OLDMASK] = (uint64_t)registers[REG_OLDMASK];
    record->registers[LS_LINUX_X86_64_CR2] = (uint64_t)registers[REG_CR2];
    record->pc = record->registers[LS_LINUX_X86_64_RIP];
    record->sp = record->registers[LS_LINUX_X86_64_RSP];
    record->fp = record->registers[LS_LINUX_X86_64_RBP];
}
#elif defined(__aarch64__)
static LS_LINUX_SIGNAL_SAFE void signal_record_aarch64(volatile ls_linux_signal_record_t *record,
                                                       const ucontext_t *machine) {
    record->architecture = LS_LINUX_RAW_ARCH_AARCH64;
    record->register_count = LS_LINUX_AARCH64_REGISTER_COUNT;
    for (unsigned index = 0; index < 31u; ++index)
        record->registers[index] = machine->uc_mcontext.regs[index];
    record->registers[LS_LINUX_AARCH64_PSTATE] = machine->uc_mcontext.pstate;
    record->pc = machine->uc_mcontext.pc;
    record->sp = machine->uc_mcontext.sp;
    record->fp = machine->uc_mcontext.regs[29];
}
#endif

/* This is the only fatal-signal path.  Do not add runtime, allocation,
 * callback, storage, stdio, or retry work here: a single async-signal-safe
 * write is attempted and the process exits immediately. */
static LS_LINUX_SIGNAL_SAFE void signal_handler(int number, siginfo_t *info, void *opaque) {
    volatile ls_linux_signal_record_t record;
    int descriptor = configured_record_fd;

    signal_record_zero(&record);
    record.signal_number = number;
    record.signal_code = info ? info->si_code : 0;
    if (info && (number == SIGSEGV || number == SIGBUS || number == SIGILL || number == SIGFPE))
        record.fault_address = (uint64_t)(uintptr_t)info->si_addr;
#if defined(__x86_64__)
    if (opaque)
        signal_record_x86_64(&record, (const ucontext_t *)opaque);
#elif defined(__aarch64__)
    if (opaque)
        signal_record_aarch64(&record, (const ucontext_t *)opaque);
#else
    (void)opaque;
#endif
    record.crc32 = signal_record_crc32((const volatile uint8_t *)(const void *)&record,
                                       offsetof(ls_linux_signal_record_t, crc32));
    record.commit = LS_LINUX_SIGNAL_RECORD_COMMIT;
    if (descriptor >= 0) {
        ssize_t written = write(descriptor, (const void *)&record, sizeof(record));
        (void)written;
    }
    _exit(128 + number);
}

ls_result_t ls_linux_signal_register_alt_stack(void *alternate_stack, size_t alternate_stack_size) {
    stack_t stack;
    if (!alternate_stack || !alt_stack_size_valid(alternate_stack_size) ||
        ((uintptr_t)alternate_stack % _Alignof(max_align_t)) != 0u)
        return LS_EINVAL;
    stack.ss_sp = alternate_stack;
    stack.ss_size = alternate_stack_size;
    stack.ss_flags = 0;
    return sigaltstack(&stack, 0) == 0 ? LS_OK : LS_EIO;
}

ls_result_t ls_linux_signal_configure(const ls_linux_signal_config_t *config) {
    int flags;
    struct stat status;
    if (!config || config->record_fd < 0 ||
        (uintmax_t)config->record_fd > (uintmax_t)SIG_ATOMIC_MAX || handlers_installed)
        return handlers_installed ? LS_EBUSY : LS_EINVAL;
    flags = fcntl(config->record_fd, F_GETFL);
    if (flags < 0)
        return LS_EIO;
    if ((flags & O_NONBLOCK) == 0)
        return LS_EINVAL;
    /* A record is smaller than PIPE_BUF, so a nonblocking pipe/FIFO gives the
     * handler all-or-nothing delivery.  Do not accept a regular file or socket
     * here: a short write could otherwise leave an ambiguous raw record. */
    if (fstat(config->record_fd, &status) != 0 || !S_ISFIFO(status.st_mode))
        return LS_EINVAL;
    ls_result_t result =
        ls_linux_signal_register_alt_stack(config->alternate_stack, config->alternate_stack_size);
    if (result != LS_OK)
        return result;
    configured_record_fd = (sig_atomic_t)config->record_fd;
    return LS_OK;
}

ls_result_t ls_linux_install_signal_handlers(void) {
    struct sigaction action;
    stack_t stack;
    unsigned installed = 0u;
    if (handlers_installed)
        return LS_EBUSY;
    if (configured_record_fd < 0 || sigaltstack(0, &stack) != 0 ||
        (stack.ss_flags & SS_DISABLE) != 0 || !alt_stack_size_valid(stack.ss_size))
        return LS_EINVAL;
    action.sa_sigaction = signal_handler;
    action.sa_flags = (int)(SA_SIGINFO | SA_ONSTACK | SA_RESETHAND);
    if (sigfillset(&action.sa_mask) != 0)
        return LS_EIO;
    for (; installed < sizeof(handled_signals) / sizeof(handled_signals[0]); ++installed) {
        if (sigaction(handled_signals[installed], &action, &previous[installed]) != 0) {
            while (installed > 0u) {
                --installed;
                (void)sigaction(handled_signals[installed], &previous[installed], 0);
            }
            return LS_EIO;
        }
    }
    handlers_installed = 1;
    return LS_OK;
}

void ls_linux_uninstall_signal_handlers(void) {
    if (!handlers_installed)
        return;
    for (unsigned index = 0; index < sizeof(handled_signals) / sizeof(handled_signals[0]); ++index)
        (void)sigaction(handled_signals[index], &previous[index], 0);
    handlers_installed = 0;
}

#else

ls_result_t ls_linux_signal_register_alt_stack(void *alternate_stack, size_t alternate_stack_size) {
    (void)alternate_stack;
    (void)alternate_stack_size;
    return LS_ENOTSUP;
}

ls_result_t ls_linux_signal_configure(const ls_linux_signal_config_t *config) {
    (void)config;
    return LS_ENOTSUP;
}

ls_result_t ls_linux_install_signal_handlers(void) {
    return LS_ENOTSUP;
}

void ls_linux_uninstall_signal_handlers(void) {
}

#endif
