#ifndef LASTSTATE_STORAGE_H
#define LASTSTATE_STORAGE_H

#include <stddef.h>
#include <stdint.h>
#include "event.h"
typedef struct ls_storage_backend {
    const char *name;
    void *context;
    size_t capacity;
    size_t erase_size;
    size_t write_size;
    ls_result_t (*read)(void *context, size_t offset, void *dst, size_t length);
    ls_result_t (*write)(void *context, size_t offset, const void *src, size_t length);
    ls_result_t (*erase)(void *context, size_t offset, size_t length);
    ls_result_t (*sync)(void *context);
} ls_storage_backend_t;
typedef struct { uint8_t *data; size_t size; } ls_memory_storage_t;
ls_result_t ls_memory_storage_read(void *context, size_t offset, void *dst, size_t length);
ls_result_t ls_memory_storage_write(void *context, size_t offset, const void *src, size_t length);
ls_result_t ls_memory_storage_erase(void *context, size_t offset, size_t length);
void ls_storage_register(ls_storage_backend_t *storage);
size_t ls_storage_required_size(void);
#endif
