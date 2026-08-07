#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#include "../src/core/internal.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"spool edges failed: %s:%d\n",#x,__LINE__); return 1; } } while(0)

enum { HEADER_SIZE=24u, RECORD_SIZE=28u };
static uint8_t bytes[50000];
typedef struct {
    uint32_t magic; uint16_t version; uint16_t length; uint32_t sequence; uint32_t data_crc;
    uint8_t priority; uint8_t event_type; uint16_t reserved; uint32_t header_crc;
    uint8_t state; uint8_t retry_bits; uint16_t reserved_after_state;
} test_spool_record_t;
static uint32_t test_record_crc(const test_spool_record_t *r) { return ls_crc32(r, offsetof(test_spool_record_t, header_crc)); }


static int setup(ls_storage_backend_t *storage, ls_memory_storage_t *memory) {
    (void)memory;
    static const ls_identity_t id={.project_id="spool-edge",.device_id="host",.firmware_build_id="spedge01"};
    ls_config_t cfg={.identity=&id};
    CHECK(ls_init(&cfg)==LS_OK);
    if (storage) ls_storage_register(storage);
    return 0;
}
static int make_storage(ls_storage_backend_t *storage, ls_memory_storage_t *memory) {
    *memory=(ls_memory_storage_t){bytes,sizeof bytes};
    *storage=(ls_storage_backend_t){.name="ram",.context=memory,.capacity=sizeof bytes,
        .read=ls_memory_storage_read,.write=ls_memory_storage_write,.erase=ls_memory_storage_erase};
    return 0;
}
static int make_envelope(uint8_t *out,size_t *length) {
    ls_event_t e={.type=LS_EVENT_MESSAGE,.priority=LS_PRIORITY_WARNING,.timestamp_ms=1u,.domain="spool",
                  .severity=LS_SEVERITY_WARNING,.message="edge",.capture_level=LS_CAPTURE_METADATA};
    return ls_envelope_encode(&e,out,LS_MAX_EVENT_SIZE,length)==LS_OK?0:1;
}

int main(void) {
    ls_storage_backend_t storage; ls_memory_storage_t memory;
    CHECK(setup(NULL,NULL)==0);
    CHECK(ls_spool_init()==LS_OK);
    CHECK(ls_spool_flush()==LS_OK);
    ls_spool_stats_t stats;
    CHECK(ls_spool_get_stats(&stats)==LS_OK && stats.committed==0u);
    uint8_t env[LS_MAX_EVENT_SIZE]; size_t env_len=0u; CHECK(make_envelope(env,&env_len)==0);
    CHECK(ls_spool_append(NULL,env_len,LS_PRIORITY_ERROR)==LS_EINVAL);
    CHECK(ls_spool_append(env,0u,LS_PRIORITY_ERROR)==LS_EINVAL);
    CHECK(ls_spool_append(env,LS_MAX_EVENT_SIZE+1u,LS_PRIORITY_ERROR)==LS_EINVAL);
    CHECK(ls_spool_append(env,env_len,(ls_priority_t)(LS_PRIORITY_DIAGNOSTIC+1))==LS_EINVAL);
    CHECK(ls_spool_append(env,env_len,LS_PRIORITY_ERROR)==LS_OK); /* no storage: capture remains non-blocking */

    memset(bytes,0xff,sizeof bytes); make_storage(&storage,&memory); storage.capacity=1u;
    CHECK(setup(&storage,&memory)==0);
    CHECK(ls_spool_init()==LS_ENOSPACE);
    CHECK(ls_spool_append(env,env_len,LS_PRIORITY_ERROR)==LS_ENOSPACE);
    CHECK(ls_spool_flush()==LS_ENOSPACE);
    CHECK(ls_spool_get_stats(&stats)==LS_ENOSPACE);

    memset(bytes,0xff,sizeof bytes); make_storage(&storage,&memory); storage.read=NULL;
    CHECK(setup(&storage,&memory)==0); CHECK(ls_spool_init()==LS_ENOSPACE);
    make_storage(&storage,&memory); storage.write=NULL;
    CHECK(setup(&storage,&memory)==0); CHECK(ls_spool_init()==LS_ENOSPACE);

    /* A non-erased invalid spool with no erase callback cannot be recovered silently. */
    memset(bytes,0,sizeof bytes); make_storage(&storage,&memory); storage.erase=NULL;
    CHECK(setup(&storage,&memory)==0); CHECK(ls_spool_init()==LS_ECORRUPT);

    /* Erase geometry that cannot cover the complete spool is explicitly unsupported. */
    memset(bytes,0,sizeof bytes); make_storage(&storage,&memory); storage.erase_size=7u;
    CHECK(setup(&storage,&memory)==0); CHECK(ls_spool_init()==LS_ENOTSUP);

    /* Establish a valid baseline, then independently invalidate header discriminators. */
    memset(bytes,0xff,sizeof bytes); make_storage(&storage,&memory); CHECK(setup(&storage,&memory)==0); CHECK(ls_spool_init()==LS_OK);
    uint8_t baseline[50000]; memcpy(baseline,bytes,sizeof bytes);
    const size_t header_mutations[] = {0u,4u,6u,8u,18u,16u};
    for (size_t i=0;i<sizeof header_mutations/sizeof header_mutations[0];++i) {
        memcpy(bytes,baseline,sizeof bytes); bytes[header_mutations[i]] ^= 1u;
        make_storage(&storage,&memory); CHECK(setup(&storage,&memory)==0);
        CHECK(ls_spool_init()==LS_OK); /* reformats invalid metadata when storage is recoverable */
    }

    /* Record validity fields are all fail-closed: malformed metadata is never transmitted. */
    memset(bytes,0xff,sizeof bytes); make_storage(&storage,&memory); CHECK(setup(&storage,&memory)==0); CHECK(ls_spool_init()==LS_OK);
    CHECK(make_envelope(env,&env_len)==0); CHECK(ls_spool_append(env,env_len,LS_PRIORITY_ERROR)==LS_OK);
    memcpy(baseline,bytes,sizeof bytes);
    const size_t rec = HEADER_SIZE;
    const size_t rec_mutations[] = {0u,4u,6u,16u,17u,20u};
    for (size_t i=0;i<sizeof rec_mutations/sizeof rec_mutations[0];++i) {
        memcpy(bytes,baseline,sizeof bytes); bytes[rec+rec_mutations[i]] ^= 0x80u;
        make_storage(&storage,&memory); CHECK(setup(&storage,&memory)==0); CHECK(ls_spool_init()==LS_OK);
        CHECK(ls_spool_flush()==LS_OK);
    }

    /* Semantic corruption with a recomputed record-header CRC: these checks kill
     * mutants that merely bypass the length/data CRC predicates. */
    memcpy(bytes,baseline,sizeof bytes);
    test_spool_record_t *record=(test_spool_record_t *)(void *)(bytes+rec);
    record->length=0u; record->header_crc=test_record_crc(record);
    make_storage(&storage,&memory); CHECK(setup(&storage,&memory)==0); CHECK(ls_spool_init()==LS_OK);
    CHECK(ls_spool_flush()==LS_OK);
    CHECK(ls_spool_get_stats(&stats)==LS_OK && stats.committed==0u && stats.corrupt_records==0u);

    memcpy(bytes,baseline,sizeof bytes); record=(test_spool_record_t *)(void *)(bytes+rec);
    record->data_crc^=1u; record->header_crc=test_record_crc(record);
    make_storage(&storage,&memory); CHECK(setup(&storage,&memory)==0); CHECK(ls_spool_init()==LS_OK);
    CHECK(ls_spool_flush()==LS_OK);
    CHECK(ls_spool_get_stats(&stats)==LS_OK && stats.corrupt_records>=1u);

    /* Invalid LEP input is rejected before it can consume flash. */
    memcpy(bytes,baseline,sizeof bytes); make_storage(&storage,&memory); CHECK(setup(&storage,&memory)==0);
    env[0]^=1u; CHECK(ls_spool_append(env,env_len,LS_PRIORITY_ERROR)==LS_ECORRUPT);
    return 0;
}
