# Versioning and releases

The pipeline is fully automated. People merge; nothing else is done by hand. Nobody creates,
moves or deletes tags, edits releases, declares versions in the workflow UI or re-runs jobs: the
`Native quality` workflow reads the declared version, releases it, completes anything an earlier
run left unfinished, and asks the PHP binding to follow.

## The version file and the one rule

`resources/capabilities.json` `version` (mirrored into `computation.engine_version`) is the only
declared version. `CMakeLists.txt` reads it, so `project(KumweEngine VERSION ...)`, the installed
CMake package version and the runtime capabilities always agree.

**Every change to released source declares a new version in the same change.** Released source
is everything the source archive exports: the whole repository except the paths marked
`export-ignore` in `.gitattributes` (the workflow, the release tooling under `tools/`, the PHP
oracles and the benchmark evidence). Headers, sources, corpora, manifests and documentation are
released source; a change to any of them is a new release.

```sh
bash tools/version.sh get           # print the declared version
bash tools/version.sh next          # next patch above the declared version and every published tag of that line
bash tools/version.sh set 1.0.4     # declare the version (patch, minor or major), then commit
bash tools/version.sh check         # what the pull-request check does
bash tools/version.sh digest [REV]  # released-source identity of a commit (exported paths and blob ids)
```

`set` also refreshes the `resources/capabilities.json` digest recorded in `docs/release-record.md`,
which `tools/check-manifests.mjs` verifies; commit both files together with the change.

The rule is enforced, not remembered. The `version` job of every pull request runs
`bash tools/version.sh check`, which fails when the change alters released source while the
declared version is already published from another commit, and prints the exact command to run.
A change that touches only export-ignored files passes without a new version, because it does
not change what a release contains.

## What the workflow does on `main`

Every push to `main` (and every `workflow_dispatch` on it) runs the complete workflow. Its
`version` job decides what the run releases, in this order:

1. **A tag without a published release is completed first.** If an earlier run tagged a version
   and then failed before its GitHub release was published (or left a draft), this run releases
   that tag from the commit it identifies, with the current release tooling, and then starts a
   follow-up run so the tip of `main` is evaluated afresh. The quality lanes are not repeated
   for that older commit: a tag is only ever created after every lane passed on it, and this
   workflow's lanes belong to the current tree. Tags are never moved or deleted.
2. **An unreleased declared version is released.** If tag `vVERSION` does not exist, this
   commit is released as that version once every quality lane passes.
3. **A published version whose released source is unchanged releases nothing.** If the tag
   identifies another commit and `version.sh digest` is the same for both, the merge changed
   only export-ignored files.
4. **Otherwise the run declares the next patch itself.** A merge that changed released source
   without declaring a version (the pull-request check was bypassed, or two pull requests
   declared the same version) makes the job commit `Release vX.Y.Z` to `main`, and every later
   job tests and releases that commit. If that push is refused, the run fails and names the fix.

The quality lanes (Linux GCC and Clang, macOS AppleClang, ASan/UBSan with fuzzing,
ThreadSanitizer, the network-isolated archive consumer, fault seeds, the `version.sh` self-test
and a dry run of the release bundle) run against the exact commit being released.

The `release` job checks out the release tooling from the commit that triggered the run and,
separately, the commit being released (`KUMWE_ROOT`), so a release completed for an older tag
is still built and published by the current scripts. `tools/release-bundle.sh` exports the
committed source twice and requires identical bytes, writes the SPDX inventory, `source.json`
and `SHA256SUMS`, and refuses build residue, oracles or development-only tooling in the
archive. GitHub OIDC build provenance is attested for the bundle. `tools/release-publish.sh`
creates the annotated tag on the released commit and publishes the GitHub release with notes
from `tools/release-notes.sh`, naming the repository explicitly on every `gh` call. Tags are
never moved and assets are never replaced: a rerun verifies existing bytes, uploads only what is
missing and promotes a draft left by an interrupted publish. A version that a neighbouring run
published first makes the job start a fresh run on `main`, which then follows the rules above.

`notify-binding` sends `repository_dispatch` `engine-release` to `kumwe/kumwe-engine` with the
version, tag, commit and archive digest. The binding embeds the exact archive and publishes the
PHP extension under the same version. This job needs the repository secret
`KUMWE_BINDING_DISPATCH_TOKEN`; without it the job fails with that instruction, and the binding
also polls the latest Engine release every six hours.

The `bump` input of `workflow_dispatch` declares the next patch even when the tip of `main` is
already released. The binding requests this itself when a binding-only change needs a version
(its versions are hard-linked to the Engine), so no person has to; a bump requested while an
unfinished release is being completed is carried into the follow-up run.

## Release assets

| Asset | Content |
|---|---|
| `kumwe-engine-source.tar.gz` | `git archive --format=tar --prefix=kumwe-engine/ COMMIT \| gzip -n`; excludes `.github`, PHP oracles, benchmark evidence and the release tooling (`.gitattributes` `export-ignore`) |
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

These are configured once; the workflow reports which one is missing when it cannot proceed.

- GitHub Actions must be allowed to push to `main` (branch protection or rulesets must allow
  it, or list GitHub Actions as a bypass). The `version` job pushes the `Release vX.Y.Z` commit of
  rule 4 with the workflow token; with the pull-request check in place this is the fallback for
  bypassed checks and concurrent version declarations, and a refused push fails the run with the
  command that declares the version in a pull request instead.
- The workflow token creates releases and starts this workflow (`contents: write` and
  `actions: write` are requested per job). No personal token is used for the release itself.
- Repository secret `KUMWE_BINDING_DISPATCH_TOKEN`: a fine-grained token with Contents: read
  and write on `kumwe/kumwe-engine`, used only to send the `engine-release` dispatch. One
  fine-grained token with Contents and Actions read/write on both repositories can serve as this
  secret and as the binding's `KUMWE_ENGINE_DISPATCH_TOKEN`.
- Bump and tag commits are authored as `Lemuel <lemuel@vdm.to>` unless repository variables
  `KUMWE_RELEASE_AUTHOR_NAME` and `KUMWE_RELEASE_AUTHOR_EMAIL` override them.
- Default-branch runs use a per-commit concurrency group, so quick successive merges each keep
  their run; the release job serialises tagging.

## Tooling

Bash, Git, `jq`, Node and `gh` are the only release-tooling dependencies, and they are used
only by the workflow and by `tools/*.sh` and `tools/*.mjs`. `tools/test-version.sh` exercises
the version policy against a disposable origin and a simulated `gh` in every run. None of these
tools are needed to build or test the engine from the published archive, and none of the
published files depend on them.
