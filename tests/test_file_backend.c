#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "file_backend.h"
#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "file backend check failed: %s line %d\\n", #condition, __LINE__); \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

enum { TEST_STORAGE_SIZE = 8193u };

static const uint8_t envelope[] = {
    0x4c, 0x53, 0x54, 0x50, 0x01, 0x02, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x74, 0xdd, 0xc4, 0x89,
    0x01, 0x00, 0x02, 0x00, 0xaa, 0xbb, 0xea, 0x84, 0xcc, 0xd8,
};

static int make_missing_path(char path[], size_t capacity) {
    if (capacity < sizeof("/tmp/latch-file-backend-XXXXXX")) {
        return -1;
    }
    (void)snprintf(path, capacity, "%s", "/tmp/latch-file-backend-XXXXXX");
    int descriptor = mkstemp(path);
    if (descriptor < 0) {
        return -1;
    }
    if (close(descriptor) != 0 || unlink(path) != 0) {
        return -1;
    }
    return 0;
}

static int write_exact(int descriptor, const uint8_t *data, size_t length) {
    size_t written = 0;
    while (written < length) {
        ssize_t result = write(descriptor, data + written, length - written);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return -1;
        }
        written += (size_t)result;
    }
    return 0;
}

static int read_exact(int descriptor, uint8_t *data, size_t length) {
    size_t read_count = 0;
    while (read_count < length) {
        ssize_t result = read(descriptor, data + read_count, length - read_count);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return -1;
        }
        read_count += (size_t)result;
    }
    return 0;
}

static int test_invalid_storage_inputs(void) {
    const uint8_t byte = 0x5au;
    uint8_t destination = 0;
    ls_file_storage_t no_path = LS_FILE_STORAGE_INITIALIZER(NULL, 16u);
    ls_file_storage_t no_size = LS_FILE_STORAGE_INITIALIZER("/tmp/unused", 0u);
    ls_file_storage_t unopened = LS_FILE_STORAGE_INITIALIZER("/tmp/unused", 16u);
    ls_file_storage_t invalid_descriptor = LS_FILE_STORAGE_INITIALIZER("/tmp/unused", 16u);

    CHECK(ls_file_storage_init(NULL) == LS_EINVAL);
    CHECK(ls_file_storage_init(&no_path) == LS_EINVAL);
    CHECK(ls_file_storage_init(&no_size) == LS_EINVAL);
    CHECK(ls_file_storage_close(NULL) == LS_EINVAL);

    unopened.fd = 123;
    CHECK(ls_file_storage_close(&unopened) == LS_OK);
    CHECK(unopened.fd == -1 && !unopened.initialized);

    invalid_descriptor.initialized = true;
    invalid_descriptor.fd = -1;
    CHECK(ls_file_storage_close(&invalid_descriptor) == LS_EIO);
    CHECK(invalid_descriptor.fd == -1 && !invalid_descriptor.initialized);

    CHECK(ls_file_storage_read(NULL, 0u, &destination, sizeof(destination)) == LS_EINVAL);
    CHECK(ls_file_storage_write(NULL, 0u, &byte, sizeof(byte)) == LS_EINVAL);
    CHECK(ls_file_storage_erase(NULL, 0u, sizeof(byte)) == LS_EINVAL);
    CHECK(ls_file_storage_sync(NULL) == LS_EINVAL);
    CHECK(ls_file_storage_read(&no_path, 0u, &destination, sizeof(destination)) == LS_EINVAL);
    CHECK(ls_file_storage_write(&no_path, 0u, &byte, sizeof(byte)) == LS_EINVAL);
    CHECK(ls_file_storage_erase(&no_path, 0u, sizeof(byte)) == LS_EINVAL);
    CHECK(ls_file_storage_sync(&no_path) == LS_EINVAL);
    return 0;
}

static int test_storage_path_rejections(void) {
    char path[64];
    char target[64];
    char link[64];
    char directory[] = "/tmp/latch-file-directory-XXXXXX";

    CHECK(mkdtemp(directory) != NULL);
    ls_file_storage_t storage = LS_FILE_STORAGE_INITIALIZER(directory, 16u);
    CHECK(ls_file_storage_init(&storage) == LS_EIO);
    CHECK(!storage.initialized && storage.fd == -1);
    CHECK(rmdir(directory) == 0);

    CHECK(make_missing_path(path, sizeof(path)) == 0);
    CHECK(mkfifo(path, S_IRUSR | S_IWUSR) == 0);
    storage = (ls_file_storage_t)LS_FILE_STORAGE_INITIALIZER(path, 16u);
    CHECK(ls_file_storage_init(&storage) == LS_EIO);
    CHECK(!storage.initialized && storage.fd == -1);
    CHECK(unlink(path) == 0);

    CHECK(make_missing_path(target, sizeof(target)) == 0);
    int descriptor = open(target, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(make_missing_path(link, sizeof(link)) == 0);
    CHECK(symlink(target, link) == 0);
    storage = (ls_file_storage_t)LS_FILE_STORAGE_INITIALIZER(link, 16u);
    CHECK(ls_file_storage_init(&storage) == LS_EIO);
    CHECK(!storage.initialized && storage.fd == -1);
    CHECK(unlink(link) == 0);
    CHECK(unlink(target) == 0);
    return 0;
}

static int test_storage_error_propagation(void) {
    int pipe_fds[2];
    CHECK(pipe(pipe_fds) == 0);

    const uint8_t byte = 0x5au;
    uint8_t destination = 0;
    ls_file_storage_t storage = LS_FILE_STORAGE_INITIALIZER("/tmp/latch-unavailable", 16u);
    /* A descriptor invalidated outside the backend must not yield a partial success. */
    storage.initialized = true;
    storage.fd = pipe_fds[1];
    CHECK(ls_file_storage_read(&storage, 0u, &destination, sizeof(destination)) == LS_EIO);
    CHECK(ls_file_storage_write(&storage, 0u, &byte, sizeof(byte)) == LS_EIO);
    CHECK(ls_file_storage_erase(&storage, 0u, sizeof(byte)) == LS_EIO);
    /* Pipes also exercise the fdatasync-to-fsync fallback's error propagation. */
    CHECK(ls_file_storage_sync(&storage) == LS_EIO);
    CHECK(ls_file_storage_close(&storage) == LS_EIO);
    CHECK(close(pipe_fds[0]) == 0);

#if SIZE_MAX > INT64_MAX
    char path[64];
    CHECK(make_missing_path(path, sizeof(path)) == 0);
    int descriptor = open(path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    CHECK(descriptor >= 0);
    storage = (ls_file_storage_t)LS_FILE_STORAGE_INITIALIZER(path, SIZE_MAX);
    storage.initialized = true;
    storage.fd = descriptor;
    const size_t unrepresentable_offset = (size_t)INT64_MAX + 1u;
    CHECK(ls_file_storage_read(&storage, unrepresentable_offset, &destination,
                               sizeof(destination)) == LS_EOVERFLOW);
    CHECK(ls_file_storage_write(&storage, unrepresentable_offset, &byte, sizeof(byte)) ==
          LS_EOVERFLOW);
    CHECK(ls_file_storage_close(&storage) == LS_OK);
    CHECK(unlink(path) == 0);
#endif
    return 0;
}

static int test_storage_primitives(void) {
    char path[64];
    CHECK(make_missing_path(path, sizeof(path)) == 0);

    ls_file_storage_t storage = LS_FILE_STORAGE_INITIALIZER(path, TEST_STORAGE_SIZE);
    uint8_t bytes[128];
    CHECK(ls_file_storage_init(&storage) == LS_OK);
    CHECK(ls_file_storage_init(&storage) == LS_OK);
    CHECK(ls_file_storage_read(&storage, 0u, bytes, sizeof(bytes)) == LS_OK);
    for (size_t index = 0; index < sizeof(bytes); index++) {
        CHECK(bytes[index] == 0xffu);
    }

    /* Zero-length callbacks are valid and do not require a dummy buffer. */
    CHECK(ls_file_storage_read(&storage, TEST_STORAGE_SIZE, NULL, 0u) == LS_OK);
    CHECK(ls_file_storage_write(&storage, TEST_STORAGE_SIZE, NULL, 0u) == LS_OK);
    CHECK(ls_file_storage_erase(&storage, TEST_STORAGE_SIZE, 0u) == LS_OK);

    const uint8_t first[] = {0x12u, 0x34u, 0x56u, 0x78u};
    const uint8_t last[] = {0x87u, 0x65u, 0x43u, 0x21u};
    CHECK(ls_file_storage_write(&storage, 12u, first, sizeof(first)) == LS_OK);
    CHECK(ls_file_storage_write(&storage, TEST_STORAGE_SIZE - sizeof(last), last, sizeof(last)) ==
          LS_OK);
    memset(bytes, 0, sizeof(bytes));
    CHECK(ls_file_storage_read(&storage, 12u, bytes, sizeof(first)) == LS_OK);
    CHECK(memcmp(bytes, first, sizeof(first)) == 0);
    CHECK(ls_file_storage_read(&storage, TEST_STORAGE_SIZE - sizeof(last), bytes, sizeof(last)) ==
          LS_OK);
    CHECK(memcmp(bytes, last, sizeof(last)) == 0);

    /* This crosses the backend's erase chunk boundary and must erase every byte. */
    CHECK(ls_file_storage_erase(&storage, 0u, TEST_STORAGE_SIZE) == LS_OK);
    CHECK(ls_file_storage_read(&storage, 0u, bytes, sizeof(bytes)) == LS_OK);
    for (size_t index = 0; index < sizeof(bytes); index++) {
        CHECK(bytes[index] == 0xffu);
    }
    CHECK(ls_file_storage_read(&storage, TEST_STORAGE_SIZE - sizeof(bytes), bytes, sizeof(bytes)) ==
          LS_OK);
    for (size_t index = 0; index < sizeof(bytes); index++) {
        CHECK(bytes[index] == 0xffu);
    }

    const uint8_t persisted[] = {0x5au, 0xa5u};
    CHECK(ls_file_storage_write(&storage, 4u, persisted, sizeof(persisted)) == LS_OK);
    CHECK(ls_file_storage_sync(&storage) == LS_OK);
    CHECK(ls_file_storage_read(&storage, TEST_STORAGE_SIZE, bytes, 1u) == LS_EINVAL);
    CHECK(ls_file_storage_read(&storage, TEST_STORAGE_SIZE - 1u, bytes, 2u) == LS_EINVAL);
    CHECK(ls_file_storage_write(&storage, 0u, NULL, 1u) == LS_EINVAL);
    CHECK(ls_file_storage_write(&storage, TEST_STORAGE_SIZE - 1u, first, sizeof(first)) ==
          LS_EINVAL);
    CHECK(ls_file_storage_erase(&storage, TEST_STORAGE_SIZE - 1u, sizeof(first)) == LS_EINVAL);
    CHECK(ls_file_storage_close(&storage) == LS_OK);
    CHECK(ls_file_storage_close(&storage) == LS_OK);

    /* A correctly-sized store is preserved across an explicit reopen. */
    storage = (ls_file_storage_t)LS_FILE_STORAGE_INITIALIZER(path, TEST_STORAGE_SIZE);
    CHECK(ls_file_storage_init(&storage) == LS_OK);
    CHECK(ls_file_storage_read(&storage, 4u, bytes, sizeof(persisted)) == LS_OK);
    CHECK(memcmp(bytes, persisted, sizeof(persisted)) == 0);
    CHECK(ls_file_storage_close(&storage) == LS_OK);

    /* An externally truncated active backing file is surfaced as an I/O error. */
    storage = (ls_file_storage_t)LS_FILE_STORAGE_INITIALIZER(path, TEST_STORAGE_SIZE);
    CHECK(ls_file_storage_init(&storage) == LS_OK);
    CHECK(ftruncate(storage.fd, 0) == 0);
    CHECK(ls_file_storage_read(&storage, 0u, bytes, 1u) == LS_EIO);
    CHECK(ls_file_storage_close(&storage) == LS_OK);

    /* A short nonzero file is reset as erased media before it is exposed. */
    int descriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    CHECK(descriptor >= 0);
    CHECK(write_exact(descriptor, first, sizeof(first)) == 0);
    CHECK(close(descriptor) == 0);
    storage = (ls_file_storage_t)LS_FILE_STORAGE_INITIALIZER(path, TEST_STORAGE_SIZE);
    CHECK(ls_file_storage_init(&storage) == LS_OK);
    CHECK(ls_file_storage_read(&storage, 0u, bytes, sizeof(first)) == LS_OK);
    for (size_t index = 0; index < sizeof(first); index++) {
        CHECK(bytes[index] == 0xffu);
    }
    CHECK(ls_file_storage_close(&storage) == LS_OK);

    /* Lazy initialization repairs a zero-length file before its first read. */
    descriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    storage = (ls_file_storage_t)LS_FILE_STORAGE_INITIALIZER(path, TEST_STORAGE_SIZE);
    CHECK(ls_file_storage_read(&storage, 0u, bytes, sizeof(bytes)) == LS_OK);
    for (size_t index = 0; index < sizeof(bytes); index++) {
        CHECK(bytes[index] == 0xffu);
    }
    CHECK(ls_file_storage_close(&storage) == LS_OK);

    /* A larger store must not be truncated accidentally by a smaller configuration. */
    descriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    CHECK(descriptor >= 0);
    CHECK(ftruncate(descriptor, (off_t)TEST_STORAGE_SIZE + 1) == 0);
    CHECK(close(descriptor) == 0);
    storage = (ls_file_storage_t)LS_FILE_STORAGE_INITIALIZER(path, TEST_STORAGE_SIZE);
    CHECK(ls_file_storage_init(&storage) == LS_EINVAL);
    struct stat status;
    CHECK(stat(path, &status) == 0);
    CHECK(status.st_size == (off_t)TEST_STORAGE_SIZE + 1);
    CHECK(ls_file_storage_close(&storage) == LS_OK);

#if SIZE_MAX > INT64_MAX
    /* Reject storage sizes that cannot be represented by POSIX file offsets. */
    storage = (ls_file_storage_t)LS_FILE_STORAGE_INITIALIZER(path, SIZE_MAX);
    CHECK(ls_file_storage_init(&storage) == LS_EOVERFLOW);
    CHECK(!storage.initialized && storage.fd == -1);
#endif

    CHECK(unlink(path) == 0);
    return 0;
}

static int test_transport_rejections(void) {
    char directory[] = "/tmp/latch-file-transport-reject-XXXXXX";
    char missing[64];
    char ordinary_file[64];
    char directory_link[64];
    char target[64];
    char output[96];
    ls_file_transport_t no_directory = {.directory = NULL};

    CHECK(mkdtemp(directory) != NULL);
    ls_file_transport_t transport = {.directory = directory};
    CHECK(ls_file_transport_send(NULL, envelope, sizeof(envelope)) == LS_EINVAL);
    CHECK(ls_file_transport_send(&no_directory, envelope, sizeof(envelope)) == LS_EINVAL);
    CHECK(ls_file_transport_send(&transport, NULL, sizeof(envelope)) == LS_EINVAL);
    CHECK(ls_file_transport_send(&transport, envelope, 0u) == LS_EINVAL);
    uint8_t malformed[sizeof(envelope)];
    memcpy(malformed, envelope, sizeof(malformed));
    malformed[0] ^= 0xffu;
    CHECK(ls_file_transport_send(&transport, malformed, sizeof(malformed)) == LS_EINVAL);

    CHECK(make_missing_path(missing, sizeof(missing)) == 0);
    ls_file_transport_t missing_transport = {.directory = missing};
    CHECK(ls_file_transport_send(&missing_transport, envelope, sizeof(envelope)) == LS_EIO);

    CHECK(make_missing_path(ordinary_file, sizeof(ordinary_file)) == 0);
    int descriptor = open(ordinary_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    ls_file_transport_t file_transport = {.directory = ordinary_file};
    CHECK(ls_file_transport_send(&file_transport, envelope, sizeof(envelope)) == LS_EIO);
    CHECK(unlink(ordinary_file) == 0);

    CHECK(make_missing_path(directory_link, sizeof(directory_link)) == 0);
    CHECK(symlink(directory, directory_link) == 0);
    ls_file_transport_t link_transport = {.directory = directory_link};
    CHECK(ls_file_transport_send(&link_transport, envelope, sizeof(envelope)) == LS_EIO);
    CHECK(unlink(directory_link) == 0);

    int output_length = snprintf(output, sizeof(output), "%s/%08x.lst", directory, 9u);
    CHECK(output_length > 0 && (size_t)output_length < sizeof(output));
    CHECK(make_missing_path(target, sizeof(target)) == 0);
    descriptor = open(target, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    CHECK(descriptor >= 0);
    const uint8_t sentinel[] = {0xdeu, 0xadu, 0xbeu, 0xefu};
    CHECK(write_exact(descriptor, sentinel, sizeof(sentinel)) == 0);
    CHECK(close(descriptor) == 0);
    CHECK(symlink(target, output) == 0);
    CHECK(ls_file_transport_send(&transport, envelope, sizeof(envelope)) == LS_EIO);
    descriptor = open(target, O_RDONLY);
    CHECK(descriptor >= 0);
    uint8_t received[sizeof(sentinel)];
    CHECK(read_exact(descriptor, received, sizeof(received)) == 0);
    CHECK(close(descriptor) == 0);
    CHECK(memcmp(received, sentinel, sizeof(sentinel)) == 0);
    CHECK(unlink(output) == 0);
    CHECK(unlink(target) == 0);
    CHECK(rmdir(directory) == 0);
    return 0;
}

static int test_boot_and_transport(void) {
    char path[64];
    char directory[] = "/tmp/latch-file-transport-XXXXXX";
    CHECK(make_missing_path(path, sizeof(path)) == 0);
    CHECK(mkdtemp(directory) != NULL);

    size_t capacity = ls_storage_required_size();
    CHECK(capacity > 0u);
    ls_file_storage_t file = LS_FILE_STORAGE_INITIALIZER(path, capacity);
    ls_storage_backend_t storage = {.name = "linux-file",
                                    .context = &file,
                                    .capacity = capacity,
                                    .erase_size = 1u,
                                    .write_size = 1u,
                                    .read = ls_file_storage_read,
                                    .write = ls_file_storage_write,
                                    .erase = ls_file_storage_erase,
                                    .sync = ls_file_storage_sync};
    ls_identity_t identity = {.project_id = "test",
                              .device_id = "linux-file",
                              .firmware_build_id = "linux-file-backend"};
    ls_config_t config = {.identity = &identity, .architecture = LS_ARCH_LINUX};

    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    /* The backing path is missing: ls_boot() exercises lazy erased initialization. */
    CHECK(ls_boot() == LS_OK);
    struct stat status;
    CHECK(stat(path, &status) == 0);
    CHECK((size_t)status.st_size == capacity);

    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    CHECK(ls_boot() == LS_OK);
    CHECK(ls_boot_count() == 2u);

    ls_file_transport_t transport = {.directory = directory};
    CHECK(ls_file_transport_send(&transport, envelope, sizeof(envelope)) == LS_OK);
    char output[96];
    int output_length = snprintf(output, sizeof(output), "%s/%08x.lst", directory, 9u);
    CHECK(output_length > 0 && (size_t)output_length < sizeof(output));
    int output_file = open(output, O_RDONLY);
    CHECK(output_file >= 0);
    uint8_t received[sizeof(envelope)];
    CHECK(read_exact(output_file, received, sizeof(received)) == 0);
    CHECK(close(output_file) == 0);
    CHECK(memcmp(received, envelope, sizeof(envelope)) == 0);

    CHECK(ls_file_storage_close(&file) == LS_OK);
    CHECK(unlink(output) == 0);
    CHECK(rmdir(directory) == 0);
    CHECK(unlink(path) == 0);
    return 0;
}

int main(void) {
    if (test_invalid_storage_inputs() != 0) {
        return 1;
    }
    if (test_storage_path_rejections() != 0) {
        return 1;
    }
    if (test_storage_error_propagation() != 0) {
        return 1;
    }
    if (test_storage_primitives() != 0) {
        return 1;
    }
    if (test_transport_rejections() != 0) {
        return 1;
    }
    return test_boot_and_transport();
}
