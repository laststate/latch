#ifndef LASTSTATE_FILE_BACKEND_H
#define LASTSTATE_FILE_BACKEND_H
#include <stddef.h>
#include "laststate/storage.h"
#include "laststate/transport.h"
typedef struct {
    const char *path;
    size_t size;
} ls_file_storage_t;
ls_result_t ls_file_storage_read(void *context, size_t offset, void *dst, size_t length);
ls_result_t ls_file_storage_write(void *context, size_t offset, const void *src, size_t length);
ls_result_t ls_file_storage_erase(void *context, size_t offset, size_t length);
ls_result_t ls_file_storage_sync(void *context);
typedef struct {
    const char *directory;
} ls_file_transport_t;
ls_result_t ls_file_transport_send(void *context, const uint8_t *data, size_t length);
#endif
