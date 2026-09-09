# Versioning and releases

Every merge to `main` that passes the complete `Native quality` workflow is published as
an immutable source release. Green CI on the exact commit is the release gate; there is no
separate publisher workflow and no external attestation that must exist before a tag can
be created. Semantic-owner coordinates, corpus digests and any independent verification
receipts stay recorded in `resources/contracts.json` as metadata and are printed in the
release notes; they do not block publication.

## Version source of truth

`resources/capabilities.json` `version` (mirrored into `computation.engine_version`) is the
only declared version. `CMakeLists.txt` reads it, so `project(KumweEngine VERSION ...)`, the
installed CMake package version and the runtime capabilities always agree.

```sh
bash tools/version.sh get          # print the declared version
bash tools/version.sh set 1.1.0    # declare a minor/major release in a pull request
bash tools/version.sh next         # next patch above the declared version and published tags
```

`set` also refreshes the `resources/capabilities.json` digest recorded in
`MIGRATION-HANDOFF.md`, which `tools/check-manifests.mjs` verifies.

## What the workflow does on `main`

1. **`version`** reads the declared version. If tag `vVERSION` does not exist, this commit
   is released as that version. If the tag already exists for a different commit, the job
   bumps the patch (above the declared version and every published `vMAJOR.MINOR.*` tag),
   commits `Release vX.Y.Z` to `main` and every later job tests that bump commit, so the
   tagged commit is always the fully tested commit. Minor and major releases are declared
   by editing the version in the pull request.
2. **Quality lanes** (Linux GCC and Clang, macOS AppleClang, ASan/UBSan with fuzzing,
   ThreadSanitizer, network-isolated archive consumer, fault seeds and a dry run of the
   release bundle) run against that exact commit.
3. **`release`** runs `tools/release-bundle.sh`, which exports the committed source twice
   and requires identical bytes, writes the SPDX inventory, `source.json` and `SHA256SUMS`,
   and refuses build residue, oracles or development-only tooling in the archive. GitHub
   OIDC build provenance is attested for the bundle. `tools/release-publish.sh` creates the
   annotated tag on the tested commit and publishes the GitHub release with notes from
   `tools/release-notes.sh`. Tags are never moved and assets are never replaced; a rerun
   verifies existing bytes and uploads only missing assets.
4. **`notify-binding`** sends `repository_dispatch` `engine-release` to
   `kumwe/kumwe-engine` with the version, tag, commit and archive digest. The binding then
   embeds the exact archive and publishes the PHP extension under the same version. This
   job needs the repository secret `KUMWE_BINDING_DISPATCH_TOKEN` (a fine-grained token
   with Contents: read and write on `kumwe/kumwe-engine`). Without it the job fails with
   that instruction; the binding also polls the latest Engine release on a schedule.

A `workflow_dispatch` run on `main` follows the same rules. To cut a patch release of a
`main` tip that is already released (for example to ship a binding-only change, because
the binding version is hard-linked to the Engine version), start the run with the `bump`
input enabled: the version job then declares the next patch, pushes the bump commit and
releases it after the lanes pass.

Reruns are safe: a tag that exists on the tested commit without its GitHub release is
completed (`version.sh` checks `gh release view`), a "Release vX.Y.Z" bump commit pushed
by an earlier run on top of the rerun commit is adopted instead of stacked, and a tag that
another run published first makes the release job start a fresh run on the `main` tip,
which declares the next patch.

## Release assets

| Asset | Content |
|---|---|
| `kumwe-engine-source.tar.gz` | `git archive --format=tar --prefix=kumwe-engine/ COMMIT \| gzip -n`; excludes `.github`, PHP oracles, benchmark evidence and Node release tooling (`.gitattributes` `export-ignore`) |
| `SHA256SUMS` | digests of the archive, `source.spdx.json` and `source.json` |
| `source.json` | version, tag, commit, tree, archive digest and size, ABI, capabilities, corpora, semantic inputs |
| `source.spdx.json` | complete SPDX 2.3 inventory of the archive |
| `build-provenance.sigstore.json` | the OIDC provenance bundle also stored in the GitHub attestation store |

Verify a download:

```sh
sha256sum --check SHA256SUMS
gh attestation verify kumwe-engine-source.tar.gz --repo kumwe/engine
```

## Repository settings the pipeline relies on

- The `version` job pushes bump commits to `main` with the workflow token. Branch
  protection must allow that push (or the bypass list must include GitHub Actions). If the
  push is refused, the run reports it and skips the release; declaring the bump in the
  pull request avoids the push entirely.
- Bump and tag commits are authored as `Lemuel <lemuel@vdm.to>` unless repository
  variables `KUMWE_RELEASE_AUTHOR_NAME` and `KUMWE_RELEASE_AUTHOR_EMAIL` override them.
- Actions must be allowed to create releases and to start this workflow (`contents: write`
  and `actions: write` are requested per job; no personal token is needed for the release
  itself).
- Default-branch runs use a per-commit concurrency group, so quick successive merges each
  keep their run; the release job serialises tagging, and a version published first by a
  neighbouring run is handled as described above.

## Tooling

Bash, Git, `jq`, Node and `gh` are the only release-tooling dependencies, and they are
used only by the workflow and by `tools/*.sh` and `tools/*.mjs`. None of them are needed
to build or test the engine from the published archive, and none of the published files
depend on them. No Python is used anywhere in this repository.
