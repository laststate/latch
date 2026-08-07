#include "internal.h"
#include "laststate/provisioning.h"

static bool monotonic_allowed(uint32_t counter) {
    return counter != 0u && counter > ls_runtime.persistent.provision_monotonic_counter;
}

ls_provisioning_state_t ls_provisioning_get_state(void) {
    return (ls_provisioning_state_t){
        .state = (ls_provisioning_state_code_t)ls_runtime.persistent.provision_state,
        .key_id = ls_runtime.persistent.provision_key_id,
        .pending_key_id = ls_runtime.persistent.provision_pending_key_id,
        .generation = ls_runtime.persistent.provision_generation,
        .monotonic_counter = ls_runtime.persistent.provision_monotonic_counter,
    };
}

ls_result_t ls_provisioning_activate(uint32_t key_id, uint32_t monotonic_counter) {
    if (key_id == 0u || !monotonic_allowed(monotonic_counter) ||
        (ls_runtime.persistent.provision_state != LS_PROVISIONING_UNPROVISIONED &&
         ls_runtime.persistent.provision_state != LS_PROVISIONING_REVOKED)) {
        return LS_EINVAL;
    }
    ls_runtime.persistent.provision_state = LS_PROVISIONING_ACTIVE;
    ls_runtime.persistent.provision_key_id = key_id;
    ls_runtime.persistent.provision_pending_key_id = 0u;
    ls_runtime.persistent.provision_monotonic_counter = monotonic_counter;
    if (ls_runtime.persistent.provision_generation != UINT32_MAX) {
        ls_runtime.persistent.provision_generation++;
    }
    return ls_boot_state_save();
}

ls_result_t ls_provisioning_begin_rotation(uint32_t new_key_id, uint32_t monotonic_counter) {
    if (ls_runtime.persistent.provision_state != LS_PROVISIONING_ACTIVE || new_key_id == 0u ||
        new_key_id == ls_runtime.persistent.provision_key_id || !monotonic_allowed(monotonic_counter)) {
        return LS_EINVAL;
    }
    ls_runtime.persistent.provision_state = LS_PROVISIONING_ROTATING;
    ls_runtime.persistent.provision_pending_key_id = new_key_id;
    ls_runtime.persistent.provision_monotonic_counter = monotonic_counter;
    return ls_boot_state_save();
}

ls_result_t ls_provisioning_commit_rotation(void) {
    if (ls_runtime.persistent.provision_state != LS_PROVISIONING_ROTATING ||
        ls_runtime.persistent.provision_pending_key_id == 0u) {
        return LS_EINVAL;
    }
    ls_runtime.persistent.provision_key_id = ls_runtime.persistent.provision_pending_key_id;
    ls_runtime.persistent.provision_pending_key_id = 0u;
    ls_runtime.persistent.provision_state = LS_PROVISIONING_ACTIVE;
    if (ls_runtime.persistent.provision_generation != UINT32_MAX) {
        ls_runtime.persistent.provision_generation++;
    }
    return ls_boot_state_save();
}

ls_result_t ls_provisioning_revoke(void) {
    if (ls_runtime.persistent.provision_state != LS_PROVISIONING_ACTIVE &&
        ls_runtime.persistent.provision_state != LS_PROVISIONING_ROTATING) {
        return LS_EINVAL;
    }
    ls_runtime.persistent.provision_state = LS_PROVISIONING_REVOKED;
    ls_runtime.persistent.provision_pending_key_id = 0u;
    ls_security_clear_key();
    return ls_boot_state_save();
}

ls_result_t ls_provisioning_decommission(void) {
    if (ls_runtime.persistent.provision_state == LS_PROVISIONING_DECOMMISSIONED) {
        return LS_OK;
    }
    ls_security_clear_key();
    ls_runtime.persistent.provision_state = LS_PROVISIONING_DECOMMISSIONED;
    ls_runtime.persistent.provision_key_id = 0u;
    ls_runtime.persistent.provision_pending_key_id = 0u;
    if (ls_runtime.persistent.provision_generation != UINT32_MAX) {
        ls_runtime.persistent.provision_generation++;
    }
    return ls_boot_state_save();
}

ls_result_t ls_provisioning_decommission_secure(const ls_secure_element_t *element) {
    uint32_t key_id = ls_runtime.persistent.provision_key_id;
    if (ls_runtime.persistent.provision_state == LS_PROVISIONING_DECOMMISSIONED) {
        return LS_OK;
    }
    if (!element || !key_id ||
        (ls_runtime.persistent.provision_state != LS_PROVISIONING_ACTIVE &&
         ls_runtime.persistent.provision_state != LS_PROVISIONING_REVOKED &&
         ls_runtime.persistent.provision_state != LS_PROVISIONING_DECOMMISSIONING)) {
        return LS_EINVAL;
    }
    uint8_t previous = ls_runtime.persistent.provision_state;
    if (previous != LS_PROVISIONING_DECOMMISSIONING) {
        ls_runtime.persistent.provision_state = LS_PROVISIONING_DECOMMISSIONING;
        ls_result_t persisted = ls_boot_state_save();
        if (persisted != LS_OK) {
            ls_runtime.persistent.provision_state = previous;
            return persisted;
        }
    }
    ls_result_t result = ls_secure_element_destroy_key(element, key_id);
    if (result != LS_OK) {
        /* Leave DECOMMISSIONING durable: fail closed and allow a later retry. */
        ls_security_clear_key();
        return result;
    }
    return ls_provisioning_decommission();
}

ls_result_t ls_provisioning_attest(const ls_secure_element_t *element, const uint8_t *challenge,
                                   size_t challenge_length, uint8_t digest[32],
                                   uint8_t signature[64]) {
    if (!element || (!challenge && challenge_length) || !digest || !signature ||
        challenge_length > 128u) {
        return LS_EINVAL;
    }
    uint8_t material[256];
    size_t length = 0u;
    const char *device = ls_runtime.config.identity ? ls_runtime.config.identity->device_id : 0;
    const char *build = ls_runtime.config.identity && ls_runtime.config.identity->firmware_build_id
                            ? ls_runtime.config.identity->firmware_build_id
                            : ls_build_id();
    size_t device_length = ls_string_length(device);
    size_t build_length = ls_string_length(build);
    if (challenge_length + device_length + build_length + 12u > sizeof(material)) {
        return LS_ENOSPACE;
    }
    if (challenge_length) {
        ls_memcpy(material + length, challenge, challenge_length);
        length += challenge_length;
    }
    ls_memcpy(material + length, device, device_length);
    length += device_length;
    ls_memcpy(material + length, build, build_length);
    length += build_length;
    uint32_t fields[3] = {ls_runtime.persistent.provision_generation,
                          ls_runtime.persistent.provision_monotonic_counter, ls_boot_count()};
    ls_memcpy(material + length, fields, sizeof(fields));
    length += sizeof(fields);
    ls_sha256(material, length, digest);
    ls_secure_zero(material, sizeof(material));
    return ls_secure_element_sign(element, digest, signature);
}
