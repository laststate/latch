#include <stdint.h>
#include <stdio.h>

#include "cortex_m.h"
#include "laststate/capture.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "check failed: %s at line %d\n", #condition, __LINE__);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static void fill_basic_frame(uint32_t *frame) {
    frame[0] = 0x00000010u;
    frame[1] = 0x00000011u;
    frame[2] = 0x00000012u;
    frame[3] = 0x00000013u;
    frame[4] = 0x0000001cu;
    frame[5] = 0x08001235u;
    frame[6] = 0x08004567u;
    frame[7] = 0x21000000u;
}

static int test_basic_and_psp_frames(void) {
    uint32_t stack[64] = {0};
    ls_cortex_m_saved_t saved = {0};
    ls_cortex_m_exception_frame_t frame;

    CHECK(ls_cortex_m_stack_bounds_set(stack, stack + 64, stack, stack + 64) == LS_OK);
    fill_basic_frame(&stack[8]);
    saved.msp = (uint32_t)(uintptr_t)&stack[8];
    saved.psp = (uint32_t)(uintptr_t)&stack[20];
    saved.exc_return = 0xfffffff9u;

    CHECK(ls_cortex_m_decode_exception_frame(&stack[8], &saved, &frame) == LS_OK);
    CHECK((frame.flags & LS_MINIMAL_SNAPSHOT_FRAME_VALID) != 0u);
    CHECK((frame.flags & LS_MINIMAL_SNAPSHOT_MSP_VALID) != 0u);
    CHECK((frame.flags & LS_MINIMAL_SNAPSHOT_PSP_VALID) != 0u);
    CHECK(frame.r0 == 0x00000010u);
    CHECK(frame.r12 == 0x0000001cu);
    CHECK(frame.lr == 0x08001235u);
    CHECK(frame.pc == 0x08004567u);
    CHECK(frame.xpsr == 0x21000000u);

    fill_basic_frame(&stack[20]);
    saved.exc_return = 0xfffffffdu;
    CHECK(ls_cortex_m_decode_exception_frame(&stack[20], &saved, &frame) == LS_OK);
    CHECK(frame.pc == 0x08004567u);
    return 0;
}

static int test_extended_fpu_frame(void) {
    uint32_t stack[64] = {0};
    ls_cortex_m_saved_t saved = {0};
    ls_cortex_m_exception_frame_t frame;

    CHECK(ls_cortex_m_stack_bounds_set(stack, stack + 64, 0, 0) == LS_OK);
    for (unsigned index = 0; index < 16u; ++index) {
        stack[8 + index] = 0x3f000000u + index;
    }
    stack[24] = 0xa5a5a5a5u;
    fill_basic_frame(&stack[26]);
    saved.msp = (uint32_t)(uintptr_t)&stack[8];
    saved.psp = 0u;
    saved.exc_return = 0xffffffe9u;

    CHECK(ls_cortex_m_decode_exception_frame(&stack[8], &saved, &frame) == LS_OK);
    CHECK((frame.flags & LS_MINIMAL_SNAPSHOT_FRAME_VALID) != 0u);
    CHECK((frame.flags & LS_MINIMAL_SNAPSHOT_FPU_FRAME) != 0u);
    CHECK((frame.flags & LS_MINIMAL_SNAPSHOT_STACK_BOUNDS_UNAVAILABLE) != 0u);
    CHECK(frame.fpscr == 0xa5a5a5a5u);
    CHECK(frame.pc == 0x08004567u);
    return 0;
}

static int test_invalid_frames_are_not_decoded(void) {
    uint32_t stack[64] = {0};
    ls_cortex_m_saved_t saved = {0};
    ls_cortex_m_exception_frame_t frame;

    CHECK(ls_cortex_m_stack_bounds_set(stack, stack + 64, 0, 0) == LS_OK);
    saved.msp = (uint32_t)(uintptr_t)&stack[60];
    saved.exc_return = 0xfffffff9u;
    CHECK(ls_cortex_m_decode_exception_frame(&stack[60], &saved, &frame) == LS_ECORRUPT);
    CHECK((frame.flags & LS_MINIMAL_SNAPSHOT_FRAME_VALID) == 0u);

    saved.msp = (uint32_t)(uintptr_t)&stack[8];
    CHECK(ls_cortex_m_decode_exception_frame(&stack[9], &saved, &frame) == LS_ECORRUPT);
    CHECK((frame.flags & LS_MINIMAL_SNAPSHOT_FRAME_VALID) == 0u);

    saved.exc_return = 0u;
    CHECK(ls_cortex_m_decode_exception_frame(&stack[8], &saved, &frame) == LS_ECORRUPT);
    CHECK((frame.flags & LS_MINIMAL_SNAPSHOT_EXC_RETURN_INVALID) != 0u);

    ls_cortex_m_stack_bounds_clear();
    saved.exc_return = 0xfffffff9u;
    CHECK(ls_cortex_m_decode_exception_frame(&stack[8], &saved, &frame) == LS_ECORRUPT);
    CHECK((frame.flags & LS_MINIMAL_SNAPSHOT_STACK_BOUNDS_UNAVAILABLE) != 0u);
    return 0;
}

static int test_minimal_snapshot(void) {
    ls_minimal_snapshot_t snapshot;

    ls_minimal_snapshot_clear();
    CHECK(!ls_minimal_snapshot_read(&snapshot));
    ls_capture_minimal_prepare();
    ls_capture_minimal_fault(0x08004567u, 0x08001235u, 0x20001000u, 0x20002000u, 0x00010002u,
                             0x40000000u, LS_FAULT_HARD, 0xffffffe9u, 0x21000000u, 0x01000000u,
                             LS_MINIMAL_SNAPSHOT_FRAME_VALID | LS_MINIMAL_SNAPSHOT_FPU_FRAME, 96u,
                             3u);

    CHECK(ls_minimal_snapshot_read(&snapshot));
    CHECK(snapshot.version == LS_MINIMAL_SNAPSHOT_VERSION);
    CHECK(snapshot.pc == 0x08004567u);
    CHECK(snapshot.lr == 0x08001235u);
    CHECK(snapshot.fault == LS_FAULT_HARD);
    CHECK((snapshot.flags & LS_MINIMAL_SNAPSHOT_FPU_FRAME) != 0u);
    CHECK(snapshot.exc_return == 0xffffffe9u);
    CHECK(snapshot.fpscr == 0x01000000u);
    CHECK(snapshot.emergency_stack_used == 96u);
    CHECK(snapshot.fault_sequence == 3u);

    ls_minimal_snapshot_clear();
    CHECK(!ls_minimal_snapshot_read(&snapshot));
    return 0;
}

int main(void) {
    ls_cortex_m_init();
    CHECK(ls_cortex_m_emergency_stack_ok());
    CHECK(ls_cortex_m_emergency_stack_usage() == 0u);
    CHECK(ls_cortex_m_emergency_stack_high_water_mark() == 0u);
    CHECK(test_basic_and_psp_frames() == 0);
    CHECK(test_extended_fpu_frame() == 0);
    CHECK(test_invalid_frames_are_not_decoded() == 0);
    CHECK(test_minimal_snapshot() == 0);
    puts("cortex-m fault tests passed");
    return 0;
}
