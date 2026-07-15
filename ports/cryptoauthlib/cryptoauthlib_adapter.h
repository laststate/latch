#ifndef LASTSTATE_CRYPTOAUTHLIB_ADAPTER_H
#define LASTSTATE_CRYPTOAUTHLIB_ADAPTER_H
#include <stdint.h>
#include "laststate/secure_element.h"
typedef struct {
    uint16_t private_key_slot;
    uint16_t kdf_key_slot;
    const void *certificate_definition;
    const uint8_t *signer_ca_public_key;
} ls_cryptoauthlib_context_t;
ls_result_t ls_cryptoauthlib_secure_element_init(ls_secure_element_t *element,
                                                 ls_cryptoauthlib_context_t *context);
#endif
