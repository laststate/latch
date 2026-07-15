#include "../core/internal.h"
#include "laststate/noinit.h"
#define LS_MINIMAL_MAGIC 0x4d534c53u
static LS_NOINIT volatile ls_minimal_snapshot_t minimal_snapshot;

ls_result_t ls_capture_minimal(const ls_arch_context_t *context){if(!context)return LS_EINVAL;ls_minimal_snapshot_t snapshot={LS_MINIMAL_MAGIC,1,context->pc,context->lr,context->msp,context->psp,context->cfsr,context->hfsr,ls_hash_string(ls_build_id()),0};snapshot.crc=ls_crc32(&snapshot,offsetof(ls_minimal_snapshot_t,crc));minimal_snapshot=snapshot;return LS_OK;}
bool ls_minimal_snapshot_read(ls_minimal_snapshot_t *snapshot){if(!snapshot)return false;ls_minimal_snapshot_t copy=minimal_snapshot;if(copy.magic!=LS_MINIMAL_MAGIC||copy.crc!=ls_crc32(&copy,offsetof(ls_minimal_snapshot_t,crc)))return false;*snapshot=copy;return true;}
void ls_minimal_snapshot_clear(void){minimal_snapshot.magic=0;}
ls_result_t ls_capture_cpu_context(const ls_arch_context_t *context){
    if(!context)return LS_EINVAL;
    if(ls_runtime.capturing)return ls_capture_minimal(context);
    ls_breadcrumb_t breadcrumb={"cpu",LS_SEVERITY_FATAL,1,"cpu_fault",0,0};ls_breadcrumb_event(&breadcrumb);
    uint32_t fingerprint=context->pc^context->lr^context->cfsr^context->mcause;
    ls_event_t event={.type=LS_EVENT_CRASH,.priority=LS_PRIORITY_CRITICAL,.timestamp_ms=ls_uptime_ms(),.fingerprint=fingerprint,.domain="cpu",.code=(int32_t)context->fault,.severity=LS_SEVERITY_FATAL,.message="exception",.cpu=context,.capture_level=LS_ENABLE_STACK_SNAPSHOT?LS_CAPTURE_STACK:LS_CAPTURE_SNAPSHOT};
    ls_runtime.previous_crashed=true;ls_boot_state_mark_crash(fingerprint);ls_result_t result=ls_capture_event(&event);if(result!=LS_OK)(void)ls_capture_minimal(context);if(ls_runtime.config.reset)ls_runtime.config.reset(ls_runtime.config.reset_context);return result;
}
