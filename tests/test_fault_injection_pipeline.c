#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"fault pipeline failed: %s:%d\n",#x,__LINE__); return 1; } } while (0)
static uint8_t bytes[50000];
static int online;
static unsigned sends;
static bool available(void *c){(void)c; return online != 0;}
static size_t mtu(void *c){(void)c; return LS_MAX_EVENT_SIZE;}
static ls_result_t send_data(void *c,const uint8_t*d,size_t n){(void)c; CHECK(d&&n); sends++; return LS_OK;}

static int setup(void) {
    static const ls_identity_t id={.project_id="fault-pipeline",.device_id="auv",.firmware_build_id="fault-pipeline-0001"};
    static ls_memory_storage_t mem;
    static ls_storage_backend_t storage;
    static ls_transport_backend_t transport;
    mem=(ls_memory_storage_t){bytes,sizeof bytes};
    storage=(ls_storage_backend_t){.name="ram",.context=&mem,.capacity=sizeof bytes,.read=ls_memory_storage_read,.write=ls_memory_storage_write,.erase=ls_memory_storage_erase};
    transport=(ls_transport_backend_t){.name="sink",.priority=1,.available=available,.send=send_data,.max_payload=mtu,.retry_limit=1};
    ls_config_t config={.identity=&id};
    CHECK(ls_init(&config)==LS_OK); ls_storage_register(&storage); ls_transport_register(&transport); CHECK(ls_boot()==LS_OK); return 0;
}

static int capture_with_commit_failure(void) {
    memset(bytes,0xff,sizeof bytes); online=0; sends=0; CHECK(setup()==0); ls_fault_injection_clear();
    CHECK(ls_fault_injection_arm("spool.before_commit",1u,LS_EIO)==LS_OK);
    ls_arch_context_t cpu={.architecture=LS_ARCH_CORTEX_M,.fault=LS_FAULT_HARD,.pc=1u,.lr=2u};
    CHECK(ls_capture_cpu_context(&cpu)==LS_EIO);
    ls_spool_stats_t stats; CHECK(ls_spool_get_stats(&stats)==LS_OK); CHECK(stats.committed==0u);
    ls_fault_injection_clear();
    /* Reboot must tolerate the torn slot and recover the retained minimal crash
     * into a fresh durable record after the injected fault is gone. */
    CHECK(setup()==0); CHECK(ls_spool_get_stats(&stats)==LS_OK); CHECK(stats.committed==1u);
    return 0;
}

static int send_and_ack_failures_are_retryable(void) {
    memset(bytes,0xff,sizeof bytes); online=0; sends=0; CHECK(setup()==0); ls_fault_injection_clear();
    ls_capture_message("durable",LS_SEVERITY_ERROR);
    ls_spool_stats_t stats; CHECK(ls_spool_get_stats(&stats)==LS_OK); CHECK(stats.committed==1u);
    online=1;
    CHECK(ls_fault_injection_arm("transport.send",1u,LS_EAGAIN)==LS_OK);
    /* Retryable transport faults are retried inside the transport backend. */
    CHECK(ls_flush()==LS_OK); CHECK(sends==1u); CHECK(ls_spool_get_stats(&stats)==LS_OK); CHECK(stats.committed==0u);
    ls_fault_injection_clear();

    ls_capture_message("ack-boundary",LS_SEVERITY_ERROR);
    CHECK(ls_spool_get_stats(&stats)==LS_OK); CHECK(stats.committed==1u);
    CHECK(ls_fault_injection_arm("spool.before_ack",1u,LS_EIO)==LS_OK);
    CHECK(ls_flush()==LS_EIO); CHECK(sends==2u); CHECK(ls_spool_get_stats(&stats)==LS_OK); CHECK(stats.committed==1u);
    ls_fault_injection_clear();
    /* At-least-once semantics: if durable ACK marking fails, the same event is
     * resent and the receiver deduplicates by device/event ID. */
    CHECK(ls_flush()==LS_OK); CHECK(sends==3u); CHECK(ls_spool_get_stats(&stats)==LS_OK); CHECK(stats.committed==0u);
    return 0;
}

static int storage_hook_is_real(void) {
    uint8_t local[32]; memset(local,0xff,sizeof local);
    ls_memory_storage_t mem={local,sizeof local};
    ls_storage_backend_t storage={.context=&mem,.capacity=sizeof local,.read=ls_memory_storage_read,.write=ls_memory_storage_write,.erase=ls_memory_storage_erase};
    uint8_t value=0x7fu; ls_fault_injection_clear();
    CHECK(ls_fault_injection_arm("storage.program.before",1u,LS_EIO)==LS_OK);
    CHECK(ls_storage_program(&storage,0u,&value,1u)==LS_EIO); CHECK(local[0]==0xffu);
    ls_fault_injection_clear(); CHECK(ls_storage_program(&storage,0u,&value,1u)==LS_OK); CHECK(local[0]==0x7fu);
    return 0;
}

int main(void){CHECK(capture_with_commit_failure()==0);CHECK(send_and_ack_failures_are_retryable()==0);CHECK(storage_hook_is_real()==0);return 0;}
