#include "cryptoauthlib_adapter.h"
#include <cryptoauthlib.h>
#include <atcacert/atcacert_client.h>
static ls_result_t status_result(ATCA_STATUS status) {
    return status == ATCA_SUCCESS ? LS_OK : LS_EIO;
}
static ls_result_t random_bytes(void *context, uint8_t *output, size_t length) {
    (void)context;
    while (length) {
        uint8_t block[32];
        ATCA_STATUS status = atcab_random(block);
        if (status != ATCA_SUCCESS)
            return LS_EIO;
        size_t take = length > sizeof block ? sizeof block : length;
        for (size_t i = 0; i < take; i++)
            output[i] = block[i];
        output += take;
        length -= take;
    }
    return LS_OK;
}
static ls_result_t sign_digest(void *opaque, const uint8_t digest[32], uint8_t signature[64]) {
    ls_cryptoauthlib_context_t *context = (ls_cryptoauthlib_context_t *)opaque;
    return status_result(atcab_sign(context->private_key_slot, digest, signature));
}
static ls_result_t read_certificate(void *opaque, uint8_t *output, size_t capacity,
                                    size_t *length) {
    ls_cryptoauthlib_context_t *context = (ls_cryptoauthlib_context_t *)opaque;
    if (!context->certificate_definition || !context->signer_ca_public_key || capacity > UINT32_MAX)
        return LS_EINVAL;
    size_t certificate_size = capacity;
    ATCA_STATUS status =
        atcacert_read_cert((const atcacert_def_t *)context->certificate_definition,
                           context->signer_ca_public_key, output, &certificate_size);
    if (status != ATCA_SUCCESS)
        return LS_EIO;
    *length = certificate_size;
    return LS_OK;
}
static ls_result_t derive_key(void *opaque, uint32_t key_id, const uint8_t *context_data,
                              size_t context_length, uint8_t output[32]) {
    ls_cryptoauthlib_context_t *context = (ls_cryptoauthlib_context_t *)opaque;
    if (context_length > 32u)
        return LS_EINVAL;
    uint8_t message[32] = {0};
    for (size_t i = 0; i < context_length; i++)
        message[i] = context_data[i];
    message[28] = (uint8_t)key_id;
    message[29] = (uint8_t)(key_id >> 8);
    message[30] = (uint8_t)(key_id >> 16);
    message[31] = (uint8_t)(key_id >> 24);
    return status_result(atcab_sha_hmac(message, sizeof message, context->kdf_key_slot, output, 0));
}
ls_result_t ls_cryptoauthlib_secure_element_init(ls_secure_element_t *element,
                                                 ls_cryptoauthlib_context_t *context) {
    if (!element || !context || !context->certificate_definition || !context->signer_ca_public_key)
        return LS_EINVAL;
    *element =
        (ls_secure_element_t){context, random_bytes, sign_digest, read_certificate, derive_key};
    return LS_OK;
}
