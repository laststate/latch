# Test guidance

Apply the root [AGENT.md](../AGENT.md) first.

- Tests must be deterministic, self-contained, bounded, and free of device
  credentials, production endpoints, private Relay data, and captured memory.
- Add regression coverage for the behavior and its error path. For parsers,
  cover truncation, malformed lengths, unsupported versions, and corruption as
  appropriate.
- Preserve C/Rust LEP-vector compatibility. Update vectors only when the
  protocol change is intentional, versioned, and documented.
- Use the existing simple `CHECK` style and static test storage where possible.
  Do not hide a product bug with permissive test expectations.
- If a test relies on a hardware assumption, move that evidence to `hil/` or
  explicitly label it as a host model rather than a physical result.
