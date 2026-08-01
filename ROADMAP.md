# Roadmap

Latch is a pre-1.0 public preview. The portable evidence pipeline is implemented and host-tested; the next phase is to make evaluation easy and turn platform claims into reproducible hardware evidence.

This roadmap is directional rather than a promise of dates. Issues and release notes are the source of truth for committed work.

## Public-preview priorities

### 1. Make the first result obvious

- Keep the host capture-and-decode demo working on Linux, macOS, and Windows.
- Keep the machine-readable decoder and copy-paste CMake consumers stable in CI.
- Document the smallest useful retained-RAM and Flash integrations.

### 2. Publish hardware qualification evidence

- Record board, revision, SDK, compiler, linker script, and test procedure for each result.
- Exercise real HardFault/trap, watchdog, brownout, corrupt-stack, nested-fault, and interrupted-Flash scenarios.
- Promote a platform from “integration boundary” to “qualified example” only when its evidence is reproducible.

### 3. Harden protocol and storage tooling

- Keep LEP, stream/ACK, compression and AEAD fuzzers seeded from public vectors.
- Expand cross-language and negative compatibility vectors.
- Make spool inspection and recovery behavior easier to test outside firmware.

### 4. Review the security boundaries

- Obtain independent review of envelope cryptography, nonce/key provisioning, redaction, replay behavior, and secure-element adapters.
- Document measured stack, code-size, and timing costs for representative profiles.
- Keep security claims narrower than the evidence.

### 5. Establish a predictable release rhythm

- Tag the first `v0.2.0` public-preview baseline after CI is green.
- Publish small, reviewable releases with changelog entries and generated provenance.
- Keep `good first issue` work curated and close the loop with contributors quickly.

## Community-owned opportunities

The highest-value outside contributions are:

- real board and toolchain qualification reports;
- FreeRTOS, Zephyr, ESP-IDF, and bare-metal examples;
- decoder, fuzzing, and packaging improvements;
- reviews from people who have debugged retained state, brownouts, Flash interruption, or nested faults in production devices.

See [`good first issue`](https://github.com/laststate/latch/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) and [`help wanted`](https://github.com/laststate/latch/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22).

## Non-goals

Latch does not aim to own:

- a hosted observability dashboard;
- the product's scheduler, allocator, bootloader, network stack, or Flash driver;
- automatic capture of arbitrary memory without an explicit policy;
- certification for hardware that has not been tested;
- source minification as a substitute for compiler and linker optimization.
