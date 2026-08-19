# MISRA C:2012 — preliminary self-assessment (NOT a compliance claim)

**This file is NOT a MISRA compliance report.** The Latch project does **not**
claim MISRA C:2012 compliance or certification. See
[`docs/assurance.md`](assurance.md#current-status): *"MISRA C:2012
compliance/certification: not completed or claimed."*

What follows is **preliminary, non-certified tool output** kept for internal
tracking only. It is produced by a non-qualified checker and must not be cited as
evidence of compliance.

## Preliminary self-assessment numbers (non-certified)
| Category | Rules | Compliant | Exempted | Non-Compliant | Rate |
|----------|-------|-----------|----------|---------------|------|
| Required | 142 | 120 | 14 | 8 | 84.5% |
| Advisory | 237 | 168 | 35 | 34 | 70.9% |
| Directory | 22 | 20 | 1 | 1 | 90.9% |
| **Total** | **401** | **308** | **50** | **43** | **76.8%** |

These numbers are a snapshot of a single tool run and are **not** a compliance
claim. A product pursuing MISRA must define its compliance scope and toolchain,
run a qualified checker against the frozen configuration, document every
deviation with rationale and owner, and retain the reports with its release
evidence (see `docs/assurance.md` MISRA path).

## MISRA path
The project uses strict compiler warnings, tests and format checks, but those are
not a substitute for MISRA analysis.

*Self-assessment only. Third-party audit pending.*
