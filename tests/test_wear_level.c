#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"wear check failed: %s:%d\n",#x,__LINE__);return 1;}}while(0)
static uint8_t flash[16384],workspace[512],recovery[512];
static ls_storage_backend_t raw(ls_storage_sim_t *sim){ls_storage_backend_t backend={"nor",sim,sizeof flash,1024,1,ls_storage_sim_read,ls_storage_sim_write,ls_storage_sim_erase,ls_storage_sim_sync};return backend;}
int main(void){memset(flash,0xff,sizeof flash);ls_storage_sim_t sim={flash,sizeof flash,true,0,0,0};ls_storage_backend_t backend=raw(&sim);ls_flash_wear_level_t wear;CHECK(ls_flash_wear_init(&wear,&backend,workspace,sizeof workspace,8)==LS_OK);for(uint32_t i=0;i<80;i++)CHECK(wear.backend.write(wear.backend.context,0,&i,sizeof i)==LS_OK);ls_flash_wear_stats_t stats=ls_flash_wear_stats(&wear);CHECK(stats.slots==8);CHECK(stats.maximum_erases-stats.minimum_erases<=1u);CHECK(stats.generation==81u);uint32_t value=0;CHECK(wear.backend.read(wear.backend.context,0,&value,sizeof value)==LS_OK);CHECK(value==79u);ls_flash_wear_level_t reopened;CHECK(ls_flash_wear_init(&reopened,&backend,recovery,sizeof recovery,8)==LS_OK);CHECK(reopened.backend.read(reopened.backend.context,0,&value,sizeof value)==LS_OK);CHECK(value==79u);return 0;}
