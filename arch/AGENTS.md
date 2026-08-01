# Architecture fault-path guidance

Apply the root [AGENT.md](../AGENT.md) and read
[docs/architecture.md](../docs/architecture.md) and
[docs/concurrency.md](../docs/concurrency.md) before changing this directory.

- Assume this code runs after an exception or trap, with damaged stacks and
  limited register state. Keep it bounded, allocation-free, and independent of
  storage, transport, logging, callbacks, and normal runtime locks.
- Preserve assembly/C ABI, stack alignment, retained snapshot layout, linker
  section, and emergency-stack assumptions. Do not change frame layout or
  offset assumptions without a documented compatibility plan.
- Host tests prove only modeled behavior. Any hardware-specific assertion needs
  exact board, revision, linker script, SDK/toolchain, and fault-scenario
  evidence; otherwise report `not hardware-tested`.
- Include corrupt-frame, invalid-stack, recursion, and recovery behavior in
  review and tests where applicable.
