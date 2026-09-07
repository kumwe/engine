# Release and candidate gates

All five native kernels are implemented and have owner-corpus replay, bounded ABI execution,
consumer builds and memory/security CI. `0.0.0-dev` identifies an implementation candidate;
ABI 1 is not frozen and no native release has been published. Implementation completion does
not establish release verification or App acceleration.

`resources/contracts.json` records exact published semantic-owner tags, commits and corpus paths.
The recorded corpus bytes match those immutable source commits. External release attestations
remain a separate requirement: publication and a matching hash alone do not establish that the
owner release passed every programme acceptance gate. Updates must retain an exact coordinate;
never use a moving branch, `latest`, or an unconstrained range for embedded native sources.

The remaining release gates are the independently verified semantic release barrier, accepted
ABI compatibility/freeze, a final supported-platform run, representative whole-boundary performance
acceptance, and signed source/artifact provenance. The retained benchmark evidence includes slower
native document, preparation and canonical workloads; a faster inner kernel cannot close that gap.
Repeat the complete PHP/Zend comparison on the final artifact after optimization. No App cutover
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
It refuses the current development version, unfrozen ABI, draft contract matrix and
unverified semantic releases. It is intentionally absent from ordinary candidate CI.
This option does not verify signatures, approve ABI freeze or replace the independent
candidate/release attestations. Those are review decisions and evidence produced by
the existing programme release-verification process.

A stable release publisher must use the verified source bundle from the exact approved
commit and existing immutable tag. It must retain the final supported-platform and
whole-boundary benchmark evidence, sign/verifiably attest the source assembly and any
compiled outputs with their actual toolchain/build tuple, and provide the external
candidate and release verification records. A source SPDX inventory does not describe
an unbuilt binary. No publishing workflow runs for a candidate, and this tooling never
turns an unsigned source statement into a release-verification claim.
