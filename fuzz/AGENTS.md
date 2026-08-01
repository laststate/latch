# Fuzzing guidance

Apply the root [AGENT.md](../AGENT.md) first.

- Fuzz targets must accept untrusted bytes without assumptions about alignment,
  termination, length, or structure.
- Keep harnesses deterministic and bounded. Do not use networks, devices,
  secrets, or production captures in corpora.
- When a parser or crypto boundary changes, inspect the matching harness and
  seed corpus. Add a minimal, sanitized regression seed only when it increases
  durable coverage.
- Reproduce a finding with the smallest deterministic input and add a normal
  regression test when practical; do not merely suppress a sanitizer finding.
