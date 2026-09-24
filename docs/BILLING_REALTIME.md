# Latch ↔ billing realtime (via gateway)

Latch firmware never contacts billing-service directly — devices stay
offline-first on constrained links. The live link terminates at the gateway
and backend, which push the provisioned tier down:

```text
billing-service --SSE/webhook--> trace + relay --LEP/LSAK--> latch
```

## On-device contract (`include/laststate/billing.h`, header-only)

- `ls_billing_apply(&tier, plan)` — called at boot and on re-provision with
  the tier pushed by relay (`local|pilot|fleet|enterprise`).
- `ls_billing_admit(&tier, pending)` — gate before buffering: returns 0 when
  the tier spool cap is reached so the app can park or drop oldest instead of
  losing newest evidence.
- Caps: local 256, pilot 512, fleet 2048, enterprise 8192 buffered events.
  Metering itself (crash counts → `POST billing-service /v1/usage`) happens in
  relay/trace, never on-device.

## Gateway duties

- Relay caches `GET billing-service /v1/entitlements/{org}` (5 min, fail-open)
  and reports `POST /v1/usage` every 60s.
- Trace applies `POST /v1/admin/organizations/{id}/entitlements` and enforces
  quotas in middleware.
- Power-fail seal, crypto evidence pack and spool durability are unchanged —
  billing only sizes caps, it never weakens persistence.
