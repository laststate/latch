# Repository automation

Latch ships with 22 GitHub Actions workflows. Read-only validation covers Windows, Linux, macOS, GCC, Clang, MSVC, ARM, RISC-V, Rust `no_std`, sanitizers, coverage, CodeQL, fuzzing, reproducibility, documentation, packages and releases.

The repository bots perform these write operations:

- `PR autofix bot` formats changed C/C++ and Rust files on non-fork pull requests, commits the result to the pull-request branch and dispatches fresh validation for the new commit.
- `Maintenance PR bot` runs monthly, applies deterministic formatting to the complete tree, tests it, updates `automation/maintenance` and opens or refreshes a pull request.
- `Dependabot merge bot` enables squash auto-merge. Branch protection and required checks still decide whether the pull request may merge.
- `Repository triage bot` creates the managed labels, labels pull requests by component and size, and marks new issues for triage.
- `Stale repository bot` manages inactivity without automatically closing pull requests. Security and automation incidents are exempt.
- `Workflow health bot` opens or updates an issue when a scheduled/default-branch workflow fails and closes it after recovery.

Release automation generates checksums, an SPDX 2.3 SBOM, GitHub/Sigstore build provenance attestations and release notes grouped by labels.

## Required repository settings

After the workflows are committed to GitHub, configure the repository with:

1. Actions workflow permissions set to read and write.
2. “Allow GitHub Actions to create and approve pull requests” enabled so the maintenance bot can open its PR. The bots do not approve their own changes.
3. Auto-merge enabled for Dependabot automation.
4. A branch protection rule or ruleset for `main` requiring CI, quality, Rust and security checks before merge.
5. Code scanning enabled if SARIF results from CodeQL, OpenSSF Scorecard and OSV should appear in the Security tab.

Fork pull requests never receive write credentials. Autofix commits are limited to branches in the same repository, and `pull_request_target` workflows do not checkout or execute pull-request code.
