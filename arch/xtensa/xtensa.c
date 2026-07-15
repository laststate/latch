#include "xtensa.h"
#include "laststate/latch.h"
void ls_xtensa_capture_frame(const ls_xtensa_frame_t *frame){if(!frame)return;ls_arch_context_t context={0};context.architecture=LS_ARCH_XTENSA;context.fault=LS_FAULT_TRAP;for(unsigned i=0;i<16;i++)context.registers[i]=frame->a[i];context.pc=frame->pc;context.ps=frame->ps;context.sar=frame->sar;context.exccause=frame->exccause;context.excvaddr=frame->excvaddr;context.fault_address=frame->excvaddr;(void)ls_capture_cpu_context(&context);}
