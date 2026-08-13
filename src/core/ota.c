#include "internal.h"
#include "laststate/ota.h"
#include "laststate/blackbox.h"

static uint32_t digest_fingerprint(const uint8_t digest[32]) {
    uint32_t value = (uint32_t)digest[0] | ((uint32_t)digest[1] << 8) |
                     ((uint32_t)digest[2] << 16) | ((uint32_t)digest[3] << 24);
    return value ? value : 1u;
}

static bool backend_valid(const ls_ota_backend_t *backend) {
    return backend && backend->authenticate && backend->stage && backend->request_boot &&
           backend->confirm && backend->rollback;
}

ls_result_t ls_ota_stage_candidate(const ls_ota_backend_t *backend, uint32_t version_counter,
                                   uint32_t signing_key_id, const uint8_t *image,
                                   size_t image_length, const uint8_t *signature,
                                   size_t signature_length) {
    if (!backend_valid(backend) || !image || !image_length || !signature || !signature_length ||
        !signing_key_id || !ls_update_version_allowed(version_counter)) {
        return LS_EINVAL;
    }
    uint8_t digest[32];
    ls_sha256(image, image_length, digest);
    ls_result_t result = backend->authenticate(backend->context, version_counter, signing_key_id,
                                               digest, signature, signature_length);
    if (result == LS_OK)
        result = backend->stage(backend->context, image, image_length);
    uint32_t fingerprint = digest_fingerprint(digest);
    if (result == LS_OK)
        result = ls_update_stage(version_counter, fingerprint, signing_key_id);
    if (result == LS_OK) {
        result = backend->request_boot(backend->context, version_counter);
        if (result != LS_OK) {
            /* The inactive image may exist, but it was never selected for boot.
             * Remove the misleading pending state while preserving the
             * anti-rollback floor. */
            (void)ls_update_abort_stage(version_counter);
            (void)ls_blackbox_record_values(LS_BLACKBOX_STATE, 0x0a04u,
                                            LS_BLACKBOX_IMPORTANT | LS_BLACKBOX_ERROR,
                                            (int32_t)version_counter, (int32_t)result, 0, 0);
        }
    }
    if (result == LS_OK) {
        /* Only mark the previous firmware as released when we have an
         * identity. A NULL firmware_version would hash to zero and
         * produce a misleading release record. */
        if (ls_runtime.config.identity) {
            ls_release_mark_pending(ls_runtime.config.identity->firmware_version);
        }
        ls_reset_mark_expected(true);
        (void)ls_blackbox_record_values(LS_BLACKBOX_STATE, 0x0a01u, LS_BLACKBOX_IMPORTANT,
                                        (int32_t)version_counter, (int32_t)signing_key_id,
                                        (int32_t)fingerprint, 0);
    }
    ls_secure_zero(digest, sizeof(digest));
    return result;
}

ls_result_t ls_ota_confirm_running(const ls_ota_backend_t *backend, uint32_t version_counter) {
    if (!backend_valid(backend) || !version_counter)
        return LS_EINVAL;
    ls_result_t result = backend->confirm(backend->context, version_counter);
    if (result == LS_OK)
        result = ls_update_confirm_version(version_counter);
    if (result == LS_OK) {
        ls_release_confirm();
        ls_reset_mark_expected(false);
        (void)ls_blackbox_record_values(LS_BLACKBOX_STATE, 0x0a02u, LS_BLACKBOX_IMPORTANT,
                                        (int32_t)version_counter, 0, 0, 0);
    }
    return result;
}

ls_result_t ls_ota_request_rollback(const ls_ota_backend_t *backend, uint32_t failed_version) {
    if (!backend_valid(backend) || !failed_version)
        return LS_EINVAL;
    ls_result_t result = backend->rollback(backend->context, failed_version);
    if (result == LS_OK)
        result = ls_update_mark_version_rollback(failed_version);
    if (result == LS_OK) {
        ls_release_mark_rollback();
        ls_reset_mark_expected(true);
        (void)ls_blackbox_record_values(LS_BLACKBOX_STATE, 0x0a03u,
                                        LS_BLACKBOX_IMPORTANT | LS_BLACKBOX_ERROR,
                                        (int32_t)failed_version, 0, 0, 0);
    }
    return result;
}
