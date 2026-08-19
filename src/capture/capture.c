// SPDX-License-Identifier: Apache-2.0
// Copyright 2024-2026 LastState Contributors
// src/capture/capture.c
//
// Crash capture implementation. Serializes the fault context, CPU
// registers, and dump regions into the minimal retained snapshot
// that survives a CPU reset.
//
// Heap-free, bounded, deterministic.

#include "../core/internal.h"
#include "laststate/noinit.h"

#define LS_MINIMAL_MAGIC 0x4d534c53u
#define LS_WIDE_CONTEXT_MAGIC 0x3656534cu

static LS_NOINIT volatile ls_minimal_snapshot_t minimal_snapshot;
#if LS_ENABLE_WIDE_CONTEXT
static LS_NOINIT volatile ls_wide_context_snapshot_t wide_context_snapshot;
#endif
static uint32_t minimal_build_hash;
/* Kept outside retained memory: it distinguishes nested RV64 traps during one
 * boot, while a reboot only needs the matching values already committed in
 * the two retained records. */
static volatile uint32_t riscv64_fault_capture_sequence;

static uint32_t minimal_crc32_update(uint32_t crc, const volatile uint8_t *data, size_t length) {
    while (length-- != 0u) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8u; ++bit) {
            crc = (crc >> 1u) ^ (0xedb88320u & (uint32_t)(-(int32_t)(crc & 1u)));
        }
    }
    return crc;
}

static uint32_t minimal_snapshot_fault_crc(const volatile ls_minimal_snapshot_t *snapshot,
                                           size_t length) {
    const uint32_t magic = LS_MINIMAL_MAGIC;
    uint32_t crc = 0xffffffffu;

    if (length < sizeof magic) {
        return 0u;
    }

    crc = minimal_crc32_update(crc, (const volatile uint8_t *)(const void *)&magic, sizeof magic);
    crc = minimal_crc32_update(crc, (const volatile uint8_t *)(const void *)&snapshot->version,
                               length - sizeof magic);
    return ~crc;
}

#if LS_ENABLE_WIDE_CONTEXT
static uint32_t wide_snapshot_fault_crc(const volatile ls_wide_context_snapshot_t *snapshot,
                                        size_t length) {
    const uint32_t magic = LS_WIDE_CONTEXT_MAGIC;
    uint32_t crc = 0xffffffffu;

    if (length < sizeof magic) {
        return 0u;
    }

    crc = minimal_crc32_update(crc, (const volatile uint8_t *)(const void *)&magic, sizeof magic);
    crc = minimal_crc32_update(crc, (const volatile uint8_t *)(const void *)&snapshot->version,
                               length - sizeof magic);
    return ~crc;
}

static bool wide_snapshot_store(const uint64_t registers[32], uint64_t mstatus, uint64_t mcause,
                                uint64_t mtval, uint64_t mepc, uint32_t fault_sequence) {
    volatile ls_wide_context_snapshot_t *snapshot = &wide_context_snapshot;

    if (!registers) {
        snapshot->magic = 0u;
        return false;
    }

    snapshot->magic = 0u;
    snapshot->version = LS_WIDE_CONTEXT_SNAPSHOT_VERSION;
    snapshot->architecture = (uint32_t)LS_ARCH_RISCV64;
    snapshot->fault = (uint32_t)LS_FAULT_TRAP;
    snapshot->build_hash = minimal_build_hash;
    snapshot->fault_sequence = fault_sequence;
    for (unsigned index = 0u; index < 32u; ++index) {
        snapshot->context.x[index] = registers[index];
    }
    snapshot->context.mstatus = mstatus;
    snapshot->context.mcause = mcause;
    snapshot->context.mtval = mtval;
    snapshot->context.mepc = mepc;
    snapshot->crc = wide_snapshot_fault_crc(snapshot, offsetof(ls_wide_context_snapshot_t, crc));
    snapshot->magic = LS_WIDE_CONTEXT_MAGIC;
    return true;
}

static void wide_snapshot_clear(void) {
    wide_context_snapshot.magic = 0u;
}

#else
static bool wide_snapshot_store(const uint64_t registers[32], uint64_t mstatus, uint64_t mcause,
                                uint64_t mtval, uint64_t mepc, uint32_t fault_sequence) {
    (void)registers;
    (void)mstatus;
    (void)mcause;
    (void)mtval;
    (void)mepc;
    (void)fault_sequence;
    return false;
}

static void wide_snapshot_clear(void) {
}
#endif

static uint32_t riscv64_fault_sequence_next(void) {
    uint32_t sequence = riscv64_fault_capture_sequence + 1u;

    if (sequence == 0u) {
        sequence = 1u;
    }
    riscv64_fault_capture_sequence = sequence;
    return sequence;
}

static void minimal_snapshot_store(uint32_t pc, uint32_t lr, uint32_t msp, uint32_t psp,
                                   uint32_t cfsr, uint32_t hfsr, ls_fault_kind_t fault,
                                   uint32_t exc_return, uint32_t xpsr, uint32_t fpscr,
                                   uint32_t flags, uint32_t emergency_stack_used,
                                   uint32_t fault_sequence, const ls_arch_context_t *context) {
    volatile ls_minimal_snapshot_t *snapshot = &minimal_snapshot;
    ls_blackbox_freeze();

    snapshot->magic = 0u;
    snapshot->version = LS_MINIMAL_SNAPSHOT_VERSION;
    snapshot->pc = pc;
    snapshot->lr = lr;
    snapshot->msp = msp;
    snapshot->psp = psp;
    snapshot->cfsr = cfsr;
    snapshot->hfsr = hfsr;
    snapshot->build_hash = minimal_build_hash;
    snapshot->crc = 0u;
    snapshot->fault = (uint32_t)fault;
    snapshot->flags = flags;
    snapshot->exc_return = exc_return;
    snapshot->xpsr = xpsr;
    snapshot->fpscr = fpscr;
    snapshot->emergency_stack_used = emergency_stack_used;
    snapshot->fault_sequence = fault_sequence;
    snapshot->extension_crc = 0u;
    snapshot->architecture = context ? (uint32_t)context->architecture : (uint32_t)LS_ARCH_UNKNOWN;
    for (unsigned index = 0; index < 16u; ++index) {
        snapshot->registers[index] = context ? context->registers[index] : 0u;
    }
    snapshot->ps = context ? context->ps : 0u;
    snapshot->sar = context ? context->sar : 0u;
    snapshot->exccause = context ? context->exccause : 0u;
    snapshot->excvaddr = context ? context->excvaddr : 0u;
    snapshot->context_crc = 0u;

    snapshot->crc = minimal_snapshot_fault_crc(snapshot, offsetof(ls_minimal_snapshot_t, crc));
    snapshot->extension_crc =
        minimal_snapshot_fault_crc(snapshot, offsetof(ls_minimal_snapshot_t, extension_crc));
    snapshot->context_crc =
        minimal_snapshot_fault_crc(snapshot, offsetof(ls_minimal_snapshot_t, context_crc));
    snapshot->magic = LS_MINIMAL_MAGIC;
}

void ls_capture_minimal_prepare(void) {
    minimal_build_hash = ls_hash_string(ls_build_id());
    riscv64_fault_capture_sequence = 0u;
}

void ls_capture_minimal_fault(uint32_t pc, uint32_t lr, uint32_t msp, uint32_t psp, uint32_t cfsr,
                              uint32_t hfsr, ls_fault_kind_t fault, uint32_t exc_return,
                              uint32_t xpsr, uint32_t fpscr, uint32_t flags,
                              uint32_t emergency_stack_used, uint32_t fault_sequence) {
    wide_snapshot_clear();
    minimal_snapshot_store(pc, lr, msp, psp, cfsr, hfsr, fault, exc_return, xpsr, fpscr, flags,
                           emergency_stack_used, fault_sequence, 0);
}

void ls_capture_minimal_context_fault(const ls_arch_context_t *context) {
    uint32_t flags = 0u;
    if (!context) {
        return;
    }
    if (context->has_fpu)
        flags |= LS_MINIMAL_SNAPSHOT_FPU_FRAME;
    if (context->fpu_lazy)
        flags |= LS_MINIMAL_SNAPSHOT_FPU_LAZY;
    wide_snapshot_clear();
    minimal_snapshot_store(context->pc, context->lr, context->msp, context->psp, context->cfsr,
                           context->hfsr, context->fault, context->exc_return, context->xpsr,
                           context->fpscr, flags, 0u, 0u, context);
}

void ls_capture_minimal_riscv64_fault(const uint64_t registers[32], uint64_t mstatus,
                                      uint64_t mcause, uint64_t mtval, uint64_t mepc) {
    volatile ls_minimal_snapshot_t *snapshot = &minimal_snapshot;
    uint32_t fault_sequence;
    uint32_t flags;

    if (!registers) {
        return;
    }

    fault_sequence = riscv64_fault_sequence_next();
    flags = wide_snapshot_store(registers, mstatus, mcause, mtval, mepc, fault_sequence)
                ? LS_MINIMAL_SNAPSHOT_WIDE_CONTEXT_VALID
                : 0u;

    /* Keep the v1/v2/v3 record byte-for-byte layout. The low words provide
       a legacy summary, while the committed sidecar carries the complete
       RV64 register bank. This function intentionally constructs no large
       automatic context: it is callable directly from the trap path. */
    snapshot->magic = 0u;
    snapshot->version = LS_MINIMAL_SNAPSHOT_VERSION;
    snapshot->pc = (uint32_t)mepc;
    snapshot->lr = (uint32_t)registers[1];
    snapshot->msp = (uint32_t)registers[2];
    snapshot->psp = 0u;
    snapshot->cfsr = 0u;
    snapshot->hfsr = 0u;
    snapshot->build_hash = minimal_build_hash;
    snapshot->crc = 0u;
    snapshot->fault = (uint32_t)LS_FAULT_TRAP;
    snapshot->flags = flags;
    snapshot->exc_return = 0u;
    snapshot->xpsr = 0u;
    snapshot->fpscr = 0u;
    snapshot->emergency_stack_used = 0u;
    snapshot->fault_sequence = fault_sequence;
    snapshot->extension_crc = 0u;
    snapshot->architecture = (uint32_t)LS_ARCH_RISCV64;
    for (unsigned index = 0u; index < 16u; ++index) {
        snapshot->registers[index] = (uint32_t)registers[index];
    }
    snapshot->ps = 0u;
    snapshot->sar = 0u;
    snapshot->exccause = 0u;
    snapshot->excvaddr = (uint32_t)mtval;
    snapshot->context_crc = 0u;

    snapshot->crc = minimal_snapshot_fault_crc(snapshot, offsetof(ls_minimal_snapshot_t, crc));
    snapshot->extension_crc =
        minimal_snapshot_fault_crc(snapshot, offsetof(ls_minimal_snapshot_t, extension_crc));
    snapshot->context_crc =
        minimal_snapshot_fault_crc(snapshot, offsetof(ls_minimal_snapshot_t, context_crc));
    snapshot->magic = LS_MINIMAL_MAGIC;
}

ls_result_t ls_capture_minimal(const ls_arch_context_t *context) {
    if (!context) {
        return LS_EINVAL;
    }

    ls_capture_minimal_context_fault(context);
    return LS_OK;
}

bool ls_minimal_snapshot_read(ls_minimal_snapshot_t *snapshot) {
    ls_minimal_snapshot_t copy;

    if (!snapshot) {
        return false;
    }

    /* Keep the conservative zero-iteration case well-defined for analyzers;
       the real bound is the non-zero compile-time size of the snapshot. */
    copy.magic = 0u;
    const volatile uint8_t *source = (const volatile uint8_t *)(const void *)&minimal_snapshot;
    uint8_t *destination = (uint8_t *)(void *)&copy;
    for (size_t index = 0; index < sizeof copy; ++index) {
        destination[index] = source[index];
    }

    if (!ls_minimal_snapshot_validate(&copy)) {
        return false;
    }

    ls_memcpy(snapshot, &copy, sizeof copy);
    return true;
}

bool ls_wide_context_snapshot_read(ls_wide_context_snapshot_t *snapshot) {
#if LS_ENABLE_WIDE_CONTEXT
    ls_wide_context_snapshot_t copy;

    if (!snapshot) {
        return false;
    }

    copy.magic = 0u;
    const volatile uint8_t *source = (const volatile uint8_t *)(const void *)&wide_context_snapshot;
    uint8_t *destination = (uint8_t *)(void *)&copy;
    for (size_t index = 0u; index < sizeof copy; ++index) {
        destination[index] = source[index];
    }

    if (copy.magic != LS_WIDE_CONTEXT_MAGIC || copy.version != LS_WIDE_CONTEXT_SNAPSHOT_VERSION ||
        copy.architecture != (uint32_t)LS_ARCH_RISCV64 || copy.fault != (uint32_t)LS_FAULT_TRAP ||
        copy.crc != ls_crc32(&copy, offsetof(ls_wide_context_snapshot_t, crc))) {
        return false;
    }

    ls_memcpy(snapshot, &copy, sizeof copy);
    return true;
#else
    (void)snapshot;
    return false;
#endif
}

bool ls_minimal_snapshot_validate(ls_minimal_snapshot_t *snapshot) {
    if (!snapshot || snapshot->magic != LS_MINIMAL_MAGIC) {
        return false;
    }

    uint32_t prefix_crc = ls_crc32(snapshot, offsetof(ls_minimal_snapshot_t, crc));
    if (snapshot->version == 1u) {
        if (snapshot->crc != prefix_crc) {
            return false;
        }
        ls_memset((uint8_t *)snapshot + offsetof(ls_minimal_snapshot_t, fault), 0,
                  sizeof *snapshot - offsetof(ls_minimal_snapshot_t, fault));
    } else if (snapshot->version == 2u) {
        uint32_t extension_crc = ls_crc32(snapshot, offsetof(ls_minimal_snapshot_t, extension_crc));

        if (snapshot->crc != prefix_crc || snapshot->extension_crc != extension_crc) {
            return false;
        }
        ls_memset((uint8_t *)snapshot + offsetof(ls_minimal_snapshot_t, architecture), 0,
                  sizeof *snapshot - offsetof(ls_minimal_snapshot_t, architecture));
    } else if (snapshot->version == LS_MINIMAL_SNAPSHOT_VERSION) {
        uint32_t extension_crc = ls_crc32(snapshot, offsetof(ls_minimal_snapshot_t, extension_crc));
        uint32_t context_crc = ls_crc32(snapshot, offsetof(ls_minimal_snapshot_t, context_crc));
        if (snapshot->crc != prefix_crc || snapshot->extension_crc != extension_crc ||
            snapshot->context_crc != context_crc) {
            return false;
        }
    } else {
        return false;
    }
    return true;
}

void ls_minimal_snapshot_clear(void) {
    minimal_snapshot.magic = 0u;
    wide_snapshot_clear();
}

static bool wide_snapshot_matches_minimal(const ls_wide_context_snapshot_t *wide,
                                          const ls_minimal_snapshot_t *minimal) {
    return wide && minimal && wide->architecture == minimal->architecture &&
           wide->fault == minimal->fault && wide->build_hash == minimal->build_hash &&
           wide->fault_sequence == minimal->fault_sequence &&
           (uint32_t)wide->context.x[1] == minimal->lr &&
           (uint32_t)wide->context.x[2] == minimal->msp &&
           (uint32_t)wide->context.mtval == minimal->excvaddr &&
           (uint32_t)wide->context.mepc == minimal->pc;
}

static void riscv64_context_to_arch(const ls_riscv64_context_t *source, ls_fault_kind_t fault,
                                    ls_arch_context_t *destination) {
    *destination = (ls_arch_context_t){
        .architecture = LS_ARCH_RISCV64,
        .fault = fault,
        .lr = (uint32_t)source->x[1],
        .pc = (uint32_t)source->mepc,
        .msp = (uint32_t)source->x[2],
        .mcause = (uint32_t)source->mcause,
        .mtval = (uint32_t)source->mtval,
        .mstatus = (uint32_t)source->mstatus,
        .mepc = (uint32_t)source->mepc,
        .fault_address = (uintptr_t)source->mtval,
    };
    for (unsigned index = 0u; index < 32u; ++index) {
        destination->registers[index] = (uint32_t)source->x[index];
    }
}

ls_result_t ls_capture_minimal_recover(void) {
    ls_minimal_snapshot_t snapshot;
    ls_arch_context_t context;
    ls_wide_context_snapshot_t wide_snapshot;
    ls_riscv64_context_t riscv64_context;
    const ls_riscv64_context_t *riscv64 = 0;
    ls_event_t event;
    ls_result_t result;

    if (!ls_minimal_snapshot_read(&snapshot)) {
        return LS_OK;
    }
    if (!ls_runtime.storage) {
        return LS_EAGAIN;
    }

    context = (ls_arch_context_t){
        .architecture =
            snapshot.architecture > LS_ARCH_UNKNOWN && snapshot.architecture <= LS_ARCH_RISCV64
                ? (ls_architecture_t)snapshot.architecture
                : ls_runtime.config.architecture,
        .fault = (ls_fault_kind_t)snapshot.fault,
        .lr = snapshot.lr,
        .pc = snapshot.pc,
        .xpsr = snapshot.xpsr,
        .msp = snapshot.msp,
        .psp = snapshot.psp,
        .exc_return = snapshot.exc_return,
        .cfsr = snapshot.cfsr,
        .hfsr = snapshot.hfsr,
        .fpscr = snapshot.fpscr,
        .has_fpu = (snapshot.flags & LS_MINIMAL_SNAPSHOT_FPU_FRAME) != 0u,
        .fpu_lazy = (snapshot.flags & LS_MINIMAL_SNAPSHOT_FPU_LAZY) != 0u,
        .ps = snapshot.ps,
        .sar = snapshot.sar,
        .exccause = snapshot.exccause,
        .excvaddr = snapshot.excvaddr,
        .fault_address = snapshot.excvaddr,
    };
    for (unsigned index = 0; index < 16u; ++index) {
        context.registers[index] = snapshot.registers[index];
    }
    if ((snapshot.flags & LS_MINIMAL_SNAPSHOT_WIDE_CONTEXT_VALID) != 0u &&
        ls_wide_context_snapshot_read(&wide_snapshot) &&
        wide_snapshot_matches_minimal(&wide_snapshot, &snapshot)) {
        riscv64_context = wide_snapshot.context;
        riscv64_context_to_arch(&riscv64_context, (ls_fault_kind_t)snapshot.fault, &context);
        riscv64 = &riscv64_context;
    }
    event = (ls_event_t){
        .type = LS_EVENT_CRASH,
        .priority = LS_PRIORITY_EMERGENCY,
        .timestamp_ms = ls_uptime_ms(),
        .fingerprint = ls_crash_fingerprint(&context),
        .domain = "fault",
        .code = (int32_t)snapshot.fault,
        .severity = LS_SEVERITY_FATAL,
        .message = "retained_fault",
        .cpu = &context,
        .riscv64 = riscv64,
        .capture_level = LS_CAPTURE_SNAPSHOT,
    };
    result = ls_capture_event(&event);
    if (result == LS_OK) {
        ls_minimal_snapshot_clear();
    }
    return result;
}

static ls_result_t capture_cpu_context(const ls_arch_context_t *context,
                                       const ls_riscv64_context_t *riscv64) {
    ls_breadcrumb_t breadcrumb;
    ls_event_t event;
    uint32_t fingerprint;
    ls_result_t result;

    if (!context) {
        return LS_EINVAL;
    }
    if (ls_runtime.capturing) {
        if (riscv64) {
            ls_capture_minimal_riscv64_fault(riscv64->x, riscv64->mstatus, riscv64->mcause,
                                             riscv64->mtval, riscv64->mepc);
            return LS_OK;
        }
        return ls_capture_minimal(context);
    }

    breadcrumb = (ls_breadcrumb_t){"cpu", LS_SEVERITY_FATAL, 1u, "cpu_fault", 0, 0u};
    ls_breadcrumb_event(&breadcrumb);

    fingerprint = ls_crash_fingerprint(context);
    event = (ls_event_t){
        .type = LS_EVENT_CRASH,
        .priority = LS_PRIORITY_EMERGENCY,
        .timestamp_ms = ls_uptime_ms(),
        .fingerprint = fingerprint,
        .domain = "cpu",
        .code = (int32_t)context->fault,
        .severity = LS_SEVERITY_FATAL,
        .message = "exception",
        .cpu = context,
        .riscv64 = riscv64,
        .capture_level = LS_ENABLE_STACK_SNAPSHOT ? LS_CAPTURE_STACK : LS_CAPTURE_SNAPSHOT,
    };

    ls_runtime.previous_crashed = true;
    ls_boot_state_mark_crash(fingerprint);
    result = ls_capture_event(&event);
    if (result != LS_OK) {
        (void)ls_capture_minimal(context);
    }
    if (ls_runtime.config.reset) {
        ls_runtime.config.reset(ls_runtime.config.reset_context);
    }
    return result;
}

ls_result_t ls_capture_cpu_context(const ls_arch_context_t *context) {
    return capture_cpu_context(context, 0);
}

ls_result_t ls_capture_riscv64_context(const ls_riscv64_context_t *context) {
    ls_arch_context_t legacy;

    if (!context) {
        return LS_EINVAL;
    }
    riscv64_context_to_arch(context, LS_FAULT_TRAP, &legacy);
    return capture_cpu_context(&legacy, context);
}
