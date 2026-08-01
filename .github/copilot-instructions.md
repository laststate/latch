# Latch instructions for GitHub Copilot

The complete, tool-neutral guide is [AGENT.md](../AGENT.md). Apply it together
with the nearest scoped `AGENTS.md` file before proposing or editing code.

- Latch is a heap-free embedded failure-capture runtime. Preserve strict
  separation between normal runtime, ISR, and fault-handler contexts.
- Treat public headers, LEP, retained snapshots, persistent records, and Rust
  FFI types as compatibility contracts. Validate hostile input before access.
- Never weaken crypto, redaction, TLS verification, replay protection, or
  power-loss recovery. Never add secrets or private Relay details to artifacts.
- Add focused tests for behavior and failure paths. Use CMake presets and
  report the checks actually run; do not claim hardware evidence without it.
- Preserve unrelated work, avoid generated build output, use the configured
  human Git identity, and never use an `agent/` branch prefix.
- Read [SECURITY.md](../SECURITY.md), [CONTRIBUTING.md](../CONTRIBUTING.md), and
  the relevant files under `docs/` for security- or protocol-sensitive work.
