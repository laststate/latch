#ifndef LASTSTATE_RISCV64_H
#define LASTSTATE_RISCV64_H

#include "laststate/capture.h"

/* The assembly frame and the retained CPU64 encoding deliberately share this
 * exact, fixed-width layout. */
typedef ls_riscv64_context_t ls_riscv64_saved_t;

void ls_riscv64_init(void);
void ls_riscv64_trap_handler(void);
void ls_riscv64_capture_minimal_from_saved(const ls_riscv64_saved_t *saved);
void ls_riscv64_trap_from_saved(const ls_riscv64_saved_t *saved);

#endif
