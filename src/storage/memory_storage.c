#include "../core/internal.h"

static ls_result_t memory_bounds(const ls_memory_storage_t *memory, size_t offset, size_t length) {
    if (!memory || !memory->data || offset > memory->size || length > memory->size - offset) {
        return LS_EINVAL;
    }

    return LS_OK;
}

static ls_result_t storage_bounds(const ls_storage_backend_t *storage, size_t offset,
                                  size_t length) {
    if (!storage || !storage->read || !storage->write || offset > storage->capacity ||
        length > storage->capacity - offset) {
        return LS_EINVAL;
    }

    return LS_OK;
}

ls_result_t ls_memory_storage_read(void *context, size_t offset, void *dst, size_t length) {
    ls_memory_storage_t *memory = (ls_memory_storage_t *)context;
    if (!dst && length) {
        return LS_EINVAL;
    }

    ls_result_t result = memory_bounds(memory, offset, length);
    if (result == LS_OK && length) {
        ls_memcpy(dst, memory->data + offset, length);
    }

    return result;
}

ls_result_t ls_memory_storage_write(void *context, size_t offset, const void *src, size_t length) {
    ls_memory_storage_t *memory = (ls_memory_storage_t *)context;
    if (!src && length) {
        return LS_EINVAL;
    }

    ls_result_t result = memory_bounds(memory, offset, length);
    if (result == LS_OK && length) {
        ls_memcpy(memory->data + offset, src, length);
    }

    return result;
}

ls_result_t ls_memory_storage_erase(void *context, size_t offset, size_t length) {
    ls_memory_storage_t *memory = (ls_memory_storage_t *)context;
    ls_result_t result = memory_bounds(memory, offset, length);
    if (result == LS_OK) {
        ls_memset(memory->data + offset, 0xff, length);
    }

    return result;
}

ls_result_t ls_storage_program(ls_storage_backend_t *storage, size_t offset, const void *src,
                               size_t length) {
    const uint8_t *input = (const uint8_t *)src;
    if ((!src && length) || storage_bounds(storage, offset, length) != LS_OK) {
        return LS_EINVAL;
    }

    if (!length) {
        return LS_OK;
    }

    size_t write_size = storage->write_size ? storage->write_size : 1u;
    if (write_size > LS_STORAGE_MAX_WRITE_SIZE) {
        return LS_ENOTSUP;
    }

    if (offset > SIZE_MAX - length) {
        return LS_EOVERFLOW;
    }

    if (write_size == 1u) {
        return storage->write(storage->context, offset, src, length);
    }

    size_t first = offset - (offset % write_size);
    size_t target_end = offset + length;
    size_t last = target_end;
    if (target_end > SIZE_MAX - (write_size - 1u)) {
        return LS_EOVERFLOW;
    }
    last = ((last + write_size - 1u) / write_size) * write_size;
    if (last > storage->capacity) {
        return LS_ENOSPACE;
    }

    for (size_t unit_offset = first; unit_offset < last; unit_offset += write_size) {
        uint8_t unit[LS_STORAGE_MAX_WRITE_SIZE];
        ls_result_t result = storage->read(storage->context, unit_offset, unit, write_size);
        if (result != LS_OK) {
            return result;
        }

        size_t unit_end = unit_offset + write_size;
        size_t begin = offset > unit_offset ? offset : unit_offset;
        size_t end = target_end < unit_end ? target_end : unit_end;

        bool changed = false;
        for (size_t position = begin; position < end; position++) {
            size_t index = position - unit_offset;
            uint8_t value = input[position - offset];
            if (((uint8_t)~unit[index] & value) != 0u) {
                return LS_EIO;
            }

            uint8_t programmed = (uint8_t)(unit[index] & value);
            if (programmed != unit[index]) {
                unit[index] = programmed;
                changed = true;
            }
        }

        if (changed) {
            result = storage->write(storage->context, unit_offset, unit, write_size);
            if (result != LS_OK) {
                return result;
            }
        }
    }

    return LS_OK;
}

ls_result_t ls_storage_is_erased(ls_storage_backend_t *storage, size_t offset, size_t length,
                                 int *erased) {
    if (!erased || storage_bounds(storage, offset, length) != LS_OK) {
        return LS_EINVAL;
    }

    uint8_t bytes[LS_STORAGE_MAX_WRITE_SIZE];
    while (length) {
        size_t count = length > sizeof(bytes) ? sizeof(bytes) : length;
        ls_result_t result = storage->read(storage->context, offset, bytes, count);
        if (result != LS_OK) {
            return result;
        }

        for (size_t index = 0; index < count; index++) {
            if (bytes[index] != 0xffu) {
                *erased = 0;
                return LS_OK;
            }
        }

        offset += count;
        length -= count;
    }

    *erased = 1;
    return LS_OK;
}

void ls_storage_register(ls_storage_backend_t *storage) {
    ls_runtime.storage = storage;
}
