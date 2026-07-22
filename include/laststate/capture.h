#ifndef LASTSTATE_CAPTURE_H
#define LASTSTATE_CAPTURE_H
#include <stdbool.h>
#include <stdint.h>
#include "event.h"
#define LS_MINIMAL_SNAPSHOT_VERSION 2u
enum{LS_MINIMAL_SNAPSHOT_FRAME_VALID=1u<<0,LS_MINIMAL_SNAPSHOT_MSP_VALID=1u<<1,LS_MINIMAL_SNAPSHOT_PSP_VALID=1u<<2,LS_MINIMAL_SNAPSHOT_FPU_FRAME=1u<<3,LS_MINIMAL_SNAPSHOT_FPU_LAZY=1u<<4,LS_MINIMAL_SNAPSHOT_RECURSIVE=1u<<5,LS_MINIMAL_SNAPSHOT_EMERGENCY_STACK_CORRUPT=1u<<6,LS_MINIMAL_SNAPSHOT_STACK_BOUNDS_UNAVAILABLE=1u<<7,LS_MINIMAL_SNAPSHOT_EXC_RETURN_INVALID=1u<<8};typedef struct{uint32_t magic;uint32_t version;uint32_t pc;uint32_t lr;uint32_t msp;uint32_t psp;uint32_t cfsr;uint32_t hfsr;uint32_t build_hash;uint32_t crc;uint32_t fault;uint32_t flags;uint32_t exc_return;uint32_t xpsr;uint32_t fpscr;uint32_t emergency_stack_used;uint32_t fault_sequence;uint32_t extension_crc;}ls_minimal_snapshot_t;ls_result_t ls_capture_minimal(const ls_arch_context_t*context);void ls_capture_minimal_prepare(void);void ls_capture_minimal_fault(uint32_t pc,uint32_t lr,uint32_t msp,uint32_t psp,uint32_t cfsr,uint32_t hfsr,ls_fault_kind_t fault,uint32_t exc_return,uint32_t xpsr,uint32_t fpscr,uint32_t flags,uint32_t emergency_stack_used,uint32_t fault_sequence);bool ls_minimal_snapshot_read(ls_minimal_snapshot_t*snapshot);void ls_minimal_snapshot_clear(void);ls_result_t ls_capture_minimal_recover(void);
#endif
