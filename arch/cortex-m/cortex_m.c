#include "cortex_m.h"

#include "laststate/config.h"
#include "laststate/noinit.h"

#define REG32(address) (*(volatile const uint32_t *)(uintptr_t)(address))
#define REG32W(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define LS_STACK_CANARY 0x51ac7e5au
#define LS_CORTEX_M_EXC_RETURN_PSP (1u << 2)
#define LS_CORTEX_M_EXC_RETURN_FPU (1u << 4)
#define LS_CORTEX_M_BASIC_FRAME_WORDS 8u
#define LS_CORTEX_M_FPU_FRAME_WORDS 18u
#define LS_CORTEX_M_FPCCR 0xe000ef34u

#ifndef LS_CORTEX_M_HAS_FPU
#if defined(__ARM_FP) && (__ARM_FP != 0)
#define LS_CORTEX_M_HAS_FPU 1
#else
#define LS_CORTEX_M_HAS_FPU 0
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#define LS_ALIGN32 __attribute__((aligned(32)))
#elif defined(_MSC_VER)
#define LS_ALIGN32 __declspec(align(32))
#else
#define LS_ALIGN32
#endif

typedef struct {
    uint8_t guard[32];
    uint8_t stack[LS_EMERGENCY_STACK_SIZE];
} ls_emergency_memory_t;

typedef struct {
    volatile uint32_t msp_lower;
    volatile uint32_t msp_upper;
    volatile uint32_t psp_lower;
    volatile uint32_t psp_upper;
    volatile uint32_t msp_ready;
    volatile uint32_t psp_ready;
} ls_cortex_m_stack_bounds_t;

_Static_assert(LS_EMERGENCY_STACK_SIZE >= 8u, "emergency stack must leave room for its canary");
_Static_assert((LS_EMERGENCY_STACK_SIZE % 8u) == 0u,
               "emergency stack must preserve AAPCS stack alignment");

LS_NOINIT LS_ALIGN32 static ls_emergency_memory_t emergency_memory;
static ls_cortex_m_stack_bounds_t fault_stack_bounds;
static volatile uint32_t emergency_stack_high_water;
static volatile uint32_t fault_capture_sequence;

uintptr_t ls_cortex_m_emergency_stack_top =
    (uintptr_t)(emergency_memory.stack + sizeof emergency_memory.stack);
volatile ls_cortex_m_saved_t ls_cortex_m_saved_context;
volatile uint32_t ls_cortex_m_fault_active;

static bool stack_bounds_pair_valid(const void *lower, const void *upper) {
    uintptr_t lower_address;
    uintptr_t upper_address;

    if (!lower && !upper) {
        return true;
    }
    if (!lower || !upper) {
        return false;
    }

    lower_address = (uintptr_t)lower;
    upper_address = (uintptr_t)upper;
    return lower_address < upper_address && ((lower_address | upper_address) & 3u) == 0u;
}

static bool stack_range_contains(uint32_t lower, uint32_t upper, uint32_t address, size_t length) {
    uint32_t available;

    if (lower >= upper || (address & 3u) != 0u || address < lower || address >= upper) {
        return false;
    }

    available = upper - address;
    return length <= (size_t)available;
}

static bool stack_pointer_in_bounds(uint32_t lower, uint32_t upper, uint32_t address) {
    return lower < upper && (address & 3u) == 0u && address >= lower && address <= upper;
}

static bool exc_return_valid(uint32_t exc_return) {
    return (exc_return & 0xff000000u) == 0xff000000u && (exc_return & 1u) != 0u;
}

static uint32_t fault_sequence_next(void) {
    uint32_t sequence = fault_capture_sequence + 1u;

    if (sequence == 0u) {
        sequence = 1u;
    }
    fault_capture_sequence = sequence;
    return sequence;
}

static bool fpu_lazy_state_active(void) {
#if LS_CORTEX_M_HAS_FPU
    return (REG32(LS_CORTEX_M_FPCCR) & 1u) != 0u;
#else
    return false;
#endif
}

void ls_cortex_m_init(void) {
    for (size_t index = 0; index < sizeof emergency_memory.stack; ++index) {
        emergency_memory.stack[index] = 0xa5u;
    }

    *(uint32_t *)(void *)emergency_memory.stack = LS_STACK_CANARY;
    emergency_stack_high_water = 0u;
    fault_capture_sequence = 0u;
    ls_cortex_m_fault_active = 0u;
    ls_capture_minimal_prepare();
}

bool ls_cortex_m_emergency_stack_ok(void) {
    return *(const uint32_t *)(const void *)emergency_memory.stack == LS_STACK_CANARY;
}

size_t ls_cortex_m_emergency_stack_usage(void) {
    size_t first_used = sizeof(uint32_t);
    size_t usage;

    while (first_used < sizeof emergency_memory.stack &&
           emergency_memory.stack[first_used] == 0xa5u) {
        ++first_used;
    }

    usage = sizeof emergency_memory.stack - first_used;
    if (!ls_cortex_m_emergency_stack_ok()) {
        usage = sizeof emergency_memory.stack;
    }
    if (usage > (size_t)emergency_stack_high_water) {
        emergency_stack_high_water = (uint32_t)usage;
    }
    return usage;
}

size_t ls_cortex_m_emergency_stack_high_water_mark(void) {
    return (size_t)emergency_stack_high_water;
}

ls_result_t ls_cortex_m_stack_bounds_set(const void *msp_lower, const void *msp_upper,
                                         const void *psp_lower, const void *psp_upper) {
    if (!stack_bounds_pair_valid(msp_lower, msp_upper) ||
        !stack_bounds_pair_valid(psp_lower, psp_upper)) {
        return LS_EINVAL;
    }

    fault_stack_bounds.msp_ready = 0u;
    fault_stack_bounds.psp_ready = 0u;
    fault_stack_bounds.msp_lower = (uint32_t)(uintptr_t)msp_lower;
    fault_stack_bounds.msp_upper = (uint32_t)(uintptr_t)msp_upper;
    fault_stack_bounds.psp_lower = (uint32_t)(uintptr_t)psp_lower;
    fault_stack_bounds.psp_upper = (uint32_t)(uintptr_t)psp_upper;
    fault_stack_bounds.msp_ready = msp_lower ? 1u : 0u;
    fault_stack_bounds.psp_ready = psp_lower ? 1u : 0u;
    return LS_OK;
}

void ls_cortex_m_stack_bounds_clear(void) {
    fault_stack_bounds.msp_ready = 0u;
    fault_stack_bounds.psp_ready = 0u;
}

ls_result_t ls_cortex_m_configure_emergency_stack_mpu(uint8_t region_number) {
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
    if (region_number >= 8u) {
        return LS_EINVAL;
    }
    REG32W(0xe000ed98u) = region_number;
    REG32W(0xe000ed9cu) = (uint32_t)(uintptr_t)emergency_memory.guard;
    REG32W(0xe000eda0u) = (1u << 28) | (4u << 1) | 1u;
    REG32W(0xe000ed94u) |= 5u;
    __asm volatile("dsb 0xf\n isb 0xf" ::: "memory");
    return LS_OK;
#elif defined(__ARM_ARCH_8M_MAIN__)
    if (region_number >= 16u) {
        return LS_EINVAL;
    }
    REG32W(0xe000ed98u) = region_number;
    REG32W(0xe000edc0u) = 0x44u;
    REG32W(0xe000ed9cu) = ((uint32_t)(uintptr_t)emergency_memory.guard & ~31u) | (2u << 1) | 1u;
    REG32W(0xe000eda0u) = (((uint32_t)(uintptr_t)emergency_memory.guard + 31u) & ~31u) | 1u;
    REG32W(0xe000ed94u) |= 5u;
    __asm volatile("dsb 0xf\n isb 0xf" ::: "memory");
    return LS_OK;
#else
    (void)region_number;
    return LS_ENOTSUP;
#endif
}

void ls_cortex_m_enable_configurable_faults(bool secure_fault) {
    uint32_t mask = (1u << 16) | (1u << 17) | (1u << 18);

#if defined(__ARM_ARCH_8M_MAIN__) || defined(__ARM_ARCH_8M_BASE__)
    if (secure_fault) {
        mask |= 1u << 19;
    }
#else
    (void)secure_fault;
#endif
    REG32W(0xe000ed24u) |= mask;
}

void ls_cortex_m_configure_fpu_lazy_stacking(bool enabled) {
#if LS_CORTEX_M_HAS_FPU
    if (enabled) {
        REG32W(LS_CORTEX_M_FPCCR) |= (1u << 31) | (1u << 30);
    } else {
        REG32W(LS_CORTEX_M_FPCCR) &= ~(1u << 30);
    }
#else
    (void)enabled;
#endif
}

ls_result_t ls_cortex_m_decode_exception_frame(const uint32_t *raw_frame,
                                               const volatile ls_cortex_m_saved_t *saved,
                                               ls_cortex_m_exception_frame_t *frame) {
    bool msp_ready;
    bool psp_ready;
    bool msp_valid;
    bool psp_valid;
    bool using_psp;
    bool extended;
    uint32_t selected_sp;
    uint32_t frame_address;
    uint32_t frame_lower;
    uint32_t frame_upper;
    size_t frame_words;
    const uint32_t *core_frame;

    if (!saved || !frame) {
        return LS_EINVAL;
    }

    *frame = (ls_cortex_m_exception_frame_t){0};
    msp_ready = fault_stack_bounds.msp_ready != 0u;
    psp_ready = fault_stack_bounds.psp_ready != 0u;
    msp_valid = msp_ready && stack_pointer_in_bounds(fault_stack_bounds.msp_lower,
                                                     fault_stack_bounds.msp_upper, saved->msp);
    psp_valid = psp_ready && stack_pointer_in_bounds(fault_stack_bounds.psp_lower,
                                                     fault_stack_bounds.psp_upper, saved->psp);

    if (msp_valid) {
        frame->flags |= LS_MINIMAL_SNAPSHOT_MSP_VALID;
    }
    if (psp_valid) {
        frame->flags |= LS_MINIMAL_SNAPSHOT_PSP_VALID;
    }
    if (!msp_ready || !psp_ready) {
        frame->flags |= LS_MINIMAL_SNAPSHOT_STACK_BOUNDS_UNAVAILABLE;
    }
    if (!exc_return_valid(saved->exc_return)) {
        frame->flags |= LS_MINIMAL_SNAPSHOT_EXC_RETURN_INVALID;
        return LS_ECORRUPT;
    }

    using_psp = (saved->exc_return & LS_CORTEX_M_EXC_RETURN_PSP) != 0u;
    extended = (saved->exc_return & LS_CORTEX_M_EXC_RETURN_FPU) == 0u;
    if (extended) {
        frame->flags |= LS_MINIMAL_SNAPSHOT_FPU_FRAME;
    }

    selected_sp = using_psp ? saved->psp : saved->msp;
    frame_lower = using_psp ? fault_stack_bounds.psp_lower : fault_stack_bounds.msp_lower;
    frame_upper = using_psp ? fault_stack_bounds.psp_upper : fault_stack_bounds.msp_upper;
    if ((using_psp && !psp_ready) || (!using_psp && !msp_ready) || !raw_frame) {
        return LS_ECORRUPT;
    }

    frame_words = LS_CORTEX_M_BASIC_FRAME_WORDS;
    if (extended) {
        frame_words += LS_CORTEX_M_FPU_FRAME_WORDS;
    }
    frame_address = (uint32_t)(uintptr_t)raw_frame;
    if (frame_address != selected_sp ||
        !stack_range_contains(frame_lower, frame_upper, frame_address,
                              frame_words * sizeof(uint32_t))) {
        return LS_ECORRUPT;
    }

    core_frame = raw_frame;
    if (extended) {
        core_frame += LS_CORTEX_M_FPU_FRAME_WORDS;
        frame->fpscr = raw_frame[16];
    }

    frame->r0 = core_frame[0];
    frame->r1 = core_frame[1];
    frame->r2 = core_frame[2];
    frame->r3 = core_frame[3];
    frame->r12 = core_frame[4];
    frame->lr = core_frame[5];
    frame->pc = core_frame[6];
    frame->xpsr = core_frame[7];
    frame->flags |= LS_MINIMAL_SNAPSHOT_FRAME_VALID;
    return LS_OK;
}

void ls_cortex_m_fault_from_saved(const uint32_t *raw_frame,
                                  const volatile ls_cortex_m_saved_t *saved) {
    ls_cortex_m_exception_frame_t frame = {0};
    ls_result_t frame_result = LS_EINVAL;
    ls_fault_kind_t fault = LS_FAULT_UNKNOWN;
    uint32_t msp = 0u;
    uint32_t psp = 0u;
    uint32_t exc_return = 0u;
    uint32_t flags = 0u;
    uint32_t stack_usage = 0u;
    uint32_t cfsr = REG32(0xe000ed28u);
    uint32_t hfsr = REG32(0xe000ed2cu);

    if (saved) {
        msp = saved->msp;
        psp = saved->psp;
        exc_return = saved->exc_return;
        fault = (ls_fault_kind_t)saved->fault_kind;
        frame_result = ls_cortex_m_decode_exception_frame(raw_frame, saved, &frame);
        flags = frame.flags;
    }

    if (frame_result == LS_OK && (flags & LS_MINIMAL_SNAPSHOT_FPU_FRAME) != 0u &&
        fpu_lazy_state_active()) {
        flags |= LS_MINIMAL_SNAPSHOT_FPU_LAZY;
        frame.fpscr = 0u;
    }

    if (ls_cortex_m_emergency_stack_ok()) {
        stack_usage = (uint32_t)ls_cortex_m_emergency_stack_usage();
    } else {
        flags |= LS_MINIMAL_SNAPSHOT_EMERGENCY_STACK_CORRUPT;
    }

    ls_capture_minimal_fault(frame.pc, frame.lr, msp, psp, cfsr, hfsr, fault, exc_return,
                             frame.xpsr, frame.fpscr, flags, stack_usage, fault_sequence_next());

    for (;;) {
    }
}

void ls_cortex_m_fault_recursive(uint32_t fault_kind, uint32_t exc_return, uint32_t msp,
                                 uint32_t psp) {
    uint32_t flags = LS_MINIMAL_SNAPSHOT_RECURSIVE;
    uint32_t stack_usage = 0u;

    if (ls_cortex_m_emergency_stack_ok()) {
        stack_usage = (uint32_t)ls_cortex_m_emergency_stack_usage();
    } else {
        flags |= LS_MINIMAL_SNAPSHOT_EMERGENCY_STACK_CORRUPT;
    }

    ls_capture_minimal_fault(0u, 0u, msp, psp, 0u, 0u, (ls_fault_kind_t)fault_kind, exc_return, 0u,
                             0u, flags, stack_usage, fault_sequence_next());

    for (;;) {
    }
}
