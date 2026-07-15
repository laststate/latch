#include "internal.h"
static ls_policy_record_t *find_policy(uint32_t hash, bool create) {
    for (size_t i = 0; i < ls_runtime.policy_count; i++)
        if (ls_runtime.policies[i].domain_hash == hash)
            return &ls_runtime.policies[i];
    if (!create || ls_runtime.policy_count >= LS_POLICY_CAPACITY)
        return 0;
    ls_policy_record_t *policy = &ls_runtime.policies[ls_runtime.policy_count++];
    ls_memset(policy, 0, sizeof *policy);
    policy->domain_hash = hash;
    policy->sampling_permyriad = 10000u;
    return policy;
}
ls_result_t ls_sampling_set_permyriad(const char *domain, uint16_t permyriad) {
    if (!domain || permyriad > 10000u)
        return LS_EINVAL;
    ls_policy_record_t *policy = find_policy(ls_hash_string(domain), true);
    if (!policy)
        return LS_ENOSPACE;
    policy->sampling_permyriad = permyriad;
    return LS_OK;
}
ls_result_t ls_sampling_set(const char *domain, float probability) {
    /* Convert IEEE-754 binary32 without floating-point arithmetic so the
       freestanding runtime never acquires a soft-float/CRT dependency. */
    union {
        float value;
        uint32_t bits;
    } representation = {probability};
    uint32_t magnitude = representation.bits & 0x7fffffffu;
    if ((representation.bits >> 31u && magnitude) || magnitude > 0x3f800000u)
        return LS_EINVAL;
    if (!magnitude)
        return ls_sampling_set_permyriad(domain, 0);
    uint32_t exponent = magnitude >> 23u, mantissa = magnitude & 0x7fffffu;
    if (!exponent)
        return ls_sampling_set_permyriad(domain, 0);
    mantissa |= 0x800000u;
    unsigned shift = 150u - exponent;
    uint64_t scaled = (uint64_t)mantissa * 10000u;
    uint32_t permyriad =
        shift >= 64u ? 0u : (uint32_t)((scaled + ((uint64_t)1u << (shift - 1u))) >> shift);
    return ls_sampling_set_permyriad(domain, (uint16_t)permyriad);
}
ls_result_t ls_rate_limit_set(const char *domain, uint16_t maximum, ls_rate_period_t period) {
    if (!domain || !maximum || !period)
        return LS_EINVAL;
    ls_policy_record_t *policy = find_policy(ls_hash_string(domain), true);
    if (!policy)
        return LS_ENOSPACE;
    policy->rate_maximum = maximum;
    policy->rate_period_ms = (uint32_t)period;
    policy->rate_count = 0;
    policy->window_started_ms = ls_uptime_ms();
    return LS_OK;
}
void ls_policy_reset(void) {
    ls_runtime.policy_count = 0;
    ls_runtime.dedup_count = 0;
    ls_runtime.dedup_next = 0;
}
size_t ls_dedup_count(void) {
    return ls_runtime.dedup_count;
}
ls_result_t ls_dedup_get(size_t index, ls_dedup_snapshot_t *snapshot) {
    if (index >= ls_runtime.dedup_count || !snapshot)
        return LS_EINVAL;
    ls_dedup_record_t *record = &ls_runtime.dedup[index];
    snapshot->fingerprint = record->fingerprint;
    snapshot->count = record->count;
    snapshot->first_seen_ms = record->first_seen_ms;
    snapshot->last_seen_ms = record->last_seen_ms;
    return LS_OK;
}
static uint32_t random_next(void) {
    uint32_t value = ls_runtime.random_state;
    if (!value)
        value = 0xa341316cu;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    ls_runtime.random_state = value;
    return value;
}
static bool is_power_of_two(uint32_t value) {
    return value && !(value & (value - 1u));
}
static ls_dedup_record_t *dedup_record(uint32_t fingerprint, uint32_t now) {
    for (size_t i = 0; i < ls_runtime.dedup_count; i++)
        if (ls_runtime.dedup[i].fingerprint == fingerprint)
            return &ls_runtime.dedup[i];
    size_t index;
    if (ls_runtime.dedup_count < LS_DEDUP_CAPACITY)
        index = ls_runtime.dedup_count++;
    else {
        index = ls_runtime.dedup_next;
        ls_runtime.dedup_next = (ls_runtime.dedup_next + 1u) % LS_DEDUP_CAPACITY;
    }
    ls_dedup_record_t *record = &ls_runtime.dedup[index];
    *record = (ls_dedup_record_t){fingerprint, 0, now, now};
    return record;
}
bool ls_policy_apply(ls_event_t *event) {
    if (!event)
        return false;
    uint32_t now = event->timestamp_ms;
    if (!event->fingerprint)
        event->fingerprint = ls_hash_string(event->domain) ^ ((uint32_t)event->code * 0x9e3779b9u) ^
                             ls_hash_string(event->message);
    ls_dedup_record_t *dedup = dedup_record(event->fingerprint, now);
    if (now - dedup->last_seen_ms > LS_DEDUP_WINDOW_MS) {
        dedup->count = 0;
        dedup->first_seen_ms = now;
    }
    dedup->count++;
    dedup->last_seen_ms = now;
    event->repeat_count = dedup->count;
    event->first_seen_ms = dedup->first_seen_ms;
    event->last_seen_ms = now;
    if (event->priority <= LS_PRIORITY_CRITICAL)
        return true;
    ls_policy_record_t *policy = find_policy(ls_hash_string(event->domain), false);
    if (policy) {
        policy->seen++;
        if (policy->rate_maximum) {
            if (now - policy->window_started_ms >= policy->rate_period_ms) {
                policy->window_started_ms = now;
                policy->rate_count = 0;
            }
            if (policy->rate_count >= policy->rate_maximum)
                return false;
            policy->rate_count++;
        }
        if (policy->seen > 1u && policy->sampling_permyriad < 10000u &&
            random_next() % 10000u >= policy->sampling_permyriad)
            return false;
    }
    return dedup->count == 1u || is_power_of_two(dedup->count);
}
