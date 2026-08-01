# Portable runtime guidance

This directory contains the portable C runtime. Apply the root
[AGENT.md](../AGENT.md) first.

- Preserve freestanding, heap-free C11 behavior. Do not introduce allocation,
  hosted-only APIs, unbounded recursion, or implicit global initialization.
- Keep fault capture separate from normal runtime behavior. The fault path may
  write only the bounded retained snapshot; promotion, storage, transport, and
  callbacks belong to normal boot/runtime code.
- Validate every external pointer, length, enum, version, and arithmetic
  relationship before dereference or size calculation. Avoid overflow-prone
  `a + b` comparisons when subtraction after a prior lower-bound check is
  clearer.
- Treat `src/core/internal.h` as internal. Public behavior belongs in headers
  under `include/laststate/` and requires compatibility review.
- Pair behavioral changes with a test under `tests/`. For LEP, stream, crypto,
  compression, storage, or spool changes, update malformed-input coverage and
  fuzz/vector material when relevant.
