# Long-term support policy

Latch does not currently publish an LTS line. The following gates define when
the project may leave `0.x` and call a release LTS; passing ordinary CI alone
is insufficient.

## Eligibility for 1.0 LTS

All of these conditions must be satisfied and linked from the release notes:

1. At least **three physically qualified product configurations** are present
   in the [hardware compatibility matrix](hardware-compatibility.md), spanning
   at least two CPU architectures and two independent flash implementations.
   Every configuration must cover fault entry, power-loss recovery, reset
   classification, spool recovery, transport, and durable ACK.
2. The intended release candidate has completed **90 consecutive days without
   an unresolved critical regression**. A critical regression is data loss,
   unauthenticated acceptance, secret disclosure, unbounded fault-path work,
   memory corruption, or failure to recover a previously qualified scenario.
   The clock restarts when such a regression is confirmed and ends only after
   the fix and regression test are merged.
3. An **independent external security review** of the frozen cryptography,
   secure storage, replay, redaction, and provisioning boundaries is published
   under [`docs/audits`](audits/README.md). Critical/high findings must be
   resolved; accepted lower-severity findings need a maintainer, rationale,
   mitigation, and target release.
4. Release artifacts, SBOM, provenance, package metadata, downgrade behavior,
   migration notes, and every claimed HIL record pass the release checklist.

Until all four gates pass, versions remain `0.x` and are not described as LTS.

## Stable branches and support window

The first LTS release creates `stable/1.x` from its signed release commit.
Future LTS lines use `stable/<major>.x`. Stable branches are protected like
`prod`: pull requests only, required validation, signed release tags, no force
push, no deletion, and at least one approving review.

Each LTS major receives:

- security and critical-correctness fixes for 24 months after its first LTS
  release;
- six months of security-only overlap after the next LTS major;
- patch releases when a supported fix is ready, without waiting for feature
  work on `prod`.

The exact end-of-support date is recorded in every LTS release note. The
project may extend support, but will not shorten a published window.

## Backport policy

Backports are narrow cherry-picks from an already reviewed `prod` commit.

| Change | Stable branch policy |
| --- | --- |
| Critical/high security fix | Required for every affected supported line; coordinate disclosure privately first. |
| Data-loss, corruption, crash-loop, or wire-compatibility fix | Required when the line is affected. |
| Toolchain/build fix | Allowed when it restores a documented supported configuration without changing runtime semantics. |
| New feature, new port, format extension, refactor | Not allowed; ship on `prod` and the next feature release. |

A backport PR must name the source commit, affected versions, compatibility and
power-loss risk, tests rerun on the stable branch, and hardware evidence. If a
clean cherry-pick is impossible, the minimal rewritten fix receives the same
review as a new change. Never merge a conflict resolution that has only been
tested on `prod`.

## Release decision record

An LTS release note must include a checklist for the four eligibility gates,
the 90-day observation start/end dates, qualified matrix rows, audit report,
supported stable branch, and end-of-support date. Missing evidence means the
release is a normal release, not LTS.
