#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include "linux.h"
#include "laststate/latch.h"
static struct sigaction previous[5];
static const int signals[5] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
static void handler(int number, siginfo_t *info, void *opaque) {
    ucontext_t *machine = (ucontext_t *)opaque;
    ls_arch_context_t context;
    memset(&context, 0, sizeof context);
    context.architecture = LS_ARCH_LINUX;
    context.fault = LS_FAULT_SIGNAL;
    context.signal_number = number;
    context.fault_address = (uintptr_t)(info ? info->si_addr : 0);
#if defined(__x86_64__)
    context.pc = (uint32_t)machine->uc_mcontext.gregs[REG_RIP];
    context.msp = (uint32_t)machine->uc_mcontext.gregs[REG_RSP];
    context.lr = (uint32_t)machine->uc_mcontext.gregs[REG_RBP];
#elif defined(__aarch64__)
    context.pc = (uint32_t)machine->uc_mcontext.pc;
    context.msp = (uint32_t)machine->uc_mcontext.sp;
    for (unsigned i = 0; i < 31 && i < 32; i++)
        context.registers[i] = (uint32_t)machine->uc_mcontext.regs[i];
#endif
    (void)ls_capture_cpu_context(&context);
    _Exit(128 + number);
}
ls_result_t ls_linux_install_signal_handlers(void) {
    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_sigaction = handler;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    for (unsigned i = 0; i < 5; i++)
        if (sigaction(signals[i], &action, &previous[i]) != 0)
            return LS_EIO;
    return LS_OK;
}
void ls_linux_uninstall_signal_handlers(void) {
    for (unsigned i = 0; i < 5; i++)
        (void)sigaction(signals[i], &previous[i], 0);
}
#else
#include "linux.h"
ls_result_t ls_linux_install_signal_handlers(void) {
    return LS_ENOTSUP;
}
void ls_linux_uninstall_signal_handlers(void) {
}
#endif
