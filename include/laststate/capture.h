#ifndef LASTSTATE_CAPTURE_H
#define LASTSTATE_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>
#include "event.h"

#define LS_MINIMAL_SNAPSHOT_VERSION 3u

enum {
    LS_MINIMAL_SNAPSHOT_FRAME_VALID = 1u << 0,
    LS_MINIMAL_SNAPSHOT_MSP_VALID = 1u << 1,
    LS_MINIMAL_SNAPSHOT_PSP_VALID = 1u << 2,
    LS_MINIMAL_SNAPSHOT_FPU_FRAME = 1u << 3,
    LS_MINIMAL_SNAPSHOT_FPU_LAZY = 1u << 4,
    LS_MINIMAL_SNAPSHOT_RECURSIVE = 1u << 5,
    LS_MINIMAL_SNAPSHOT_EMERGENCY_STACK_CORRUPT = 1u << 6,
    LS_MINIMAL_SNAPSHOT_STACK_BOUNDS_UNAVAILABLE = 1u << 7,
    LS_MINIMAL_SNAPSHOT_EXC_RETURN_INVALID = 1u << 8,
    /* A separately CRC-protected wide sidecar is committed before this
       retained v3 record.
       Older readers safely ignore this additive bit. */
    LS_MINIMAL_SNAPSHOT_WIDE_CONTEXT_VALID = 1u << 9
};

#define LS_WIDE_CONTEXT_SNAPSHOT_VERSION 1u

/*
 * RV64 state remains separate from ls_arch_context_t so 32-bit users do not
 * pay for a 64-bit
 * register bank on their normal capture stack. This type is
 * also the exact state persisted by
 * the optional retained sidecar.
 */
typedef struct {
    uint64_t x[32];
    uint64_t mstatus;
    uint64_t mcause;
    uint64_t mtval;
    uint64_t mepc;
} ls_riscv64_context_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t architecture;
    uint32_t fault;
    uint32_t build_hash;
    uint32_t fault_sequence;
    ls_riscv64_context_t context;
    uint32_t crc;
} ls_wide_context_snapshot_t;

/*
 * The first ten words deliberately retain the v1 layout. The extension is
 * protected by extension_crc, while crc continues to protect the legacy
 * prefix for readers that only understand v1.
 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t pc;
    uint32_t lr;
    uint32_t msp;
    uint32_t psp;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t build_hash;
    uint32_t crc;
    uint32_t fault;
    uint32_t flags;
    uint32_t exc_return;
    uint32_t xpsr;
    uint32_t fpscr;
    uint32_t emergency_stack_used;
    uint32_t fault_sequence;
    uint32_t extension_crc;
    /* v3 appends architecture context without moving the v1/v2 prefix. */
    uint32_t architecture;
    uint32_t registers[16];
    uint32_t ps;
    uint32_t sar;
    uint32_t exccause;
    uint32_t excvaddr;
    uint32_t context_crc;
} ls_minimal_snapshot_t;

ls_result_t ls_capture_minimal(const ls_arch_context_t *context);

/* Fault-safe full-context writer. Like ls_capture_minimal_fault(), it only
   touches retained
 * memory and performs bounded arithmetic/loops. */
void ls_capture_minimal_context_fault(const ls_arch_context_t *context);

/* Precompute normal-runtime metadata used by the fault-safe writer. */
void ls_capture_minimal_prepare(void);

/*
 * This writes only the retained minimal record. It does not enter the event,
 * spool, storage, transport, scheduler, or reset paths and is intended for
 * architecture fault handlers after they have switched to a known-good stack.
 */
void ls_capture_minimal_fault(uint32_t pc, uint32_t lr, uint32_t msp, uint32_t psp, uint32_t cfsr,
                              uint32_t hfsr, ls_fault_kind_t fault, uint32_t exc_return,
                              uint32_t xpsr, uint32_t fpscr, uint32_t flags,
                              uint32_t emergency_stack_used, uint32_t fault_sequence);

/* Fault-safe RV64 writer. It reads the assembly-saved frame with bounded
 * loops, writes only
 * retained memory, and never enters normal capture or
 * invokes reset callbacks. Full 64-bit
 * retained recovery requires
 * LS_ENABLE_WIDE_CONTEXT=1; otherwise the recovered LEP report
 * explicitly
 * marks the CPU64 extension incomplete. */
void ls_capture_minimal_riscv64_fault(const uint64_t registers[32], uint64_t mstatus,
                                      uint64_t mcause, uint64_t mtval, uint64_t mepc);

bool ls_minimal_snapshot_read(ls_minimal_snapshot_t *snapshot);
/* Reads the optional retained RV64 sidecar after validating its independent
 * magic, version, and
 * CRC. It returns false when the feature is disabled or
 * no complete sidecar is committed. */
bool ls_wide_context_snapshot_read(ls_wide_context_snapshot_t *snapshot);
void ls_minimal_snapshot_clear(void);

/* Normal-runtime only. Persists a retained fault snapshot after boot and
   clears it only after
 * the event reaches the spool. */
ls_result_t ls_capture_minimal_recover(void);

/* Normal-runtime RV64 capture. Unlike the fault-safe writer above, this may
 * enter the normal
 * event/spool path and follows ls_capture_cpu_context()
 * reset semantics. */
ls_result_t ls_capture_riscv64_context(const ls_riscv64_context_t *context);
#endif
