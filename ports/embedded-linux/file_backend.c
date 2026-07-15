#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "file_backend.h"
#include <stdio.h>
#include <string.h>
#include "laststate/envelope.h"
static ls_result_t access_file(ls_file_storage_t *file, size_t offset, void *buffer, size_t length,
                               bool write) {
    if (!file || !file->path || offset > file->size || length > file->size - offset)
        return LS_EINVAL;
    FILE *stream = fopen(file->path, write ? "r+b" : "rb");
    if (!stream && write)
        stream = fopen(file->path, "w+b");
    if (!stream)
        return LS_EIO;
    if (fseek(stream, (long)offset, SEEK_SET) != 0) {
        fclose(stream);
        return LS_EIO;
    }
    size_t done;
    if (write) {
        done = fwrite(buffer, 1, length, stream);
        if (fflush(stream) != 0) {
            fclose(stream);
            return LS_EIO;
        }
    } else {
        done = fread(buffer, 1, length, stream);
    }
    if (fclose(stream) != 0)
        return LS_EIO;
    return done == length ? LS_OK : LS_EIO;
}
ls_result_t ls_file_storage_read(void *context, size_t offset, void *dst, size_t length) {
    return access_file((ls_file_storage_t *)context, offset, dst, length, false);
}
ls_result_t ls_file_storage_write(void *context, size_t offset, const void *src, size_t length) {
    return access_file((ls_file_storage_t *)context, offset, (void *)src, length, true);
}
ls_result_t ls_file_storage_erase(void *context, size_t offset, size_t length) {
    ls_file_storage_t *file = (ls_file_storage_t *)context;
    uint8_t erased[256];
    memset(erased, 0xff, sizeof erased);
    while (length) {
        size_t chunk = length > sizeof erased ? sizeof erased : length;
        ls_result_t result = access_file(file, offset, erased, chunk, true);
        if (result != LS_OK)
            return result;
        offset += chunk;
        length -= chunk;
    }
    return LS_OK;
}
ls_result_t ls_file_storage_sync(void *context) {
    (void)context;
    return LS_OK;
}
ls_result_t ls_file_transport_send(void *context, const uint8_t *data, size_t length) {
    ls_file_transport_t *transport = (ls_file_transport_t *)context;
    ls_envelope_info_t info;
    if (!transport || !transport->directory || ls_envelope_validate(data, length, &info) != LS_OK)
        return LS_EINVAL;
    char path[512];
    int written =
        snprintf(path, sizeof path, "%s/%08x.lst", transport->directory, (unsigned)info.event_id);
    if (written <= 0 || (size_t)written >= sizeof path)
        return LS_ENOSPACE;
    FILE *file = fopen(path, "wb");
    if (!file)
        return LS_EIO;
    size_t done = fwrite(data, 1, length, file);
    int close_result = fclose(file);
    return done == length && close_result == 0 ? LS_OK : LS_EIO;
}
