#ifndef LASTSTATE_RISCV_H
#define LASTSTATE_RISCV_H
#include <stdint.h>
typedef struct {
    uint32_t x[32];
    uint32_t mstatus, mcause, mtval, mepc;
} ls_riscv_saved_t;
void ls_riscv_init(void);
void ls_riscv_trap_handler(void);
void ls_riscv_trap_from_saved(const ls_riscv_saved_t *saved);
#endif
