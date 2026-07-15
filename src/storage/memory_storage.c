#include "../core/internal.h"
static ls_result_t bounds(ls_memory_storage_t *memory,size_t offset,size_t length){return (!memory||!memory->data||offset>memory->size||length>memory->size-offset)?LS_EINVAL:LS_OK;}
ls_result_t ls_memory_storage_read(void *context,size_t offset,void *dst,size_t length){ls_memory_storage_t *memory=(ls_memory_storage_t *)context;if(!dst&&length)return LS_EINVAL;ls_result_t result=bounds(memory,offset,length);if(result==LS_OK&&length)ls_memcpy(dst,memory->data+offset,length);return result;}
ls_result_t ls_memory_storage_write(void *context,size_t offset,const void *src,size_t length){ls_memory_storage_t *memory=(ls_memory_storage_t *)context;if(!src&&length)return LS_EINVAL;ls_result_t result=bounds(memory,offset,length);if(result==LS_OK&&length)ls_memcpy(memory->data+offset,src,length);return result;}
ls_result_t ls_memory_storage_erase(void *context,size_t offset,size_t length){ls_memory_storage_t *memory=(ls_memory_storage_t *)context;ls_result_t result=bounds(memory,offset,length);if(result==LS_OK)ls_memset(memory->data+offset,0xff,length);return result;}
void ls_storage_register(ls_storage_backend_t *storage){ls_runtime.storage=storage;}
