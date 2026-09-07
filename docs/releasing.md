# Release and candidate gates

This incomplete E0/E1 slice has no publishing workflow and must remain a draft PR. `0.0.0-dev` is
capability metadata, not a release record. C ABI symbols/layout are proposed, not frozen. Human
merge/tag/publication and App adoption are separate decisions under the v2 protocol.

Before the first Engine candidate: complete decimal plus formula/document execution; then report
and canonical streaming; immutable plan hydration/cache, cancellation, deterministic findings,
resource limits, complete unit/property/differential/fuzz/sanitizer/lifecycle and ABI suites.
Reconcile exact verified semantic releases and corpora for all implemented owners. The current
Conversion corpus is reviewed draft evidence and is not a released immutable semantic input.

Before Engine 1.0.0 / frozen ABI 1: complete every v2 Engine brief gate, supported matrix, full-boundary
benchmarks, compatibility checks, deterministic source archives, license/advisory review, SBOM and
signed provenance. Create changelog-driven release automation with protected-branch and immutable
release checks only after the release gate is implemented. No guessed tag or self-authored passing
release attestation belongs in source.

A separate agent must build the exact Engine PR-head source archive in a dedicated
`kumwe/kumwe-engine` candidate branch, without network retrieval at consumer compile time. It tests
thin Zend marshalling, module load, ABI/capability/corpus agreement, lifecycle and the supported PHP
matrix. Its external `ENGINE-CANDIDATE-ATTESTATION.yaml` binds Engine and extension commit/tree,
archive digest and evidence; never store it inside either tested source tree. Any input change
invalidates it. Only a current passing candidate check permits ready-for-review status and human
merge/release. After publication a fresh independent verifier produces the release attestation.

`bash tools/source-archive.sh` deterministically archives committed source with no timestamp-bearing
gzip header. `check-archive.sh` compares two archives and builds an isolated consumer from it.
`node tools/source-sbom.mjs` inventories every committed source file with SPDX/SHA1/SHA256 identity.
These artifacts live outside the archived source and do not claim their own final digest. CI uploads
them keyed to the tested head; they are development artifacts, not a candidate attestation or release.
