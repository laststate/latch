#include "internal.h"
#include "laststate/fingerprint.h"

static uint32_t mix(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

uint32_t ls_event_fingerprint(const char *domain, int32_t code, uint32_t detail) {
    uint32_t value = ls_hash_string(domain) ^ mix((uint32_t)code) ^ mix(detail) ^
                     mix(ls_hash_string(ls_build_id()));
    value = mix(value);
    return value ? value : 1u;
}

uint32_t ls_crash_fingerprint(const ls_arch_context_t *context) {
    if (!context) {
        return 0u;
    }
    uint32_t value = mix((uint32_t)context->architecture) ^ mix((uint32_t)context->fault) ^
                     mix(context->pc) ^ mix(context->lr) ^ mix((uint32_t)context->fault_address) ^
                     mix(context->cfsr) ^ mix(context->hfsr) ^ mix(context->mcause) ^
                     mix(context->mtval) ^ mix(context->exccause) ^ mix(context->excvaddr) ^
                     mix(ls_hash_string(ls_build_id()));
    value = mix(value);
    return value ? value : 1u;
}
