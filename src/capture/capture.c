#include "../core/internal.h"
#include "laststate/noinit.h"

#define LS_MINIMAL_MAGIC 0x4d534c53u

static LS_NOINIT volatile ls_minimal_snapshot_t minimal_snapshot;
static uint32_t minimal_build_hash;

static uint32_t minimal_crc32_update(uint32_t crc, const volatile uint8_t *data, size_t length) {
    while (length-- != 0u) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8u; ++bit) {
            crc = (crc >> 1u) ^ (0xedb88320u & (uint32_t) - (int32_t)(crc & 1u));
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

static void minimal_snapshot_store(uint32_t pc, uint32_t lr, uint32_t msp, uint32_t psp,
                                   uint32_t cfsr, uint32_t hfsr, ls_fault_kind_t fault,
                                   uint32_t exc_return, uint32_t xpsr, uint32_t fpscr,
                                   uint32_t flags, uint32_t emergency_stack_used,
                                   uint32_t fault_sequence, const ls_arch_context_t *context) {
    volatile ls_minimal_snapshot_t *snapshot = &minimal_snapshot;

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
}

void ls_capture_minimal_fault(uint32_t pc, uint32_t lr, uint32_t msp, uint32_t psp, uint32_t cfsr,
                              uint32_t hfsr, ls_fault_kind_t fault, uint32_t exc_return,
                              uint32_t xpsr, uint32_t fpscr, uint32_t flags,
                              uint32_t emergency_stack_used, uint32_t fault_sequence) {
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
    minimal_snapshot_store(context->pc, context->lr, context->msp, context->psp, context->cfsr,
                           context->hfsr, context->fault, context->exc_return, context->xpsr,
                           context->fpscr, flags, 0u, 0u, context);
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
        uint32_t extension_crc =
            ls_crc32(snapshot, offsetof(ls_minimal_snapshot_t, extension_crc));

        if (snapshot->crc != prefix_crc || snapshot->extension_crc != extension_crc) {
            return false;
        }
        ls_memset((uint8_t *)snapshot + offsetof(ls_minimal_snapshot_t, architecture), 0,
                  sizeof *snapshot - offsetof(ls_minimal_snapshot_t, architecture));
    } else if (snapshot->version == LS_MINIMAL_SNAPSHOT_VERSION) {
        uint32_t extension_crc =
            ls_crc32(snapshot, offsetof(ls_minimal_snapshot_t, extension_crc));
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
}

ls_result_t ls_capture_minimal_recover(void) {
    ls_minimal_snapshot_t snapshot;
    ls_arch_context_t context;
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
            snapshot.architecture > LS_ARCH_UNKNOWN && snapshot.architecture <= LS_ARCH_LINUX
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
    event = (ls_event_t){
        .type = LS_EVENT_CRASH,
        .priority = LS_PRIORITY_CRITICAL,
        .timestamp_ms = ls_uptime_ms(),
        .fingerprint = snapshot.pc ^ snapshot.lr ^ snapshot.cfsr ^ snapshot.hfsr,
        .domain = "fault",
        .code = (int32_t)snapshot.fault,
        .severity = LS_SEVERITY_FATAL,
        .message = "retained_fault",
        .cpu = &context,
        .capture_level = LS_CAPTURE_SNAPSHOT,
    };
    result = ls_capture_event(&event);
    if (result == LS_OK) {
        ls_minimal_snapshot_clear();
    }
    return result;
}

ls_result_t ls_capture_cpu_context(const ls_arch_context_t *context) {
    ls_breadcrumb_t breadcrumb;
    ls_event_t event;
    uint32_t fingerprint;
    ls_result_t result;

    if (!context) {
        return LS_EINVAL;
    }
    if (ls_runtime.capturing) {
        return ls_capture_minimal(context);
    }

    breadcrumb = (ls_breadcrumb_t){"cpu", LS_SEVERITY_FATAL, 1u, "cpu_fault", 0, 0u};
    ls_breadcrumb_event(&breadcrumb);

    fingerprint = context->pc ^ context->lr ^ context->cfsr ^ context->mcause;
    event = (ls_event_t){
        .type = LS_EVENT_CRASH,
        .priority = LS_PRIORITY_CRITICAL,
        .timestamp_ms = ls_uptime_ms(),
        .fingerprint = fingerprint,
        .domain = "cpu",
        .code = (int32_t)context->fault,
        .severity = LS_SEVERITY_FATAL,
        .message = "exception",
        .cpu = context,
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
