#ifndef LASTSTATE_SECURE_STORAGE_H
#define LASTSTATE_SECURE_STORAGE_H
#include <stddef.h>
#include <stdint.h>
#include "security.h"
#include "storage.h"
typedef struct {
    ls_storage_backend_t backend;
    ls_storage_backend_t *storage;
    uint8_t *workspace;
    size_t workspace_size, logical_capacity, sealed_size;
    uint8_t key[LS_SECURITY_KEY_SIZE];
    uint8_t last_nonce[LS_XCHACHA20_NONCE_SIZE];
    uint32_t key_id, generation;
    bool has_last_nonce;
    ls_crypto_random_fn random;
    void *random_context;
} ls_secure_storage_t;
size_t ls_secure_storage_sealed_size(size_t logical_capacity);
ls_result_t ls_secure_storage_init(ls_secure_storage_t *secure, ls_storage_backend_t *storage,
                                   uint8_t *workspace, size_t workspace_size,
                                   size_t logical_capacity, const uint8_t key[LS_SECURITY_KEY_SIZE],
                                   uint32_t key_id, ls_crypto_random_fn random,
                                   void *random_context);
void ls_secure_storage_destroy(ls_secure_storage_t *secure);
/* Re-encrypt the complete logical image with a new master key and key ID.
   Use an atomic
 * mirror/wear-level backend so interrupted rotation can fall
   back to the previously committed
 * image. */
ls_result_t ls_secure_storage_rotate_key(ls_secure_storage_t *secure,
                                         const uint8_t new_key[LS_SECURITY_KEY_SIZE],
                                         uint32_t new_key_id);
#endif
