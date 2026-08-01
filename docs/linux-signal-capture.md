# Linux fatal-signal capture

The Linux port has a deliberately separate fatal-signal path. It does not call the Latch runtime, build an LEP envelope, touch a spool/storage backend, invoke callbacks, allocate memory or use stdio from a signal handler. Instead it writes one fixed `ls_linux_signal_record_t` to a caller-provided, already-open, nonblocking pipe/FIFO descriptor and exits the process.

This design makes a crash reporter process the preferred receiver: the reporter validates the raw record and persists/converts it in normal runtime. Regular files and sockets are rejected at configuration time: a single fatal-handler write could otherwise be short and leave an ambiguous record.

## Configure a process and its threads

Create a nonblocking pipe or FIFO before installing handlers, and give the thread an aligned alternate signal stack:

```c
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "linux.h"

static _Alignas(max_align_t) unsigned char crash_stack[65536];
int crash_pipe[2];
int crash_flags;

/* Normal runtime only: production code must check every return value. */
if (pipe(crash_pipe) != 0 ||
    (crash_flags = fcntl(crash_pipe[1], F_GETFL)) == -1 ||
    fcntl(crash_pipe[1], F_SETFL, crash_flags | O_NONBLOCK) == -1)
    exit(EXIT_FAILURE);
ls_linux_signal_config_t capture = {
    .record_fd = crash_pipe[1],
    .alternate_stack = crash_stack,
    .alternate_stack_size = sizeof(crash_stack),
};
if (ls_linux_signal_configure(&capture) != LS_OK ||
    ls_linux_install_signal_handlers() != LS_OK)
    exit(EXIT_FAILURE);
```

`ls_linux_signal_configure()` configures the process-wide descriptor and registers the alternate stack for the calling thread. Every other thread that may receive a handled fatal signal must call `ls_linux_signal_register_alt_stack()` with its own storage before it starts work that can fault. Configuration and installation are normal-runtime operations and must be serialized with signal delivery.

The legacy `ls_linux_install_signal_handlers()` function now fails with `LS_EINVAL` until this contract has been satisfied. It no longer falls through to the unsafe normal Latch capture path.

## Raw record contract

`ls_linux_signal_record_t` is native-endian and versioned. It carries magic, version, exact record size, architecture, signal number/code, full-width fault address, PC/SP/FP, the handler's stack address, up to 32 general/context register slots, CRC32 and a final commit marker. A receiver must call `ls_linux_signal_record_validate()` before using any field.

On x86_64, the register slots follow the documented Linux/glibc `gregs` order; on AArch64, slots 0-30 are `x0`-`x30` and slot 31 is `PSTATE`. Both retain full 64-bit values. Unsupported Linux architectures emit an `UNKNOWN` architecture record rather than inventing a register layout.

The record is not LEP and does not claim a durable Latch spool event. A normal-runtime collector may map it to an application incident or a future full-width Linux LEP extension after it has validated the record. Do not infer upper 64-bit values from the older 32-bit CPU/Fault TLVs.

## Failure and qualification boundaries

The handler makes exactly one `write()` attempt and then `_exit`s. The descriptor must be a nonblocking pipe/FIFO; `EAGAIN` or a closed receiver intentionally produces no retry or fallback in the crashed process. A record fits within `PIPE_BUF`, so a single writer is atomic at the pipe boundary.

The normal-runtime file backend is separate. Initialize its storage file with `ls_file_storage_init()` and use its explicit sync callback for spool durability; never pass it through the signal handler. It is a POSIX/Linux normal-runtime backend, deliberately not thread-safe per storage object, and opens backing paths with `O_NOFOLLOW`; serialize its use and provide real, non-symlink paths.

Before relying on this port in a product, run fork/pipe, controlled `SIGSEGV`, alternate-stack, blocked-callback and receiver-backpressure tests on the exact libc/kernel/architecture. Cross-compilation does not validate the kernel `ucontext_t` ABI, and a direct file descriptor still needs the product's power-loss and filesystem durability evidence.
