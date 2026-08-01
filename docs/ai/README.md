# AI-assisted development

Latch welcomes AI-assisted contributions when they are reviewable, evidenced,
and held to the same embedded-systems bar as any other contribution. This
directory is deliberately tool-neutral: it complements the root
[AGENT.md](../../AGENT.md), which is the canonical instruction set for Codex,
Claude, Copilot, Gemini, Cursor, and similar tools.

## Use this material

1. Give an assistant a focused task and the relevant file paths.
2. Ask it to read [AGENT.md](../../AGENT.md) and any scoped `AGENTS.md` files
   before it edits.
3. Use the [task brief template](task-brief-template.md) for work that has
   compatibility, security, storage, protocol, or hardware implications.
4. Consult the [invariants](invariants.md) and
   [hardware/external-operation policy](hardware-and-external-operations.md)
   before work on a high-risk surface.
5. Ask for the evidence required by the
   [validation guide](validation.md), then review the result with the
   [review checklist](review-checklist.md).

The assistant may accelerate investigation and implementation, but it does not
turn unverified work into production evidence. A maintainer remains responsible
for design decisions, approval, hardware qualification, release, and disclosure.

## Tool entry points

| Tool family | Repository entry point |
| --- | --- |
| Codex and agents that recognize `AGENTS.md` | [`AGENTS.md`](../../AGENTS.md) |
| Tools that look for `AGENT.md` | [`AGENT.md`](../../AGENT.md) |
| Claude Code | [`CLAUDE.md`](../../CLAUDE.md) |
| Gemini CLI | [`GEMINI.md`](../../GEMINI.md) |
| GitHub Copilot | [`.github/copilot-instructions.md`](../../.github/copilot-instructions.md) |

All entry points route to the same rules. Do not maintain separate, divergent
instructions for individual vendors.

## What good AI assistance looks like

- It reads before editing and distinguishes facts from assumptions.
- It protects the fault-path, protocol, persistence, and cryptographic
  invariants instead of optimizing only for a passing build.
- It writes focused tests that prove both acceptance and rejection paths.
- It reports precise commands and results, including what was not tested.
- It asks before external, destructive, release, or hardware operations.

## What it must not do

- Invent HIL, security-audit, certification, benchmark, or compatibility
  evidence.
- Copy private Relay behavior, credentials, keys, captured memory, or customer
  data into the public repository.
- Disable a check, lower a gate, weaken crypto, or make a parser permissive to
  obtain a green result.
- Rewrite unrelated code or discard a dirty worktree to make a task easier.

For human contribution and disclosure policy, see
[CONTRIBUTING.md](../../CONTRIBUTING.md) and [SECURITY.md](../../SECURITY.md).
