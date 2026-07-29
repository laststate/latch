# Repository automation

Latch ships with 27 GitHub Actions workflows. Read-only validation covers Windows, Linux, macOS, GCC, Clang, MSVC, ARM, RISC-V, Rust `no_std`, sanitizers, coverage, CodeQL, fuzzing, reproducibility, documentation, compact source distributions, packages and releases.

The repository bots perform these write operations:

- `PR autofix bot` formats changed C/C++ and Rust files on non-fork pull requests and commits the result to the pull-request branch. The resulting push triggers fresh validation normally.
- `Maintenance health bot` runs monthly, applies deterministic formatting to the complete tree, tests the formatted result, and opens or refreshes a maintenance issue when the checked-in tree has drifted.
- `Dependabot merge bot` enables squash auto-merge only for patch-level updates. Minor, major, grouped, and unclassified changes require review.
- `Repository triage bot` creates the managed labels, labels pull requests by component and size, and marks new issues for triage.
- `Stale repository bot` marks inactivity without automatically closing issues or pull requests. Security, automation, first-contribution, help-wanted, and hardware work are exempt.
- `Workflow health bot` opens or updates an issue when a scheduled/default-branch workflow fails and closes it after recovery.

Release automation generates checksums, an SPDX 2.3 SBOM, GitHub/Sigstore build provenance attestations and release notes grouped by labels.

## Required repository settings

After the workflows are committed to GitHub, configure the repository with:

1. Default Actions workflow permissions set to read. Individual workflows request narrowly scoped write permissions.
2. Auto-merge enabled for patch-only Dependabot automation. The repository does not require Actions to create or approve pull requests.
3. A ruleset for `prod` blocking direct pushes, deletion, and force-pushes; requiring pull requests, resolved conversations, linear history, and the stable `required` check.
4. Code scanning enabled so SARIF results from CodeQL and OpenSSF Scorecard appear in the Security tab; OSV remains an independent dependency check.
5. Discussions, private vulnerability reporting, secret scanning, and push protection enabled.

Fork pull requests never receive write credentials. Autofix commits are limited to branches in the same repository, and `pull_request_target` workflows do not checkout or execute pull-request code.

## Reviewable and compact source

The default branch always keeps reviewable, formatted source. The manually dispatched `Compact source distribution` workflow creates a separate tree, verifies minifier idempotence, builds and tests that tree, and uploads it as an artifact without writing to a contributor's branch. The stable pull-request gate performs the same verification without uploading a duplicate artifact.

Tagged releases include a compact source archive alongside compiled packages, checksums, the SPDX SBOM, and provenance. Source compaction is a distribution option, not a branch-management strategy and not a replacement for compiler or linker optimization.
