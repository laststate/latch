# Hardware-in-the-loop guidance

Apply the root [AGENT.md](../AGENT.md) and read
[docs/hil.md](../docs/hil.md) plus the relevant fixture before working in this
directory.

- HIL runs can flash devices, force resets, corrupt storage, and transmit over
  a network. Run them only with explicit authorization for the exact board and
  scenario.
- Do not erase, flash, reset, or reconfigure an unidentified board. Confirm
  the port, board identity, and any data-loss impact first.
- Record the exact board/revision, SDK/toolchain, firmware commit, command,
  scenario, raw observation, and decoded result. Redact secrets and endpoints.
- A successful host or simulator test is not HIL evidence; a one-board result
  is not qualification for another board, linker script, or SDK version.
