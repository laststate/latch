#ifndef LASTSTATE_FILE_BACKEND_H
#define LASTSTATE_FILE_BACKEND_H

#include <stdbool.h>
#include <stddef.h>

#include "laststate/storage.h"
#include "laststate/transport.h"

/*
 * This backend is for normal Linux runtime use. It is not async-signal-safe
 * and must not be called from a signal or fault handler.
 *
 * Initialize the storage before registering it with Latch when possible. The
 * read/write callbacks also initialize lazily so a missing or zero-length
 * backing file cannot make the first ls_boot() fail solely because the file
 * has not been provisioned yet.
 */
typedef struct {
    const char *path;
    size_t size;
    int fd;
    bool initialized;
} ls_file_storage_t;

#define LS_FILE_STORAGE_INITIALIZER(path_value, size_value)                                        \
    { (path_value), (size_value), -1, false }

/* Create/open and preallocate an erased (0xff) backing file. Idempotent. */
ls_result_t ls_file_storage_init(ls_file_storage_t *file);
/* Flushes and closes a storage descriptor. A later operation initializes it again. */
ls_result_t ls_file_storage_close(ls_file_storage_t *file);
ls_result_t ls_file_storage_read(void *context, size_t offset, void *dst, size_t length);
ls_result_t ls_file_storage_write(void *context, size_t offset, const void *src, size_t length);
ls_result_t ls_file_storage_erase(void *context, size_t offset, size_t length);
ls_result_t ls_file_storage_sync(void *context);

typedef struct {
    const char *directory;
} ls_file_transport_t;

ls_result_t ls_file_transport_send(void *context, const uint8_t *data, size_t length);
#endif
