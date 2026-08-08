#define _POSIX_C_SOURCE 200809L

#include "file_backend.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "laststate/envelope.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX ((ssize_t)(SIZE_MAX >> 1u))
#endif

enum { LS_FILE_ERASE_CHUNK_SIZE = 4096 };

static ls_result_t storage_bounds(const ls_file_storage_t *file, size_t offset, size_t length) {
    if (!file || !file->path || !file->size || offset > file->size ||
        length > file->size - offset) {
        return LS_EINVAL;
    }

    return LS_OK;
}

static bool size_to_offset(size_t value, off_t *offset) {
    if (!offset || value > (size_t)INT64_MAX) {
        return false;
    }

    *offset = (off_t)value;
    return *offset >= 0 && (size_t)*offset == value;
}

static int open_retry(const char *path, int flags, mode_t mode) {
    int descriptor;
    do {
        descriptor = open(path, flags, mode);
    } while (descriptor < 0 && errno == EINTR);
    return descriptor;
}

static int openat_retry(int directory, const char *path, int flags, mode_t mode) {
    int descriptor;
    do {
        descriptor = openat(directory, path, flags, mode);
    } while (descriptor < 0 && errno == EINTR);
    return descriptor;
}

static int truncate_retry(int descriptor, off_t length) {
    int result;
    do {
        result = ftruncate(descriptor, length);
    } while (result < 0 && errno == EINTR);
    return result;
}

static int sync_retry(int descriptor, bool data_only) {
    int result;
    do {
        result = data_only ? fdatasync(descriptor) : fsync(descriptor);
    } while (result < 0 && errno == EINTR);
    return result;
}

static ls_result_t sync_data(int descriptor) {
    if (sync_retry(descriptor, true) == 0) {
        return LS_OK;
    }

    /* A few POSIX filesystems do not implement fdatasync; fsync is safe. */
    if (errno == EINVAL || errno == ENOSYS || errno == EOPNOTSUPP) {
        return sync_retry(descriptor, false) == 0 ? LS_OK : LS_EIO;
    }

    return LS_EIO;
}

static ls_result_t descriptor_is_regular_file(int descriptor) {
    struct stat status;
    if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode)) {
        return LS_EIO;
    }

    return LS_OK;
}

static ls_result_t write_all(int descriptor, const uint8_t *source, size_t length, size_t offset) {
    size_t completed = 0;
    while (completed < length) {
        size_t remaining = length - completed;
        size_t count = remaining > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : remaining;
        if (offset > SIZE_MAX - completed) {
            return LS_EOVERFLOW;
        }
        off_t position;
        if (!size_to_offset(offset + completed, &position)) {
            return LS_EOVERFLOW;
        }

        ssize_t result = pwrite(descriptor, source + completed, count, position);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return LS_EIO;
        }
        if (result == 0) {
            return LS_EIO;
        }
        completed += (size_t)result;
    }

    return LS_OK;
}

static ls_result_t read_all(int descriptor, uint8_t *destination, size_t length, size_t offset) {
    size_t completed = 0;
    while (completed < length) {
        size_t remaining = length - completed;
        size_t count = remaining > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : remaining;
        if (offset > SIZE_MAX - completed) {
            return LS_EOVERFLOW;
        }
        off_t position;
        if (!size_to_offset(offset + completed, &position)) {
            return LS_EOVERFLOW;
        }

        ssize_t result = pread(descriptor, destination + completed, count, position);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return LS_EIO;
        }
        if (result == 0) {
            return LS_EIO;
        }
        completed += (size_t)result;
    }

    return LS_OK;
}

static ls_result_t erase_range(int descriptor, size_t offset, size_t length) {
    uint8_t erased[LS_FILE_ERASE_CHUNK_SIZE];
    for (size_t i = 0; i < sizeof(erased); ++i) {
        erased[i] = 0xffu;
    }

    while (length) {
        size_t count = length > sizeof(erased) ? sizeof(erased) : length;
        ls_result_t result = write_all(descriptor, erased, count, offset);
        if (result != LS_OK) {
            return result;
        }
        offset += count;
        length -= count;
    }

    return LS_OK;
}

static ls_result_t preallocate_erased(int descriptor, size_t size) {
    off_t length;
    if (!size_to_offset(size, &length)) {
        return LS_EOVERFLOW;
    }

    int allocation;
    do {
        allocation = posix_fallocate(descriptor, 0, length);
    } while (allocation == EINTR);
    if (allocation != 0 && allocation != EOPNOTSUPP && allocation != ENOSYS &&
        allocation != EINVAL) {
        return LS_EIO;
    }
    if (truncate_retry(descriptor, length) != 0) {
        return LS_EIO;
    }

    ls_result_t result = erase_range(descriptor, 0, size);
    return result == LS_OK ? sync_data(descriptor) : result;
}

ls_result_t ls_file_storage_init(ls_file_storage_t *file) {
    if (storage_bounds(file, 0, 0) != LS_OK) {
        return LS_EINVAL;
    }
    if (file->initialized) {
        return file->fd >= 0 ? LS_OK : LS_EIO;
    }

    file->fd = -1;
    int descriptor =
        open_retry(file->path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
    if (descriptor < 0) {
        return LS_EIO;
    }

    ls_result_t result = descriptor_is_regular_file(descriptor);
    struct stat status;
    if (result == LS_OK && fstat(descriptor, &status) != 0) {
        result = LS_EIO;
    }
    if (result == LS_OK) {
        if (status.st_size < 0) {
            result = LS_EIO;
        } else if ((uintmax_t)status.st_size > (uintmax_t)file->size) {
            /* Do not silently truncate a backing store configured for another size. */
            result = LS_EINVAL;
        } else if ((size_t)status.st_size < file->size) {
            /* A short file is incomplete storage, so restart it as erased media. */
            result = preallocate_erased(descriptor, file->size);
        }
    }

    if (result != LS_OK) {
        (void)close(descriptor);
        return result;
    }

    file->fd = descriptor;
    file->initialized = true;
    return LS_OK;
}

ls_result_t ls_file_storage_close(ls_file_storage_t *file) {
    if (!file) {
        return LS_EINVAL;
    }
    if (!file->initialized) {
        file->fd = -1;
        return LS_OK;
    }

    int descriptor = file->fd;
    file->fd = -1;
    file->initialized = false;
    if (descriptor < 0) {
        return LS_EIO;
    }

    /* Do not retry close after EINTR: Linux may already have released the fd. */
    ls_result_t result = sync_data(descriptor);
    if (close(descriptor) != 0 && result == LS_OK) {
        result = LS_EIO;
    }
    return result;
}

ls_result_t ls_file_storage_read(void *context, size_t offset, void *dst, size_t length) {
    ls_file_storage_t *file = (ls_file_storage_t *)context;
    if ((!dst && length) || storage_bounds(file, offset, length) != LS_OK) {
        return LS_EINVAL;
    }
    ls_result_t result = ls_file_storage_init(file);
    return result == LS_OK && length ? read_all(file->fd, dst, length, offset) : result;
}

ls_result_t ls_file_storage_write(void *context, size_t offset, const void *src, size_t length) {
    ls_file_storage_t *file = (ls_file_storage_t *)context;
    if ((!src && length) || storage_bounds(file, offset, length) != LS_OK) {
        return LS_EINVAL;
    }
    ls_result_t result = ls_file_storage_init(file);
    return result == LS_OK && length ? write_all(file->fd, src, length, offset) : result;
}

ls_result_t ls_file_storage_erase(void *context, size_t offset, size_t length) {
    ls_file_storage_t *file = (ls_file_storage_t *)context;
    if (storage_bounds(file, offset, length) != LS_OK) {
        return LS_EINVAL;
    }
    ls_result_t result = ls_file_storage_init(file);
    return result == LS_OK && length ? erase_range(file->fd, offset, length) : result;
}

ls_result_t ls_file_storage_sync(void *context) {
    ls_file_storage_t *file = (ls_file_storage_t *)context;
    if (storage_bounds(file, 0, 0) != LS_OK) {
        return LS_EINVAL;
    }
    ls_result_t result = ls_file_storage_init(file);
    return result == LS_OK ? sync_data(file->fd) : result;
}

static ls_result_t write_stream_all(int descriptor, const uint8_t *source, size_t length) {
    size_t completed = 0;
    while (completed < length) {
        size_t remaining = length - completed;
        size_t count = remaining > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : remaining;
        ssize_t result = write(descriptor, source + completed, count);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return LS_EIO;
        }
        if (result == 0) {
            return LS_EIO;
        }
        completed += (size_t)result;
    }

    return LS_OK;
}

ls_result_t ls_file_transport_send(void *context, const uint8_t *data, size_t length) {
    ls_file_transport_t *transport = (ls_file_transport_t *)context;
    ls_envelope_info_t info;
    if (!transport || !transport->directory || (!data && length) ||
        ls_envelope_validate(data, length, &info) != LS_OK) {
        return LS_EINVAL;
    }

    static const char hex[] = "0123456789abcdef";
    char name[13];
    for (size_t i = 0; i < 8u; ++i) {
        unsigned shift = (unsigned)((7u - i) * 4u);
        name[i] = hex[(info.event_id >> shift) & 0x0fu];
    }
    name[8] = '.';
    name[9] = 'l';
    name[10] = 's';
    name[11] = 't';
    name[12] = '\0';

    int directory =
        open_retry(transport->directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW, 0);
    if (directory < 0) {
        return LS_EIO;
    }
    int file = openat_retry(directory, name, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW,
                            S_IRUSR | S_IWUSR);
    if (file < 0) {
        (void)close(directory);
        return LS_EIO;
    }

    ls_result_t result = descriptor_is_regular_file(file);
    if (result == LS_OK) {
        result = write_stream_all(file, data, length);
    }
    if (result == LS_OK) {
        result = sync_data(file);
    }
    if (close(file) != 0 && result == LS_OK) {
        result = LS_EIO;
    }
    if (result == LS_OK && sync_retry(directory, false) != 0) {
        result = LS_EIO;
    }
    if (close(directory) != 0 && result == LS_OK) {
        result = LS_EIO;
    }

    return result;
}
