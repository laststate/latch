# AI-assisted change review checklist

Use this checklist before accepting AI-assisted code. It is a review aid, not a
replacement for maintainer judgment or a security audit.

## Intent and scope

- [ ] The change solves the stated problem without unrelated churn.
- [ ] The assistant identified assumptions and did not fabricate evidence.
- [ ] Every touched high-risk surface has an explicit compatibility and risk
      assessment.

## Embedded safety

- [ ] The fault path remains bounded, heap-free, and isolated from normal
      runtime callbacks, storage, transport, and logging.
- [ ] Pointer, length, version, enum, integer arithmetic, and corruption paths
      are validated before use.
- [ ] Persistent changes remain safe across interruption and recovery.
- [ ] Architecture/port changes name the relevant ABI, stack, linker, and SDK
      assumptions.

## Security and privacy

- [ ] No key, credential, customer data, captured memory, private endpoint, or
      proprietary Relay detail was introduced.
- [ ] Encryption, nonce handling, authentication, redaction, replay protection,
      and TLS verification were preserved or deliberately reviewed.
- [ ] A security-sensitive change followed [SECURITY.md](../../SECURITY.md) and
      did not rely on an unverified claim of audit or certification.

## Tests and documentation

- [ ] Tests prove the new behavior and the relevant rejection/failure path.
- [ ] LEP vectors, fuzzers, Rust bindings, and documentation were considered
      when the changed surface requires them.
- [ ] Reported commands actually ran and their result is reproducible.
- [ ] Hardware claims include board, revision, SDK/toolchain, scenario, and
      observed result; otherwise the work says `not hardware-tested`.
- [ ] User-facing behavior has the needed docs and changelog entry.

## Repository hygiene

- [ ] `git diff --check` is clean and the diff contains no generated output.
- [ ] The configured human author identity is used; no `agent/` branch prefix
      or unrequested AI attribution appears.
- [ ] No CI gate, pinned action, dependency constraint, or security check was
      relaxed merely to make the change pass.
