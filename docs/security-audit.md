# Independent security review

Latch welcomes independent review of its portable cryptography and evidence
pipeline. The project has **not yet completed an external audit**; this page is
an invitation and a ready-to-use scope, not a security badge.

## Proposed review scope

- XChaCha20-Poly1305, ChaCha20-Poly1305, Poly1305, SHA-256 and HKDF
  implementation and known-answer/interoperability evidence;
- nonce generation and reuse handling, key identifiers, rotation, memory
  wiping and replay windows;
- authenticated secure storage across partial writes and power loss;
- LEP parsing, encrypted-envelope downgrade handling and malformed inputs;
- memory-region selection, zero/hash/exclude redaction and coredump boundaries;
- fault-context constraints and any route by which key material could reach a
  retained snapshot, envelope, log, or transport callback;
- documented product responsibilities for entropy, provisioning, TLS and
  secure elements.

The audit target must be a signed tag or immutable commit. The review packet is
the SBOM/provenance plus the threat model, wire format, security/storage docs,
fuzz corpus, crypto tests, HIL matrix, and exact enabled build profile.

## How to participate

Organizations or experienced reviewers can open a public discussion that
contains no vulnerability details, or contact the maintainer through the
private reporting route in [SECURITY.md](../SECURITY.md). A review may be paid,
donated, or community-organized; independence and disclosed conflicts of
interest matter more than the funding model.

Suspected vulnerabilities must remain private until coordinated disclosure.
General methodology, coverage gaps, and a proposal to audit are safe to discuss
publicly.

## Publication policy

Completed reports live in [`docs/audits`](audits/README.md) and identify the
exact reviewed revision, reviewer, dates, scope, exclusions, methodology, and
finding disposition. Reports may redact exploit details for unresolved issues,
but must still state severity, affected versions, current status, and why the
redaction is necessary. The original reviewer report is preserved alongside a
maintainer response; findings are never silently removed.

Critical and high findings block an LTS release. A report does not make every
hardware integration, product key ceremony, or vendor backend audited unless
those items were explicitly in scope.
