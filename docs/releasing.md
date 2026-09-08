# Release and candidate gates

All five native kernels are implemented and have owner-corpus replay, bounded ABI execution,
consumer builds and memory/security CI. `1.0.0` is the proposed first release;
ABI 1 is frozen and no publication is inferred from that source version. Implementation completion does
not establish release verification or App acceleration.

`resources/contracts.json` records exact published semantic-owner tags, commits and corpus paths.
The recorded corpus bytes match those immutable source commits. External release attestations
remain a separate requirement: publication and a matching hash alone do not establish that the
owner release passed every programme acceptance gate. Updates must retain an exact coordinate;
never use a moving branch, `latest`, or an unconstrained range for embedded native sources.

The separate Computation Phase 1A prerequisite uses the published portable-only
`v0.1.1` release at `fc9d049f8b675c8e19fd1672d49b5e206c9ad52a`. Its actual source archive,
API, capability, portable corpus and external attestation identities are recorded in
`resources/contracts.json`. Only the schema-valid durable independent receipt linked by that
matrix establishes admission. It must cover the original archive, complete canonical schemas,
package gates and a fresh offline authoritative consumer with no native extension or binding classes.
Earlier malformed or superseded receipts cannot close the prerequisite.
The later Computation native adapter successor still waits for verified Engine and extension
releases; its publication and App adoption are separate steps.

The release gates include the independently verified semantic release barrier, the frozen ABI compatibility fixture, a final supported-platform run, published representative whole-boundary performance
evidence, and signed source/artifact provenance. The retained benchmark evidence includes slower
native document, preparation and canonical workloads; a faster inner kernel cannot close that gap.
Repeat the complete PHP/Zend comparison on the final artifact and retain all results. No App cutover
or capacity claim follows from passing the native test suite.

An independent candidate cross-build must consume the exact Engine PR-head archive in
`kumwe/kumwe-engine` without build-time network retrieval. It checks Zend marshalling, module load,
ABI/capability/corpus agreement, lifecycle and the claimed PHP matrix. Its external
`ENGINE-CANDIDATE-ATTESTATION.yaml` binds both tested commits/trees and the archive digest; it must
remain outside both source trees. Any tested-input change invalidates that evidence. Human merge
and immutable Engine publication follow a passing current candidate gate. A separate verifier
then attests the published Engine release before the stable extension embeds it.

`bash tools/source-archive.sh` archives committed source with reproducible gzip metadata.
`check-archive.sh` compares two archives and builds an isolated installed consumer.
`node tools/source-sbom.mjs` inventories committed source with SPDX/SHA1/SHA256 identities.
CI stores these development artifacts against the tested head. They are not release attestations.
The non-publishing source-release stage below now prepares and verifies the complete
source evidence bundle. Candidate CI retains it against the tested head; publication and
externally signed release verification remain separate stages under the immutable-release policy.

## Source bundle preparation and verification

The source assembly and verification stage is implemented by `tools/release-source.py`.
It reuses the committed `source-archive.sh` and `source-sbom.mjs` recipes, builds the
archive twice, and verifies its full SPDX file inventory, ABI files and semantic corpus
identities. It needs Git, gzip, Bash, Node and Python 3.12+ as development tools only.
No network request, tag creation, release API or publication permission is used.

Run from a clean, committed checkout. Choose a new directory outside the repository:

```sh
python3 tools/release-source-test.py
python3 tools/release-source.py prepare ../engine-source-evidence
python3 tools/release-source.py verify ../engine-source-evidence \
  --expected-commit "$(git rev-parse HEAD)"
```

The external directory contains:

- `kumwe-engine-source.tar.gz`: the exact committed export, including its licenses,
  public headers, ABI/capability manifests and corpora;
- `source.spdx.json`: the existing complete Engine source SPDX inventory;
- `source.json`: repository, exact commit/tree, optional existing tag, archive size and
  digest, ABI/version identity, manifest digests and exact semantic-owner materials;
- `source.provenance.json`: an unsigned in-toto source-assembly statement that binds
  that archive and SPDX inventory to the source materials;
- `SHA256SUMS`: checksums of all four artifacts, using relative names.

Verification regenerates the committed export and metadata, checks byte equality and
refuses rehashed false claims. Supply `--expected-sha256` when an independent caller has
an approved archive digest. `--tag v1.0.0` checks an existing tag against the same commit;
its version must also equal the declared source version, with only the optional `v` prefix ignored.
It never creates or moves a tag. Outputs are kept outside the tested source, so neither
the source tree nor its archive contains its own final identity. The tool refuses
tracked edits, unsafe archive paths, links, caches, credential-like files and PHP oracles.

`--require-stable` is an additional source-state check for the stable release stage.
It refuses development versions, unfrozen ABIs, draft contract matrices and
unverified semantic releases, and an absent or incomplete portable Computation baseline.
The baseline record must have `state: release-verified`, the exact `kumwe/computation`
version/tag/commit and source archive SHA256, public API and capability manifest SHA256s,
a nonempty map of portable corpus paths to SHA256s, the observed released runtime
requirements without a native dependency, `native_bindings_present: false`, and an
external attestation `uri`/`sha256` reference. `api_digest`, `capability_digest` and
`corpus_digests` identify the portable release artifacts; they must not be copied from
an adapter candidate. A separate verifier must first check those artifacts and the
absence of native concrete classes and ConfigProvider/factory bindings. The source
check validates recorded prerequisites; it does not download or independently attest
their content. Unknown facts remain null until that verification exists. Both
`source.json` and the unsigned provenance retain the baseline record. The stable
option is intentionally absent from ordinary candidate CI.
This option does not verify signatures, approve ABI freeze or replace the independent
candidate/release attestations. Those are review decisions and evidence produced by
the existing programme release-verification process.

## Immutable source publication

`Native source release` runs after successful `Native quality` on the exact current
default-branch commit, or through an explicit dispatch naming that successful run.
`tools/release-native.py` checks every required Linux/macOS/compiler, sanitizer and
archive lane, reruns the stable source gate, and refuses skipped or stale evidence.
The workflow signs all five source evidence files with GitHub OIDC and verifies the
repository, workflow, default-branch ref, exact commit and hosted build identity.
Only then may it create the version tag and draft release. Existing tags and assets
cannot be moved or overwritten; a retry verifies identical existing bytes before
uploading missing files. Publication rechecks the final assets and current branch.
`python3 tools/release-native-test.py` covers these refusal and interrupted-retry paths.

The sixth release asset, `build-provenance.sigstore.json`, authenticates source assembly.
This source-only publisher does not describe an unbuilt binary or produce an independent
release-verification claim. The external candidate cross-build, final supported-platform
and whole-boundary evidence, and separate verifier's published-release attestation remain
required. A candidate fails the stable source gate and cannot publish.

## Mandatory external candidate reference

A merged source version and successful Engine-only CI cannot authorize a new native
release. Before creating any new tag or uploading release assets, the publisher
requires an independently prepared, passing `ENGINE-CANDIDATE-ATTESTATION.yaml` in
immutable SDK `evidence/` storage. The exact merged Engine pull request body must
contain one strict machine block:

```text
<!-- kumwe-engine-candidate/v1
{"uri":"https://raw.githubusercontent.com/kumwe/extension-sdk/EXACT_40_HEX_COMMIT/evidence/native/ENGINE-CANDIDATE-ATTESTATION.yaml","sha256":"EXACT_YAML_SHA256"}
-->
```

The capitals above are documentation placeholders, never accepted gate values.
Optional workflow-dispatch inputs may supply the same exact URI and digest instead.
The normal `workflow_run` path reads only a genuinely merged Engine pull request
whose merge commit equals the currently qualified default-branch commit. A missing,
ambiguous, mutable or mismatched reference fails before publication; a maintainer
can attach the reviewed record and rerun the failed publisher without changing
source or creating a digest cycle.

`tools/release-validation/candidate-gate.mjs` uses the full authoritative candidate
schema and verifies the original candidate commit, Git tree, raw archive and
handoff digest. Its tree must equal the merged source tree while the fresh main
quality run independently passes. The record is never relabeled as an attestation
of the merge commit. The exact binding candidate must embed that original archive;
its five source/binding/sanitizer/offline-PIE/whole-boundary jobs must all succeed,
and its referenced artifact must belong to that actual source run. All recorded
semantic API, capability, service and corpus inputs are covered by the record.

Install and test the exact release-validator dependencies from the repository root:

```sh
npm --prefix tools/release-validation ci --ignore-scripts --no-audit
node --test tools/release-validation/candidate-gate.test.mjs
```

These are publication tools only; CMake and installed consumers need no npm
packages or network. The publisher generates release notes from the exact source,
including dependency/corpus identities, capabilities, limits, changes and security
policy links. The independently verified published Engine still has to be
re-embedded and fully rebuilt by the stable binding stage.
