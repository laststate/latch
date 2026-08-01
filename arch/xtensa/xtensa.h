#ifndef LASTSTATE_XTENSA_H
#define LASTSTATE_XTENSA_H
#include <stdint.h>
typedef struct {
    uint32_t a[16];
    uint32_t pc, ps, sar, exccause, excvaddr;
    uint32_t lbeg, lend, lcount;
} ls_xtensa_frame_t;
void ls_xtensa_capture_frame(const ls_xtensa_frame_t *frame);
void ls_xtensa_capture_minimal_frame(const ls_xtensa_frame_t *frame);
#endif
