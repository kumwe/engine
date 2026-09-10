# Changelog

## 1.0.3

- Declare the next immutable Engine source release so the corrected PHP binding metadata
  generator can synchronize and publish the matching 1.0.3 release. Runtime semantics, ABI,
  semantic-owner receipts and corpus commitments remain unchanged.

## 1.0.2

- Restore the migration handoff's exact candidate attestation schema identifier while
  retaining the separate source-release provenance and publication policy.
- Record the independently verified Reporting 0.1.3 receipt from SDK run 34515947985
  after its clean Packagist consumer and offline lock replay passed. All semantic-owner
  source, API, corpus and archive commitments remain unchanged; native semantics and ABI
  remain unchanged.

## 1.0.1

- Make the release pipeline complete without people. Every change to released source
  (everything the source archive exports) declares a new version in
  `resources/capabilities.json` in the same change, and the pull-request check
  (`tools/version.sh check`) refuses a released-source change that keeps a published version.
  On `main` the workflow completes any tag whose GitHub release is missing from the commit the
  tag identifies (with the current tooling and without repeating the lanes that passed before
  it was tagged, then re-evaluates the tip in a follow-up run),
  releases an unreleased declared version, publishes nothing when only export-ignored files
  changed, and declares the next patch itself only as a fallback. Tags are never moved or
  deleted; a draft left by an interrupted publish is promoted, never recreated. The
  `tools/test-version.sh` self-test runs in every workflow.
- Fix the release publisher: `gh` now names the repository on every call and runs from the
  checkout, so publishing no longer depends on the working directory. The first 1.0.0 release
  run tagged `v1.0.0` and then failed at `gh release create`; the fixed workflow publishes
  that release from the tagged commit.

## 1.0.0

- Publish every default-branch commit that passes the complete quality workflow as an
  immutable source release: the declared version in `resources/capabilities.json` is the
  single source of truth (CMake reads it), an already-published version is patch-bumped
  automatically, the tested commit is tagged, and the reproducible archive, SPDX inventory,
  checksums and GitHub OIDC build provenance are attached. Each release dispatches
  `engine-release` to `kumwe/kumwe-engine`, which embeds the exact archive and publishes the
  PHP extension under the same version. The former external attestation gate, the
  candidate-reference pull request block and the separate publisher workflow are removed;
  semantic-owner verification receipts remain recorded metadata.
- Remove all Python tooling. The Unicode table generator is now the C++ program
  `tools/generate-unicode-data.cpp` built and checked by CTest; the fault-seed mutation is
  pure Bash; the release tooling is Bash, `jq` and Node; the whole-boundary benchmark
  harness moved to the binding repository, where its PHP orchestrator already lives.
  The published source archive now excludes `.github` and the Node release scripts.

- Record the corrected durable semantic-owner verification receipts and complete manifest
  and corpus identities. Preserve Reporting 0.1.3's published source as explicitly unverified
  until its clean consumer can resolve the missing Access Control package registration.

- Freeze the initial C ABI 1 boundary for the proposed 1.0.0 release without
  changing symbols, statuses, view layout, lifetimes, envelopes or semantic outputs.
  Preserve a separately hashed historical C11 client/header and dynamically replay
  its 295 assertions against every Linux candidate and sanitizer build.
- Record truthful independently verified semantic-input metadata in
  `resources/contracts.json`. (The semantic-owner and candidate attestation gates that
  this and the following entry once introduced are superseded: publication is now gated
  by the quality workflow alone, as described in the first entry above.)
- Superseded: a schema-valid external candidate attestation is no longer required before
  a new Engine tag or source publication.
- Run the standalone archived CMake consumer inside a network namespace and bind
  every qualification checkout to the exact candidate head.
- Retain the maintained PCRE2 10.42 source and upstream security backports. Disable
  the unsupported legacy ARM64 SIMD JIT path while preserving scalar JIT and all
  existing validation semantics; qualify the actual ARM64 corpus and C consumer.

- Candidate canonical KEC1 framing removes tagged JSON/base64 expansion inside the existing
  ABI and PHP API; identical semantic corpus, limits and finding order are retained.
- Reuse exact incremental document output totals at finalization. Fuse UTF-8 admission and
  JSON string-size counting, and collect the quoted size of immutable serialized results
  during encoding. Existing failure ordering and original logical batch budgets remain intact.
- Implement all five deterministic kernels: exact decimal batches, compiled formula VM,
  normalized document preparation/computation/validation, report materialization and GenericV1
  canonical encoding with streaming SHA-256. All native behavior/boundary/conformance tests
  reside in Engine; the binding owns PHP-specific tests and App retains host acceptance.
- Correct exact output-byte accounting: valid nonempty batches now fit a budget equal to
  their complete serialized size; one-byte-short limits still refuse atomically.
- Remove direct-document tree copies and intermediate output-item buffers; parse ordinary
  JSON string spans in blocks while retaining UTF-8, escape, numeric and bound semantics.
- Reuse canonical tagged-decoder admission/order directly during emission instead of building
  and sorting a second normalized tree; decode base64 using a fixed lookup table.
- Match frozen corpora to exact published Conversion, Definition, Record Model, Reporting and
  Canonical JSON coordinates; preserve the independent release-attestation barrier.
- Retain C/ABI/CLI, corpus, fuzz/sanitizer/thread, lifecycle, archive/install, fault-seed and
  whole PHP/Zend benchmark gates. Pin all CI actions to reviewed source commits.
- NRM-2026-013 / NRM-2026-042 / NRM-2026-044 are enabling implementation evidence.
  ABI 1 is frozen. Verified native publication and later App workload acceptance
  remain separate evidence gates; no functional roadmap completion is claimed.
