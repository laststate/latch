#include "../core/internal.h"
#include "laststate/secure_storage.h"
#define SECURE_STORAGE_MAGIC 0x5353534cu
#define SECURE_STORAGE_VERSION 1u
typedef struct {
    uint32_t magic, version, key_id, generation, length;
    uint8_t nonce[24];
    uint32_t header_crc;
} secure_header_t;
size_t ls_secure_storage_sealed_size(size_t logical_capacity) {
    return sizeof(secure_header_t) + logical_capacity + 4u + LS_AEAD_TAG_SIZE;
}
static uint32_t read32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}
static void write32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}
static uint32_t header_crc(const secure_header_t *header) {
    return ls_crc32(header, offsetof(secure_header_t, header_crc));
}
static ls_result_t derive_key(const ls_secure_storage_t *secure, uint32_t generation,
                              uint8_t derived[32]) {
    static const uint8_t label[] = "laststate/latch/storage/v1";
    uint8_t salt[8] = {(uint8_t)secure->key_id,
                       (uint8_t)(secure->key_id >> 8),
                       (uint8_t)(secure->key_id >> 16),
                       (uint8_t)(secure->key_id >> 24),
                       (uint8_t)generation,
                       (uint8_t)(generation >> 8),
                       (uint8_t)(generation >> 16),
                       (uint8_t)(generation >> 24)};
    return ls_hkdf_sha256(salt, sizeof salt, secure->key, sizeof secure->key, label,
                          sizeof label - 1u, derived, 32u);
}
static ls_result_t load_image(ls_secure_storage_t *secure) {
    ls_result_t result =
        secure->storage->read(secure->storage->context, 0, secure->workspace, secure->sealed_size);
    if (result != LS_OK)
        return result;
    secure_header_t header;
    ls_memcpy(&header, secure->workspace, sizeof header);
    if (header.magic != SECURE_STORAGE_MAGIC || header.version != SECURE_STORAGE_VERSION ||
        header.key_id != secure->key_id || header.length != secure->logical_capacity ||
        header.header_crc != header_crc(&header))
        return LS_ECORRUPT;
    uint8_t *ciphertext = secure->workspace + sizeof header;
    uint32_t stored_crc = read32(ciphertext + secure->logical_capacity);
    if (stored_crc != ls_crc32(ciphertext, secure->logical_capacity))
        return LS_ECORRUPT;
    uint8_t derived[32];
    result = derive_key(secure, header.generation, derived);
    if (result == LS_OK)
        result = ls_xchacha20_poly1305_decrypt(
            derived, header.nonce, secure->workspace, sizeof header, ciphertext, ciphertext,
            secure->logical_capacity, ciphertext + secure->logical_capacity + 4u);
    ls_secure_zero(derived, sizeof derived);
    if (result == LS_OK) {
        secure->generation = header.generation;
        ls_memcpy(secure->last_nonce, header.nonce, sizeof secure->last_nonce);
        secure->has_last_nonce = true;
    } else
        ls_secure_zero(ciphertext, secure->logical_capacity);
    ls_secure_zero(&header, sizeof header);
    return result;
}
static ls_result_t seal_image(ls_secure_storage_t *secure) {
    secure_header_t header;
    ls_memset(&header, 0, sizeof header);
    header.magic = SECURE_STORAGE_MAGIC;
    header.version = SECURE_STORAGE_VERSION;
    header.key_id = secure->key_id;
    header.generation = secure->generation + 1u;
    header.length = (uint32_t)secure->logical_capacity;
    ls_result_t result =
        secure->random ? secure->random(secure->random_context, header.nonce, sizeof header.nonce)
                       : LS_ENOTSUP;
    if (result != LS_OK) {
        ls_secure_zero(secure->workspace, secure->workspace_size);
        return result;
    }
    if (secure->has_last_nonce &&
        ls_constant_time_equal(header.nonce, secure->last_nonce, sizeof header.nonce)) {
        ls_secure_zero(secure->workspace, secure->workspace_size);
        ls_secure_zero(&header, sizeof header);
        return LS_EAUTH;
    }
    header.header_crc = header_crc(&header);
    ls_memcpy(secure->workspace, &header, sizeof header);
    uint8_t *ciphertext = secure->workspace + sizeof header,
            *tag = ciphertext + secure->logical_capacity + 4u;
    uint8_t derived[32];
    result = derive_key(secure, header.generation, derived);
    if (result == LS_OK)
        result = ls_xchacha20_poly1305_encrypt(derived, header.nonce, (const uint8_t *)&header,
                                               sizeof header, ciphertext, ciphertext,
                                               secure->logical_capacity, tag);
    ls_secure_zero(derived, sizeof derived);
    if (result == LS_OK)
        write32(ciphertext + secure->logical_capacity,
                ls_crc32(ciphertext, secure->logical_capacity));
    if (result == LS_OK)
        result = secure->storage->write(secure->storage->context, 0, secure->workspace,
                                        secure->sealed_size);
    if (result == LS_OK && secure->storage->sync)
        result = secure->storage->sync(secure->storage->context);
    if (result == LS_OK) {
        secure->generation = header.generation;
        ls_memcpy(secure->last_nonce, header.nonce, sizeof secure->last_nonce);
        secure->has_last_nonce = true;
    }
    ls_secure_zero(secure->workspace, secure->workspace_size);
    ls_secure_zero(&header, sizeof header);
    return result;
}
static ls_result_t secure_read(void *context, size_t offset, void *destination, size_t length) {
    ls_secure_storage_t *secure = (ls_secure_storage_t *)context;
    if (!secure || (!destination && length) || offset > secure->logical_capacity ||
        length > secure->logical_capacity - offset)
        return LS_EINVAL;
    ls_result_t result = load_image(secure);
    if (result == LS_OK && length)
        ls_memcpy(destination, secure->workspace + sizeof(secure_header_t) + offset, length);
    ls_secure_zero(secure->workspace, secure->workspace_size);
    return result;
}
static ls_result_t update_image(ls_secure_storage_t *secure, size_t offset, const void *source,
                                size_t length, bool erase) {
    if (!secure || (!source && !erase && length) || offset > secure->logical_capacity ||
        length > secure->logical_capacity - offset)
        return LS_EINVAL;
    ls_result_t result = load_image(secure);
    if (result != LS_OK) {
        ls_secure_zero(secure->workspace, secure->workspace_size);
        return result;
    }
    uint8_t *plaintext = secure->workspace + sizeof(secure_header_t);
    if (erase)
        ls_memset(plaintext + offset, 0xff, length);
    else if (length)
        ls_memcpy(plaintext + offset, source, length);
    return seal_image(secure);
}
static ls_result_t secure_write(void *context, size_t offset, const void *source, size_t length) {
    return update_image((ls_secure_storage_t *)context, offset, source, length, false);
}
static ls_result_t secure_erase(void *context, size_t offset, size_t length) {
    return update_image((ls_secure_storage_t *)context, offset, 0, length, true);
}
static ls_result_t secure_sync(void *context) {
    ls_secure_storage_t *secure = (ls_secure_storage_t *)context;
    return secure->storage->sync ? secure->storage->sync(secure->storage->context) : LS_OK;
}
ls_result_t ls_secure_storage_init(ls_secure_storage_t *secure, ls_storage_backend_t *storage,
                                   uint8_t *workspace, size_t workspace_size,
                                   size_t logical_capacity, const uint8_t key[32], uint32_t key_id,
                                   ls_crypto_random_fn random, void *random_context) {
    if (!secure || !storage || !storage->read || !storage->write || !workspace ||
        !logical_capacity || logical_capacity > UINT32_MAX || !key || !key_id || !random)
        return LS_EINVAL;
    size_t sealed_size = ls_secure_storage_sealed_size(logical_capacity);
    if (workspace_size < sealed_size || storage->capacity < sealed_size)
        return LS_ENOSPACE;
    ls_memset(secure, 0, sizeof *secure);
    secure->storage = storage;
    secure->workspace = workspace;
    secure->workspace_size = workspace_size;
    secure->logical_capacity = logical_capacity;
    secure->sealed_size = sealed_size;
    secure->key_id = key_id;
    secure->random = random;
    secure->random_context = random_context;
    ls_memcpy(secure->key, key, 32);
    secure_header_t header;
    ls_result_t result = storage->read(storage->context, 0, &header, sizeof header);
    if (result != LS_OK)
        return result;
    if (header.magic == SECURE_STORAGE_MAGIC) {
        result = load_image(secure);
        ls_secure_zero(workspace, workspace_size);
        if (result != LS_OK)
            return result;
    } else {
        const uint8_t *bytes = (const uint8_t *)&header;
        for (size_t i = 0; i < sizeof header; i++)
            if (bytes[i] != 0xffu) {
                ls_secure_zero(secure->key, sizeof secure->key);
                return LS_ECORRUPT;
            }
        ls_memset(workspace, 0, sizeof header);
        ls_memset(workspace + sizeof header, 0xff, logical_capacity);
        result = seal_image(secure);
        if (result != LS_OK)
            return result;
    }
    secure->backend = (ls_storage_backend_t){"xchacha20-poly1305-storage",
                                             secure,
                                             logical_capacity,
                                             1u,
                                             1u,
                                             secure_read,
                                             secure_write,
                                             secure_erase,
                                             secure_sync};
    return LS_OK;
}
void ls_secure_storage_destroy(ls_secure_storage_t *secure) {
    if (!secure)
        return;
    ls_secure_zero(secure->key, sizeof secure->key);
    if (secure->workspace)
        ls_secure_zero(secure->workspace, secure->workspace_size);
    secure->backend.read = 0;
    secure->backend.write = 0;
    secure->backend.erase = 0;
    secure->backend.sync = 0;
}

ls_result_t ls_secure_storage_rotate_key(ls_secure_storage_t *secure,
                                         const uint8_t new_key[LS_SECURITY_KEY_SIZE],
                                         uint32_t new_key_id) {
    if (!secure || !new_key || new_key_id == 0u || new_key_id == secure->key_id ||
        !secure->storage || !secure->workspace) {
        return LS_EINVAL;
    }
    ls_result_t result = load_image(secure);
    if (result != LS_OK) {
        ls_secure_zero(secure->workspace, secure->workspace_size);
        return result;
    }

    uint8_t old_key[LS_SECURITY_KEY_SIZE];
    uint8_t old_nonce[LS_XCHACHA20_NONCE_SIZE];
    uint32_t old_key_id = secure->key_id;
    uint32_t old_generation = secure->generation;
    bool old_has_last_nonce = secure->has_last_nonce;
    ls_memcpy(old_key, secure->key, sizeof old_key);
    ls_memcpy(old_nonce, secure->last_nonce, sizeof old_nonce);
    ls_memcpy(secure->key, new_key, sizeof secure->key);
    secure->key_id = new_key_id;
    result = seal_image(secure);
    if (result != LS_OK) {
        ls_memcpy(secure->key, old_key, sizeof secure->key);
        secure->key_id = old_key_id;
        secure->generation = old_generation;
        secure->has_last_nonce = old_has_last_nonce;
        ls_memcpy(secure->last_nonce, old_nonce, sizeof secure->last_nonce);
    }
    ls_secure_zero(old_key, sizeof old_key);
    ls_secure_zero(old_nonce, sizeof old_nonce);
    return result;
}
