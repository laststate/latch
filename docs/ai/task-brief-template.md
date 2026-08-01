# AI task brief template

Use this template when delegating a substantial Latch task to an AI assistant.
It creates enough context to avoid speculative changes while keeping the work
focused.

```text
Goal
- What outcome should exist when this task is complete?

Scope
- Files/directories that are in scope:
- Explicitly out of scope:
- Is an API, LEP, retained-snapshot, storage-layout, or Rust FFI change allowed?

Evidence and constraints
- Relevant issue, design decision, vector, board report, or test failure:
- Security, compatibility, memory, timing, or power-loss constraints:
- Hardware available and explicitly authorized, if any:

Expected validation
- Required host tests/checks:
- Required cross-toolchain, fuzz, static, or Rust checks:
- Required HIL evidence, or `not hardware-tested`:

Delivery
- Expected files/behavior:
- Whether a changelog/docs update is needed:
- Whether committing, pushing, opening a PR, merging, publishing, or external
  communication is authorized:
```

## Example

```text
Goal
- Reject stream frames whose encoded payload length does not match the frame.

Scope
- src/transport/stream.c and tests/test_transport.c only.
- Do not change the public stream framing or LEP version.

Evidence and constraints
- A malformed frame can advertise a payload length inconsistent with the bytes.
- Preserve bounded parsing and existing error-code behavior.
- No hardware operations are authorized.

Expected validation
- host-debug C/C++ suite, relevant fuzz target if affected, and docs checker if
  documentation changes.

Delivery
- Focused regression test and a short evidence summary.
- Commit and update the existing PR are authorized; release and merge are not.
```

The brief does not replace [AGENT.md](../../AGENT.md). If the two conflict,
follow the safety, security, and compatibility rules in the agent guide and ask
for clarification.
