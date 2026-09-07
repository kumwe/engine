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
Release automation must be added and reviewed with the accepted immutable-release policy before
Engine 1.0.0 is published; this candidate deliberately has no publishing workflow.
