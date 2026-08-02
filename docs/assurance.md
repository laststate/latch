# Security, audit and compliance status

Latch contains implementations of XChaCha20-Poly1305, HKDF-SHA-256 and supporting cryptographic plumbing. Test vectors, negative tests, fuzzers, sanitizers and optional interoperability checks provide implementation evidence; they are not an independent cryptographic audit, a side-channel evaluation or a certification.

## Current status

- **Independent cryptographic audit:** not completed or claimed.
- **Side-channel and provisioning review:** not completed or claimed.
- **MISRA C:2012 compliance/certification:** not completed or claimed.
- **External adoption:** a community signal, not a property that source code can manufacture. Public issues, board reports and reproducible qualification evidence are the useful way to grow it.

The public [audit invitation](security-audit.md) defines a review scope and
publication policy. Completed reports will be retained under
[`docs/audits`](audits/README.md); that directory currently records that no
external audit has been completed.

Do not use an unreviewed release as the sole basis for a safety-, security- or mission-critical approval. The product integrator remains responsible for key provisioning, entropy, hardware isolation, transport authentication, retained-data policy and the selected board/toolchain.

## Audit-ready review packet

Before commissioning an independent review, freeze a release tag and provide:

1. The exact source tag, generated SBOM/provenance, compiler versions and reproducible build commands.
2. [The threat model](threat-model.md), provisioning procedure, key lifecycle, nonce ownership, replay policy and any Relay/collector trust boundary.
3. LEP format documentation, C/Rust vectors, malformed inputs and fuzz corpus.
4. The enabled feature profile, Flash/retained-memory layout and board HIL evidence for the product configuration.
5. A review scope that explicitly includes AEAD/HKDF use, error handling, downgrade/replay behavior, redaction, secure storage, side channels and provisioning operations.

Review findings should be tracked publicly when safe to do so, or through the private process in [SECURITY.md](../SECURITY.md) when disclosure would create a risk. A fixed finding should name the affected versions and verification, not silently upgrade the project's claims.

## MISRA path

The project uses strict compiler warnings, tests and format checks, but those are not a substitute for MISRA analysis. A product pursuing MISRA C:2012 needs to define its compliance scope and toolchain, run a qualified checker against the frozen configuration, document every deviation with rationale and owner, and retain the reports with its release evidence. Generated, vendor and test code should be scoped separately from the portable runtime.

That work is deliberately a product/release activity rather than a badge in this repository. It prevents a generic open-source build from implying that a specific compiler, configuration or board has been certified.
