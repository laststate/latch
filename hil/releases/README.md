# Release qualification manifests

Stable (`v1+`) releases are gated on a manifest named `v<version>.json` in this
directory. A manifest is release evidence, not a claim generator: every board
entry must point at a checked-in HIL evidence file and list only scenarios that
were actually run on that exact hardware/toolchain/firmware combination.

Use `v1.0.0.json.example` as the schema example. Do not copy its placeholder
values into a real release. The signed release tag protects the manifest and
referenced evidence as part of the Git commit.
