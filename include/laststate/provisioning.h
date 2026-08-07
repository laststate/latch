#ifndef LASTSTATE_PROVISIONING_H
#define LASTSTATE_PROVISIONING_H

#include <stdint.h>

#include "event.h"
#include "secure_element.h"

typedef enum {
    LS_PROVISIONING_UNPROVISIONED = 0,
    LS_PROVISIONING_ACTIVE = 1,
    LS_PROVISIONING_ROTATING = 2,
    LS_PROVISIONING_REVOKED = 3,
    LS_PROVISIONING_DECOMMISSIONED = 4,
    LS_PROVISIONING_DECOMMISSIONING = 5
} ls_provisioning_state_code_t;

typedef struct {
    ls_provisioning_state_code_t state;
    uint32_t key_id;
    uint32_t pending_key_id;
    uint32_t generation;
    uint32_t monotonic_counter;
} ls_provisioning_state_t;

ls_provisioning_state_t ls_provisioning_get_state(void);
ls_result_t ls_provisioning_activate(uint32_t key_id, uint32_t monotonic_counter);
ls_result_t ls_provisioning_begin_rotation(uint32_t new_key_id, uint32_t monotonic_counter);
ls_result_t ls_provisioning_commit_rotation(void);
ls_result_t ls_provisioning_revoke(void);
ls_result_t ls_provisioning_decommission(void);
/* Decommission and ask an external secure element to destroy the active key.
 * Metadata is only marked decommissioned if external destruction succeeds. */
ls_result_t ls_provisioning_decommission_secure(const ls_secure_element_t *element);
/* Creates a signed device/build challenge using an external secure element.
 * The challenge and identity are hashed before invoking sign_sha256. */
ls_result_t ls_provisioning_attest(const ls_secure_element_t *element, const uint8_t *challenge,
                                   size_t challenge_length, uint8_t digest[32],
                                   uint8_t signature[64]);

#endif
