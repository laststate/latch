#ifndef LASTSTATE_SECURITY_H
#define LASTSTATE_SECURITY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "event.h"
#define LS_HMAC_SHA256_SIZE 32u
#define LS_SECURITY_KEY_SIZE 32u
#define LS_AEAD_TAG_SIZE 16u
#define LS_CHACHA20_NONCE_SIZE 12u
#define LS_XCHACHA20_NONCE_SIZE 24u
#define LS_HKDF_MAX_INFO_SIZE 128u
typedef ls_result_t (*ls_crypto_random_fn)(void *context, uint8_t *output, size_t length);
typedef enum {
    LS_SECURITY_HMAC_SHA256 = 1,
    LS_SECURITY_XCHACHA20_POLY1305 = 2
} ls_security_algorithm_t;
typedef struct {
    ls_security_algorithm_t algorithm;
    uint32_t key_id;
    bool reject_plaintext;
    bool allow_legacy_hmac;
} ls_security_policy_t;
ls_result_t ls_security_set_key(const uint8_t *key, size_t length);
void ls_security_clear_key(void);
bool ls_security_enabled(void);
ls_result_t ls_security_set_policy(const ls_security_policy_t *policy);
ls_security_policy_t ls_security_get_policy(void);
void ls_security_set_random_provider(ls_crypto_random_fn random, void *context);
ls_result_t ls_security_random(uint8_t *output, size_t length);
void ls_secure_zero(void *data, size_t length);
bool ls_constant_time_equal(const uint8_t *left, const uint8_t *right, size_t length);
void ls_hmac_sha256(const uint8_t *key, size_t key_length, const uint8_t *data, size_t length,
                    uint8_t output[LS_HMAC_SHA256_SIZE]);
ls_result_t ls_hkdf_sha256(const uint8_t *salt, size_t salt_length, const uint8_t *ikm,
                           size_t ikm_length, const uint8_t *info, size_t info_length,
                           uint8_t *output, size_t output_length);
void ls_poly1305_auth(const uint8_t key[32], const uint8_t *message, size_t length,
                      uint8_t tag[LS_AEAD_TAG_SIZE]);
ls_result_t ls_chacha20_poly1305_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                                         const uint8_t *aad, size_t aad_length,
                                         const uint8_t *plaintext, uint8_t *ciphertext,
                                         size_t length, uint8_t tag[16]);
ls_result_t ls_chacha20_poly1305_decrypt(const uint8_t key[32], const uint8_t nonce[12],
                                         const uint8_t *aad, size_t aad_length,
                                         const uint8_t *ciphertext, uint8_t *plaintext,
                                         size_t length, const uint8_t tag[16]);
ls_result_t ls_xchacha20_poly1305_encrypt(const uint8_t key[32], const uint8_t nonce[24],
                                          const uint8_t *aad, size_t aad_length,
                                          const uint8_t *plaintext, uint8_t *ciphertext,
                                          size_t length, uint8_t tag[16]);
ls_result_t ls_xchacha20_poly1305_decrypt(const uint8_t key[32], const uint8_t nonce[24],
                                          const uint8_t *aad, size_t aad_length,
                                          const uint8_t *ciphertext, uint8_t *plaintext,
                                          size_t length, const uint8_t tag[16]);
ls_result_t ls_envelope_verify_auth(const uint8_t *data, size_t length);
bool ls_replay_accept(uint32_t sequence);
void ls_chacha20_xor(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter,
                     const uint8_t *input, uint8_t *output, size_t length);
void ls_hchacha20(const uint8_t key[32], const uint8_t nonce[16], uint8_t subkey[32]);
#endif
