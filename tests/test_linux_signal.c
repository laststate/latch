#if !defined(__linux__)
int main(void) {
    return 0;
}
#else

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "linux.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "linux signal check failed: %s at line %d\n", #condition, __LINE__);   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

_Alignas(max_align_t) static uint8_t alternate_stack[65536u];
_Alignas(max_align_t) static uint8_t configuration_stack[65536u];
static uint8_t intentionally_unaligned_stack[65537u];

static int set_nonblocking(int descriptor) {
    int flags = fcntl(descriptor, F_GETFL);
    return flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) != 0 ? -1 : 0;
}

static int read_record(int descriptor, ls_linux_signal_record_t *record) {
    size_t received = 0u;
    while (received < sizeof(*record)) {
        ssize_t count = read(descriptor, (uint8_t *)record + received, sizeof(*record) - received);
        if (count <= 0)
            return -1;
        received += (size_t)count;
    }
    return 0;
}

static int test_normal_configuration(void) {
    int pipe_fds[2];
    int null_descriptor;
    ls_linux_signal_config_t config;

    CHECK(ls_linux_signal_configure(NULL) == LS_EINVAL);
    CHECK(ls_linux_signal_register_alt_stack(NULL, sizeof(configuration_stack)) == LS_EINVAL);
    CHECK(ls_linux_signal_register_alt_stack(configuration_stack, 1u) == LS_EINVAL);
    CHECK(ls_linux_signal_register_alt_stack(intentionally_unaligned_stack + 1u,
                                             sizeof(intentionally_unaligned_stack) - 1u) ==
          LS_EINVAL);
    CHECK(pipe(pipe_fds) == 0);
    config = (ls_linux_signal_config_t){
        .record_fd = -1,
        .alternate_stack = configuration_stack,
        .alternate_stack_size = sizeof(configuration_stack),
    };
    CHECK(ls_linux_signal_configure(&config) == LS_EINVAL);
    config.record_fd = pipe_fds[1];
    CHECK(ls_linux_signal_configure(&config) == LS_EINVAL);
    CHECK(set_nonblocking(pipe_fds[1]) == 0);
    null_descriptor = open("/dev/null", O_WRONLY | O_NONBLOCK);
    CHECK(null_descriptor >= 0);
    config.record_fd = null_descriptor;
    CHECK(ls_linux_signal_configure(&config) == LS_EINVAL);
    CHECK(close(null_descriptor) == 0);
    config.record_fd = pipe_fds[1];
    CHECK(ls_linux_signal_configure(&config) == LS_OK);
    CHECK(ls_linux_install_signal_handlers() == LS_OK);
    CHECK(ls_linux_install_signal_handlers() == LS_EBUSY);
    CHECK(ls_linux_signal_configure(&config) == LS_EBUSY);
    ls_linux_uninstall_signal_handlers();
    CHECK(close(pipe_fds[0]) == 0);
    CHECK(close(pipe_fds[1]) == 0);
    return 0;
}

int main(void) {
    int pipe_fds[2];
    pid_t child;
    int status = 0;
    ls_linux_signal_record_t record;

    CHECK(!ls_linux_signal_record_validate(NULL));
    CHECK(ls_linux_install_signal_handlers() == LS_EINVAL);
    CHECK(test_normal_configuration() == 0);
    CHECK(pipe(pipe_fds) == 0);
    CHECK(set_nonblocking(pipe_fds[1]) == 0);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        ls_linux_signal_config_t config = {
            .record_fd = pipe_fds[1],
            .alternate_stack = alternate_stack,
            .alternate_stack_size = sizeof(alternate_stack),
        };
        (void)close(pipe_fds[0]);
        if (ls_linux_signal_configure(&config) != LS_OK)
            _exit(90);
        if (ls_linux_install_signal_handlers() != LS_OK)
            _exit(91);
        (void)raise(SIGABRT);
        _exit(92);
    }

    (void)close(pipe_fds[1]);
    CHECK(read_record(pipe_fds[0], &record) == 0);
    (void)close(pipe_fds[0]);
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 128 + SIGABRT);
    CHECK(ls_linux_signal_record_validate(&record));
    CHECK(record.signal_number == SIGABRT);
    CHECK(record.handler_stack_pointer >= (uintptr_t)alternate_stack);
    CHECK(record.handler_stack_pointer < (uintptr_t)(alternate_stack + sizeof(alternate_stack)));
#if defined(__x86_64__)
    CHECK(record.architecture == LS_LINUX_RAW_ARCH_X86_64);
    CHECK(record.register_count == LS_LINUX_X86_64_REGISTER_COUNT);
    CHECK(record.pc == record.registers[LS_LINUX_X86_64_RIP]);
    CHECK(record.sp == record.registers[LS_LINUX_X86_64_RSP]);
    CHECK(record.pc != 0u && record.sp != 0u);
#elif defined(__aarch64__)
    CHECK(record.architecture == LS_LINUX_RAW_ARCH_AARCH64);
    CHECK(record.register_count == LS_LINUX_AARCH64_REGISTER_COUNT);
    CHECK(record.pc != 0u && record.sp != 0u);
#endif
    record.crc32 ^= 1u;
    CHECK(!ls_linux_signal_record_validate(&record));
    puts("linux signal raw-record tests passed");
    return 0;
}

#endif
