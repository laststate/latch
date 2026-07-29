# Contributing to Latch

Thank you for helping make embedded failures easier to diagnose. Contributions do not need to be large: a reproducible board report, a clearer integration note, a test vector, or a focused code review can be more valuable than a new feature.

## Find a useful first contribution

- Browse [`good first issue`](https://github.com/laststate/latch/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) for work that should not require understanding the whole runtime.
- Browse [`help wanted`](https://github.com/laststate/latch/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22) for board, RTOS, transport, and tooling work where outside experience is especially useful.
- Use [GitHub Discussions](https://github.com/laststate/latch/discussions) for integration questions or an early design proposal.
- Open an issue directly for a small bug or documentation gap.

Comment on an issue before starting substantial work so contributors do not duplicate effort. For protocol, public API, cryptography, persistent layout, or cross-cutting architecture changes, start a discussion first.

## Maintainer response target

We aim to acknowledge new issues and pull requests within 72 hours and provide an initial triage within seven days. This is a maintainer target, not an SLA. If there is no response after seven days, one friendly ping is welcome.

## Development setup

The default host preset requires:

- CMake 3.20 or newer;
- Ninja and a C11/C++17 compiler;
- Python 3;
- Rust stable only for changes under `rust/`.

Build and run the complete portable suite:

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
python tools/check_docs.py
python tools/check_test_vector.py
python tools/test_minify_sources.py
```

On Windows with Visual Studio 2022, use:

```powershell
cmake --preset host-msvc
cmake --build --preset host-msvc
ctest --preset host-msvc
```

When Rust changes:

```sh
cargo fmt --manifest-path rust/latch/Cargo.toml -- --check
cargo clippy --manifest-path rust/latch/Cargo.toml --all-targets -- -D warnings
cargo test --manifest-path rust/latch/Cargo.toml
```

Use the narrowest useful loop while developing:

| Change | Fast validation |
|---|---|
| Documentation | `python tools/check_docs.py` |
| LEP or decoder | `python tools/check_test_vector.py` and the C/Rust tests |
| Portable C/C++ | `cmake --build --preset host-debug` and `ctest --preset host-debug` |
| Rust | format, Clippy, tests, and the relevant `no_std` target |
| Port or fault entry | host simulator where possible, plus the exact board/toolchain evidence |
| Workflow | local `actionlint` when available |

## Invariants that changes must preserve

- The fault path remains heap-free, bounded, and independent of a hosted C library.
- Changes to LEP preserve existing vectors or intentionally introduce and document a protocol version.
- Decoders reject truncated or length-overflowing data before reading it.
- Persistent writes remain recoverable after interruption.
- New platform code includes a simulator test where possible and names every unverified hardware assumption.
- Security-sensitive data is minimized and redacted before capture, not only before transport.

Do not include device secrets, production keys, captured customer memory, private endpoints, or proprietary firmware in fixtures, fuzz corpora, issues, or pull requests. Report vulnerabilities privately through the process in [SECURITY.md](SECURITY.md).

## Pull requests

1. Fork the repository and branch from the current default branch.
2. Keep the change focused. Separate refactors from behavior changes when practical.
3. Add tests for new public behavior and update documentation for user-visible changes.
4. Add an entry to [CHANGELOG.md](CHANGELOG.md) when the change affects adopters.
5. Fill in the pull request template, including hardware/toolchain evidence or `not hardware-tested`.
6. Resolve review threads and keep the branch current before merge.

Draft pull requests are welcome for early technical feedback. No Contributor License Agreement is required; contributions are accepted under the repository's Apache-2.0 license.

Repeated contributors who review changes, help with triage, or own an integration area can be invited into the maintainer workflow as the community grows.

By participating, you agree to the [Code of Conduct](CODE_OF_CONDUCT.md).
