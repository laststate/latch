/* SPDX-License-Identifier: Apache-2.0 */
/* Latch billing realtime — tier-aware spool caps (header-only).
 *
 * Firmware stays offline-first and never dials billing-service directly.
 * The gateway (relay) and backend (trace) hold the live billing link and
 * push the provisioned tier down at boot / on re-provision. Latch caches
 * that tier in RAM (optionally retained) and sizes its spool caps from it,
 * so over-quota fleets degrade gracefully instead of losing newest evidence.
 *
 * Wiring (gateway side, not on-device):
 *   billing-service --SSE/webhook--> trace/relay --LEP/LSAK--> latch
 *
 * Usage:
 *   #include <laststate/billing.h>
 *   static ls_billing_tier_t g_tier;
 *   ls_billing_apply(&g_tier, LS_BILLING_PLAN_PILOT); // pushed by gateway
 *   if (!ls_billing_admit(&g_tier, pending_events)) { park_or_drop_oldest(); }
 */

#ifndef LASTSTATE_BILLING_H
#define LASTSTATE_BILLING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ls_billing_plan {
    LS_BILLING_PLAN_LOCAL = 0,
    LS_BILLING_PLAN_PILOT = 1,
    LS_BILLING_PLAN_FLEET = 2,
    LS_BILLING_PLAN_ENTERPRISE = 3
} ls_billing_plan_t;

typedef struct ls_billing_tier {
    ls_billing_plan_t plan;
    uint32_t max_buffered_events;
    uint8_t meter_sync_enabled;
} ls_billing_tier_t;

static inline void ls_billing_apply(ls_billing_tier_t *t, ls_billing_plan_t plan) {
    if (t == (void *)0) {
        return;
    }
    t->plan = plan;
    t->meter_sync_enabled = 1;
    switch (plan) {
    case LS_BILLING_PLAN_PILOT:
        t->max_buffered_events = 512U;
        break;
    case LS_BILLING_PLAN_FLEET:
        t->max_buffered_events = 2048U;
        break;
    case LS_BILLING_PLAN_ENTERPRISE:
        t->max_buffered_events = 8192U;
        break;
    case LS_BILLING_PLAN_LOCAL:
    default:
        t->max_buffered_events = 256U;
        break;
    }
}

static inline int ls_billing_admit(const ls_billing_tier_t *t, size_t pending) {
    if (t == (void *)0) {
        return 1;
    }
    return (size_t)t->max_buffered_events == 0 || pending < (size_t)t->max_buffered_events;
}

#ifdef __cplusplus
}
#endif

#endif /* LASTSTATE_BILLING_H */
