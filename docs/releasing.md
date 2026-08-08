# Release procedure

Creating a Git tag starts the release workflow, which validates the tag, builds CPack archives on three operating systems, generates a compact source artifact, checksum file, SPDX SBOM and provenance, then creates the GitHub release. It does **not** publish to PlatformIO, crates.io or the ESP-IDF Component Registry automatically.

Use a clean, reviewed commit reachable from `prod`. Publishing a tag or a registry package changes public state; do it only after the maintainer has approved the release candidate.

## Prepare a release candidate

1. Choose the version (for example `0.3.0`) and update it consistently in `CMakeLists.txt`, `include/laststate/version.h`, `library.json`, `library.properties`, `idf_component.yml` and `rust/latch/Cargo.toml`.
2. Move the relevant `Unreleased` changelog entries into a dated `## [0.3.0] - YYYY-MM-DD` section and add `docs/releases/v0.3.0.md` with the support/qualification boundaries.
3. Run the metadata gate and normal validation:

   ```sh
   python tools/check_release_metadata.py --version 0.3.0
   cmake --preset host-release
   cmake --build --preset host-release
   ctest --preset host-release
   cargo test --manifest-path rust/latch/Cargo.toml
   cargo publish --manifest-path rust/latch/Cargo.toml --dry-run
   ```

4. Run the board-specific HIL and footprint procedure for every configuration that will be represented by the release. Do not promote a source-level port to qualified without the evidence described in [platform support](platform-support.md).
5. Open/merge the release-preparation pull request into `prod`, wait for all required checks, and confirm the resulting commit is exactly the reviewed one.

## Create the GitHub release

After explicit release approval, from the merged `prod` commit:

```sh
git fetch origin prod --tags
git switch prod
git pull --ff-only origin prod
git status --short
python tools/check_release_metadata.py --version 0.3.0
git tag -s v0.3.0 -m "Latch v0.3.0"
git push origin v0.3.0
```

The tag workflow rejects a tag whose version is not synchronized across the publishable package metadata, API header and dated changelog, or whose commit is not reachable from `prod`. Monitor the workflow and inspect the generated release assets before announcing it.

## Publish registries separately

Registry publication is intentionally manual and version-immutable. Follow [the registry runbook](registries.md) after the GitHub release succeeds. Use the same immutable tag, confirm each package's dry run and publish one registry at a time. If a publication fails, release a corrected patch version rather than trying to replace a published version.
