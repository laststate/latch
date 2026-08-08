#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "crypto provider failed: %s:%d\n", #condition, __LINE__);              \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct {
    unsigned hkdf_calls;
    unsigned encrypt_calls;
    unsigned decrypt_calls;
    ls_result_t hkdf_result;
    ls_result_t encrypt_result;
    ls_result_t decrypt_result;
} provider_state_t;

static uint8_t storage_bytes[50000];
static uint8_t delivered[LS_MAX_EVENT_SIZE];
static size_t delivered_length;
static uint8_t random_counter;

static ls_result_t random_bytes(void *context, uint8_t *output, size_t length) {
    (void)context;
    while (length--) {
        *output++ = ++random_counter;
    }
    return LS_OK;
}

static ls_result_t provider_hkdf(void *context, const uint8_t *salt, size_t salt_length,
                                 const uint8_t *ikm, size_t ikm_length, const uint8_t *info,
                                 size_t info_length, uint8_t *output, size_t output_length) {
    provider_state_t *state = context;
    state->hkdf_calls++;
    if (state->hkdf_result != LS_OK) {
        return state->hkdf_result;
    }
    return ls_hkdf_sha256(salt, salt_length, ikm, ikm_length, info, info_length, output,
                          output_length);
}

static ls_result_t provider_encrypt(void *context, const uint8_t key[32], const uint8_t nonce[24],
                                    const uint8_t *aad, size_t aad_length, const uint8_t *plaintext,
                                    uint8_t *ciphertext, size_t length, uint8_t tag[16]) {
    provider_state_t *state = context;
    state->encrypt_calls++;
    if (state->encrypt_result != LS_OK) {
        return state->encrypt_result;
    }
    return ls_xchacha20_poly1305_encrypt(key, nonce, aad, aad_length, plaintext, ciphertext, length,
                                         tag);
}

static ls_result_t provider_decrypt(void *context, const uint8_t key[32], const uint8_t nonce[24],
                                    const uint8_t *aad, size_t aad_length,
                                    const uint8_t *ciphertext, uint8_t *plaintext, size_t length,
                                    const uint8_t tag[16]) {
    provider_state_t *state = context;
    state->decrypt_calls++;
    if (state->decrypt_result != LS_OK) {
        return state->decrypt_result;
    }
    return ls_xchacha20_poly1305_decrypt(key, nonce, aad, aad_length, ciphertext, plaintext, length,
                                         tag);
}

static bool available(void *context) {
    (void)context;
    return true;
}

static size_t mtu(void *context) {
    (void)context;
    return LS_MAX_EVENT_SIZE;
}

static ls_result_t send_data(void *context, const uint8_t *data, size_t length) {
    (void)context;
    if (length > sizeof delivered) {
        return LS_ENOSPACE;
    }
    memcpy(delivered, data, length);
    delivered_length = length;
    return LS_OK;
}

int main(void) {
    static const ls_identity_t identity = {
        .project_id = "provider",
        .device_id = "host",
        .firmware_build_id = "provider1",
    };
    ls_config_t config = {.identity = &identity};
    memset(storage_bytes, 0xff, sizeof storage_bytes);
    ls_memory_storage_t memory = {storage_bytes, sizeof storage_bytes};
    ls_storage_backend_t storage = {
        .name = "ram",
        .context = &memory,
        .capacity = sizeof storage_bytes,
        .read = ls_memory_storage_read,
        .write = ls_memory_storage_write,
        .erase = ls_memory_storage_erase,
    };
    ls_transport_backend_t transport = {
        .name = "sink",
        .priority = 1u,
        .available = available,
        .send = send_data,
        .max_payload = mtu,
    };
    provider_state_t state = {0};
    ls_crypto_provider_t provider = {
        .name = "test-provider",
        .version = "1.0-test",
        .audit_reference = "test fixture only",
        .assurance = LS_CRYPTO_ASSURANCE_EXTERNAL_AUDITED,
        .context = &state,
        .hkdf_sha256 = provider_hkdf,
        .xchacha20_poly1305_encrypt = provider_encrypt,
        .xchacha20_poly1305_decrypt = provider_decrypt,
    };
    uint8_t key[LS_SECURITY_KEY_SIZE];
    memset(key, 0x42, sizeof key);

    CHECK(ls_init(&config) == LS_OK);
    ls_storage_register(&storage);
    ls_transport_register(&transport);
    CHECK(ls_boot() == LS_OK);
    CHECK(strcmp(ls_security_crypto_provider_name(), "builtin-unqualified") == 0);
    CHECK(ls_security_set_crypto_provider(NULL) == LS_EINVAL);
    ls_crypto_provider_t incomplete = provider;
    incomplete.hkdf_sha256 = NULL;
    CHECK(ls_security_set_crypto_provider(&incomplete) == LS_EINVAL);
    CHECK(ls_security_set_crypto_provider(&provider) == LS_OK);
    CHECK(strcmp(ls_security_crypto_provider_name(), "test-provider") == 0);
    CHECK(ls_security_crypto_provider_assurance() == LS_CRYPTO_ASSURANCE_EXTERNAL_AUDITED);
    CHECK(strcmp(ls_security_crypto_provider_audit_reference(), "test fixture only") == 0);
    CHECK(ls_security_crypto_provider_ready());
    state.hkdf_calls = state.encrypt_calls = state.decrypt_calls = 0u;
    CHECK(ls_security_set_key(key, sizeof key) == LS_OK);
    ls_security_set_random_provider(random_bytes, NULL);

    delivered_length = 0u;
    ls_capture_message("provider path", LS_SEVERITY_ERROR);
    CHECK(ls_flush() == LS_OK);
    CHECK(delivered_length > LS_LEP_HEADER_SIZE);
    CHECK(state.hkdf_calls >= 1u);
    CHECK(state.encrypt_calls == 1u);

    uint8_t plaintext[LS_MAX_EVENT_SIZE];
    size_t plaintext_length = 0u;
    CHECK(ls_envelope_decrypt_payload(delivered, delivered_length, plaintext, sizeof plaintext,
                                      &plaintext_length) == LS_OK);
    CHECK(plaintext_length > 0u);
    CHECK(state.decrypt_calls == 1u);

    state.decrypt_result = LS_EIO;
    CHECK(ls_envelope_decrypt_payload(delivered, delivered_length, plaintext, sizeof plaintext,
                                      &plaintext_length) == LS_EIO);
    CHECK(state.decrypt_calls == 2u);

    ls_security_clear_crypto_provider();
    CHECK(strcmp(ls_security_crypto_provider_name(), "builtin-unqualified") == 0);
    state.decrypt_result = LS_OK;
    CHECK(ls_envelope_decrypt_payload(delivered, delivered_length, plaintext, sizeof plaintext,
                                      &plaintext_length) == LS_OK);
    return 0;
}
