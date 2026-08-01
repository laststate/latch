# Hardware and external-operation policy

AI assistance must distinguish local analysis from actions that change a device,
repository, account, or third-party system. The action is allowed only when the
user has explicitly authorized its exact target and scope.

| Operation | Treat as state-changing | Required evidence before action |
| --- | --- | --- |
| Build, unit test, static analysis, docs check | No, unless a command writes outside the repository or uses a shared service. | Confirm the working tree and build directory are safe to use. |
| `idf.py flash`, PlatformIO upload/erase, serial reset | Yes; may erase Flash or disrupt a connected device. | Board identity, port, expected data-loss impact, exact command, and user authorization. |
| HIL runner, brownout, watchdog, MPU, fault injection | Yes; intentionally destructive. | Board config, scenario, recovery plan, authorization, and [HIL procedure](../hil.md). |
| Network post, collector test, cloud API, webhook | Yes; may transmit data or create records. | Endpoint is approved, payload is sanitized, and authorization covers the service. |
| Push, PR creation, merge, branch protection, issue/comment | Yes; changes public/shared collaboration state. | Confirm the repository, branch, target, message, and user authorization. |
| Package publish, tag, release, registry upload | Yes; externally visible and potentially irreversible. | Version, artifact, changelog, signing/provenance, release gate, and explicit authorization. |
| Vulnerability disclosure | Yes; may expose security information. | Follow [SECURITY.md](../../SECURITY.md); do not disclose publicly without the approved process. |

## Device rules

- Never assume a serial device is the intended board merely because it appears
  on a known port. Verify identity before flashing, erasing, or resetting.
- Do not use a broad erase or destructive reset to “make a test clean” unless
  the user has approved that specific loss of state.
- Keep serial logs, dumps, and collected envelopes private until reviewed.
  Redact identifiers, memory contents, endpoints, credentials, and keys.
- Use the exact toolchain and board configuration documented by the fixture;
  compilation against a similar target is not equivalent evidence.

## Reporting physical work

For each authorized run, record:

```text
Board and revision:
Connection/port:
Firmware commit:
SDK, compiler, linker, and configuration:
Scenario and command:
Expected destructive impact:
Observed raw output (sanitized):
Decoded/recovered result:
Pass/fail and remaining limitation:
```

Do not report a physical result from an unexecuted command, copied terminal
text, or a host simulator. The HIL policy and known limits are documented in
[docs/hil.md](../hil.md) and
[docs/production-readiness.md](../production-readiness.md).
