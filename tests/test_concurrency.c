#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"

#define CHECK(x) do { if(!(x)){fprintf(stderr,"concurrency failed: %s:%d\n",#x,__LINE__);return 1;} } while(0)
#define THREADS 4
#define RECORDS 1000
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static void enter(void *c){(void)c; int r=pthread_mutex_lock(&lock); (void)r;}
static void leave(void *c){(void)c; int r=pthread_mutex_unlock(&lock); (void)r;}
static void *producer(void *arg){
    intptr_t id=(intptr_t)arg;
    for(int i=0;i<RECORDS;i++){
        ls_result_t r=ls_blackbox_record_values(LS_BLACKBOX_USER,(uint16_t)id,0,(int32_t)id,i,0,0);
        if(r!=LS_OK) return (void*)(intptr_t)r;
    }
    return NULL;
}
int main(void){
    static const ls_identity_t id={.project_id="concurrency",.device_id="host",.firmware_build_id="concurrent-build-01"};
    ls_config_t config={.identity=&id,.enter_critical=enter,.leave_critical=leave};
    CHECK(ls_init(&config)==LS_OK); ls_blackbox_clear();
    pthread_t threads[THREADS];
    for(int i=0;i<THREADS;i++) CHECK(pthread_create(&threads[i],NULL,producer,(void*)(intptr_t)(i+1))==0);
    for(int i=0;i<THREADS;i++){void *result=NULL;CHECK(pthread_join(threads[i],&result)==0);CHECK(result==NULL);}
    ls_blackbox_stats_t stats; CHECK(ls_blackbox_get_stats(&stats)==LS_OK);
    CHECK(stats.total_records==THREADS*RECORDS); CHECK(stats.count==LS_BLACKBOX_CAPACITY);
    CHECK(stats.overwritten_records==THREADS*RECORDS-LS_BLACKBOX_CAPACITY);
    ls_blackbox_record_t records[LS_BLACKBOX_CAPACITY]; size_t count=0;
    CHECK(ls_blackbox_copy(records,LS_BLACKBOX_CAPACITY,&count)==LS_OK); CHECK(count==LS_BLACKBOX_CAPACITY);
    return 0;
}
