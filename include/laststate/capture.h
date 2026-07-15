#ifndef LASTSTATE_CAPTURE_H
#define LASTSTATE_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>
#include "event.h"
typedef struct {uint32_t magic,version,pc,lr,msp,psp,cfsr,hfsr,build_hash,crc;} ls_minimal_snapshot_t;
ls_result_t ls_capture_minimal(const ls_arch_context_t *context);
bool ls_minimal_snapshot_read(ls_minimal_snapshot_t *snapshot);
void ls_minimal_snapshot_clear(void);
#endif
