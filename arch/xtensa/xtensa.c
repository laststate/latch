#include "xtensa.h"
#include "laststate/latch.h"
static bool frame_to_context(const ls_xtensa_frame_t *frame, ls_arch_context_t *context) {
    if (!frame)
        return false;
    *context = (ls_arch_context_t){0};
    context->architecture = LS_ARCH_XTENSA;
    context->fault = LS_FAULT_TRAP;
    for (unsigned i = 0; i < 16; i++)
        context->registers[i] = frame->a[i];
    context->lr = frame->a[0];
    context->pc = frame->pc;
    context->ps = frame->ps;
    context->sar = frame->sar;
    context->exccause = frame->exccause;
    context->excvaddr = frame->excvaddr;
    context->fault_address = frame->excvaddr;
    return true;
}

void ls_xtensa_capture_frame(const ls_xtensa_frame_t *frame) {
    ls_arch_context_t context;
    if (frame_to_context(frame, &context))
        (void)ls_capture_cpu_context(&context);
}

void ls_xtensa_capture_minimal_frame(const ls_xtensa_frame_t *frame) {
    ls_arch_context_t context;
    if (frame_to_context(frame, &context))
        ls_capture_minimal_context_fault(&context);
}
