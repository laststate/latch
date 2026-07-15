#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "secure element failed: %s:%d\n", #x, __LINE__);                       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
typedef struct {
    uint8_t counter;
} mock_t;
static ls_result_t random_bytes(void *context, uint8_t *output, size_t length) {
    mock_t *mock = (mock_t *)context;
    for (size_t i = 0; i < length; i++)
        output[i] = mock->counter++;
    return LS_OK;
}
static ls_result_t sign(void *context, const uint8_t digest[32], uint8_t signature[64]) {
    (void)context;
    memcpy(signature, digest, 32);
    memcpy(signature + 32, digest, 32);
    return LS_OK;
}
static ls_result_t certificate(void *context, uint8_t *output, size_t capacity, size_t *length) {
    (void)context;
    static const uint8_t cert[] = {0x30, 0x03, 0x02, 0x01, 0x01};
    if (capacity < sizeof cert)
        return LS_ENOSPACE;
    memcpy(output, cert, sizeof cert);
    *length = sizeof cert;
    return LS_OK;
}
static ls_result_t derive(void *context, uint32_t key_id, const uint8_t *data, size_t length,
                          uint8_t output[32]) {
    (void)context;
    for (unsigned i = 0; i < 32; i++)
        output[i] = (uint8_t)(key_id + i + (length ? data[i % length] : 0));
    return LS_OK;
}
int main(void) {
    mock_t mock = {0};
    ls_secure_element_t element = {&mock, random_bytes, sign, certificate, derive};
    CHECK(ls_secure_element_validate(&element) == LS_OK);
    uint8_t random[40], digest[32] = {0}, signature[64], cert[16], key[32];
    size_t cert_length = 0;
    CHECK(ls_secure_element_random(&element, random, sizeof random) == LS_OK);
    CHECK(random[39] == 39);
    CHECK(ls_secure_element_sign(&element, digest, signature) == LS_OK);
    CHECK(ls_secure_element_certificate(&element, cert, sizeof cert, &cert_length) == LS_OK);
    CHECK(cert_length == 5);
    CHECK(ls_secure_element_derive(&element, 4, (const uint8_t *)"ctx", 3, key) == LS_OK);
    CHECK(key[0] == (uint8_t)(4 + 'c'));
    CHECK(ls_secure_element_random(&element, random, sizeof random) == LS_OK);
    return 0;
}
