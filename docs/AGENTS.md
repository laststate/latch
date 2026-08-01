# Documentation guidance

Apply the root [AGENT.md](../AGENT.md) first.

- Documentation is a product contract. Derive claims from code, tests, CI, or
  named HIL evidence; do not turn intentions into completed guarantees.
- Keep public docs free of credentials, customer data, proprietary Relay
  implementation details, and unredacted device output.
- Prefer relative links and run `python tools/check_docs.py` after Markdown
  changes. Update user-facing docs and the changelog with behavior changes.
- Describe hardware scope precisely: board, SDK/toolchain, scenario, and what
  remains unverified. Do not call private services public or imply an audit or
  certification.
