---
schema: kumwe-migration-handoff/v2
artifact_kind: native_cpp
migration_id: KUMWE-MIG-2026-011
change_set: KUMWE-CS-2026-011
state: draft_pr_open
source:
  app:
    repository: https://github.com/kumwe/app
    baseline_commit: null
    examined_paths: []
    old_namespace_roots: []
    capability_index_sha256: null
  semantic_inputs:
  - owner: kumwe/conversion
    version_or_commit: v0.1.3 at e95d5633722929e77b73005f8c44cfe4d99ac8c3
    manifest_or_corpus: resources/conformance/decimal-v1.tsv (published source; schema-valid durable independent
      release attestation pending)
    sha256: 635db251898707828e24f12b1abb672273552f5f633186a725cc9f50ac08140c
  - owner: kumwe/business-definition
    version_or_commit: v0.1.2 at 1cb59229f311067e8cfdd773a34e34cfeb501791
    manifest_or_corpus: resources/corpus/formula-v1.json (published source; schema-valid durable independent release
      attestation pending)
    sha256: 11033679b018fdc9a192e954ef11089444a00a1d89c6279d3c192be9252cf42f
  - owner: kumwe/record-model
    version_or_commit: v0.1.0 at d7955e6beae2c3a9fa30052aea5ec3478af7dffc
    manifest_or_corpus: resources/conformance/document-profile-v1.json (published source; schema-valid durable independent
      release attestation pending)
    sha256: 84b6c2e55ae591c921536aa755cbb5a9a40a7a19847fe47415e55dce2614c177
  - owner: kumwe/reporting
    version_or_commit: v0.1.0 at d8cae56726170d39cd6e95e45b0ffcb085920849
    manifest_or_corpus: resources/conformance/report-materialization-v1.json (published source; schema-valid durable
      independent release attestation pending)
    sha256: 975116dc897a0bfdee4a08f9065eb10ccfec06a32f4eb93a48015b08af408c01
  - owner: kumwe/canonical-json
    version_or_commit: v0.1.1 at e7006a2580a49a1c8ab507b0d7b9c3403b4f9f58
    manifest_or_corpus: resources/corpus/v1.json (published source; schema-valid durable independent release attestation
      pending)
    sha256: 84d21b12e7a2bfd752356d9a6e664bcb332e209d19017e7634e7485a4fa4e250
  examined_dependencies:
  - Conversion v0.1.3 ExactDecimal, ExactDecimalArithmetic and six Money/Quantity rounding modes; no PHP source
    copied.
  - Computation portable-only Phase 1A version 0.1.1 at fc9d049f8b675c8e19fd1672d49b5e206c9ad52a is published. Its
    exact archive, API, capability and corpus facts remain recorded; schema-valid durable independent release attestation
    is pending.
  active_related_pull_requests: []
target:
  repository: https://github.com/kumwe/engine
  artifact_identity: CMake Kumwe::Engine
  canonical_namespace_or_abi: kumwe::engine / kumwe_engine_v1_
  branch: codex/abi1-release-readiness
  pull_request: https://github.com/kumwe/engine/pull/7
ownership:
  responsibility: Standalone exact decimal, compiled formula/document/report execution and generic canonical encoding/digests
    through one owned C ABI.
  non_responsibilities:
  - PHP/Zend/PIE, Composer, App services and runtime fallback.
  - Database, network, authorization, transactions, trust, rendering and delivery.
  allowed_dependency_ceiling:
  - C++20 standard library and exact language-neutral semantic corpus data.
  - Statically embedded maintained PCRE2 10.42 source closure and licensed Unicode 17 casing / 15.1 NFC data; exact
    hashes, no build downloads or system selection.
  implementation_owner: kumwe/engine
  next_consumer: kumwe/kumwe-engine
  public_manifests:
  - path: "resources/abi-manifest.json"
    sha256: "40e27d2a8e7b8f619274e1bce2525a220d704f5e86a4e46f68d53fc6dafd4fb9"
  - path: "resources/abi-symbols.txt"
    sha256: "62cf59b8d6feb369f0d801b59b69796ea569570177e02b903c0bec6b01e04d21"
  - path: "resources/capabilities.json"
    sha256: "37cb2409bf96234993cf7ff2f2b8a46d17cb699aac7eb47374bf9b0a1811083f"
  - path: "resources/contracts.json"
    sha256: "83e75d028b3d7159ffc5fe494269cb68abfda8b1c7c551a19854d218c7907c5c"
  - path: "tests/ownership.json"
    sha256: "2a8b54da277777706b6c65430cbf08d4e24b28b313a57e2f4f29212929aae4e3"
  - path: "include/kumwe/engine/engine.h"
    sha256: "d7ebdaa0c03bca629a5370ebbcdd6297ea6e99afa0229c9c13c2907676a94b2f"
  intentionally_excluded:
  - No App classes, dependency adoption or test removal.
  - No PHP binding implementation is owned here; the separate kumwe/kumwe-engine candidate consumes this exact source.
framework_php: null
native_cpp:
  cpp_namespace: kumwe::engine
  cmake_targets:
  - Kumwe::Engine
  public_headers:
  - include/kumwe/engine/engine.h
  cpp_api:
    classes: []
    functions: []
  c_abi:
    major: 1
    symbol_prefix: kumwe_engine_v1_
    headers:
    - include/kumwe/engine/engine.h
    functions:
    - kumwe_engine_v1_buffer_release
    - kumwe_engine_v1_buffer_view
    - kumwe_engine_v1_capabilities
    - kumwe_engine_v1_decimal_batch
    - kumwe_engine_v1_canonical
    - kumwe_engine_v1_compile
    - kumwe_engine_v1_execute
    - kumwe_engine_v1_plan_describe
    - kumwe_engine_v1_plan_release
    - kumwe_engine_v1_cancellation_create
    - kumwe_engine_v1_cancellation_request
    - kumwe_engine_v1_cancellation_release
    structs:
    - 'kumwe_engine_v1_view: naturally aligned 24 bytes on supported 64-bit targets; struct_size 24..4096 and ABI
      1.'
    enums_and_codes:
    - 0 success; 1 invalid_input; 2 unsupported_version; 3 incompatible_capability; 4 incompatible_corpus; 5 invalid_program;
      6 exhausted_limit; 7 cancelled; 8 internal_failure.
    - ABI 1 layout/status/lifetimes are frozen; initial fixed-header dynamic client passes all 295 assertions.
    opaque_handles:
    - name: kumwe_engine_v1_buffer
      create_functions:
      - kumwe_engine_v1_capabilities
      - kumwe_engine_v1_decimal_batch
      use_functions:
      - kumwe_engine_v1_buffer_view
      release_function: kumwe_engine_v1_buffer_release
      ownership: Unique Engine-allocated immutable result; release consumes/nulls owner slot. Caller owns input
        and initializes output null. Foreign/copied/dangling handles are forbidden.
      thread_safety: Independent calls and live immutable reads are reentrant; release/access must not race.
    - name: kumwe_engine_v1_plan
      create_functions:
      - kumwe_engine_v1_compile
      use_functions:
      - kumwe_engine_v1_execute
      - kumwe_engine_v1_plan_describe
      release_function: kumwe_engine_v1_plan_release
      ownership: Unique immutable plan; owns normalized source independently from compile input.
      thread_safety: Concurrent execution allowed; no release/use race.
    - name: kumwe_engine_v1_cancellation
      create_functions:
      - kumwe_engine_v1_cancellation_create
      use_functions:
      - kumwe_engine_v1_cancellation_request
      - kumwe_engine_v1_execute
      release_function: kumwe_engine_v1_cancellation_release
      ownership: Unique sticky cancellation token, alive until every borrowing execution returns.
      thread_safety: Request may race execution; release may not race use.
  capabilities:
  - decimal-batch-draft/1
  - formula-draft/1
  - normalized-document-draft/1
  - normalized-preparation-draft/1
  - report-materialization-draft/1
  - kumwe-canonical-json/generic-v1
  corpora:
  - owner: kumwe/conversion
    version: v0.1.3 at e95d5633722929e77b73005f8c44cfe4d99ac8c3; attestation pending
    profile: decimal-v1.tsv existing Conversion exact decimal semantics
    sha256: 635db251898707828e24f12b1abb672273552f5f633186a725cc9f50ac08140c
  limits_and_errors:
  - Precision 1..65, scale 0..precision, literal at most 68 bytes.
  - Input/output at most 1 MiB; count 1..4096; deterministic work 1..1000000000 logical units.
  - Each row costs 512, plus 4356 for multiplication before arithmetic; atomic refusal produces no partial batch.
  candidate_cross_build_expectations:
    source_archive_recipe: Commit reviewed source; bash tools/source-archive.sh; exact commit/tree/archive identity
      must be recorded externally.
    network_free_build: true
    required_extension_smoke_surface:
    - Thin Zend capability/ABI/corpus handshake, all supported coarse calls, bounds, ownership and exception cleanup.
    required_platform_and_php_matrix:
    - 'Engine standalone CMake: Linux x86_64 GCC/Clang and macOS ARM64 Clang. Binding: PHP 8.5 NTS Linux x86_64
      only, with exact patch/build tuple recorded.'
    required_abi_capability_corpus_checks:
    - All five completed modules, exact verified semantic inputs, frozen ABI/header/exports, cross-layer corpus
      and lifecycle evidence.
    attestation_schema: kumwe-engine-candidate-attestation/v1
    attestation_storage: External immutable CI/check artifact outside both tested trees, linked from Engine PR;
      no passing attestation exists yet.
    publishing_permitted: false
php_extension: null
tests:
  moved_or_added:
  - Engine-owned behavior/boundary, shared semantic corpus replay, C ABI, 10000 lifecycle cycles, concurrent reads,
    plain C, archive/install consumers, architecture/export checks, fault seeds and sanitizer/fuzz CI.
  remain_in_app_or_consumer:
  - App retains current execution tests until verified Computation cutover, then composition/security/storage/provisioning/recovery/acceptance
    and measured end-to-end performance.
  - Binding owns PHP/Zend lifecycle and cross-layer marshalling/corpus tests.
  split_tests: []
  prohibited_duplicates:
  - No PHP oracle or App implementation is copied into Engine.
  corpora:
  - corpus/decimal/decimal-v1.tsv
documentation:
  charter: CHARTER.md
  readme: README.md
  public_api: docs/abi.md
  architecture: docs/architecture.md
  integration_or_consumer: docs/consumer.md
  examples:
  - tests/consumer/main.c
  - tests/support.hpp
  changelog_record: CHANGELOG.md / Unreleased
release_expectations:
  version_policy: No release before candidate and immutable-source release gates pass. First App-eligible Engine
    release requires exactly identified 1.0.0 and frozen ABI 1 after all v2 gates.
  expected_artifact_types:
  - CMake source archive
  - Checksums
  - SPDX SBOM
  - Signed provenance
  - External candidate and stable release attestations
  required_checks:
  - E1 formula/document and E2 report/canonical are implemented; verify E3 hardening/freeze on the final source.
  - Verified immutable semantic releases and corpora, including the separate portable-only Computation Phase 1A
    contract baseline before the first stable Engine release.
  - Native behavior/boundary/conformance/fuzz/sanitizer/lifecycle/ABI/consumer/benchmark/supply-chain gates.
  - Non-publishing exact Engine/extension candidate cross-build before ready-for-review status.
  required_registry_or_installer: null
  required_external_attestation: true
next_task:
  phase_name: Verify final ABI 1 source, semantic release receipts and exact non-publishing binding cross-build.
  permitted_only_when:
  - Draft implementation uses reviewed owner semantics and exact corpus/source identity.
  - Candidate cross-build and release readiness wait for all five modules plus verified immutable semantic releases.
  consumer_repository: https://github.com/kumwe/engine
  dependency_or_native_change: Accept the final independent candidate attestation, publish and independently verify
    immutable Engine 1.0.0, then re-embed that exact release in the binding. App integration remains a later task.
  namespace_or_api_replacements: []
  files_to_update:
  - src
  - tests
  - resources/contracts.json
  - resources/capabilities.json
  - MIGRATION-HANDOFF.md
  files_to_remove: []
  tests_to_remove: []
  tests_to_retain_or_add:
  - Retain all five kernel corpora and ABI/ownership/bounds tests in Engine; downstream binding tests cover PHP-specific
    behavior.
  di_or_provisioning_changes: []
  capability_index_changes: []
  changelog_and_evidence_changes:
  - CHANGELOG.md; NRM-2026-013 remains enabling evidence with completion_claim false.
  verification_commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  - cmake --build build --parallel 4
  - ctest --test-dir build --output-on-failure
  - node tools/check-manifests.mjs build
  - bash tools/check-archive.sh
  - bash tools/check-fault-seeds.sh
concurrency:
  likely_conflict_files:
  - resources/contracts.json
  - resources/capabilities.json
  - include/kumwe/engine/engine.h
  - MIGRATION-HANDOFF.md
  related_migrations:
  - KUMWE-MIG-2026-008
  - KUMWE-MIG-2026-010
  ownership_conflicts: []
  integration_train: null
  resolution_rule: semantic-preservation
governance:
  roadmap_source_sha256: a202155ef1a65f5ab293d4f8397ebf4ac430db7f1e877c776bbe7851e6fe18d8
  roadmap_refs: []
  non_roadmap_refs:
  - NRM-2026-013
  - NRM-2026-042
  - NRM-2026-044
  completion_claim: false
decisions:
- All five kernels have implementation and corpus coverage. ABI 1 header, layout, status and lifetime behavior are
  frozen; the fixed historical C11 client runs against the current shared library.
- Preserve exact semantic owners and report all whole-boundary performance results honestly, including slower native
  workloads. Production App workload acceptance is a later independent step.
- The immutable source publisher requires exact default-branch quality success, stable-source admission and verified
  GitHub OIDC provenance; it never creates an independent candidate or release attestation.
blockers:
- Final schema-valid durable semantic owner and portable Computation release attestations must be recorded before
  stable source admission.
- Final exact-source native and binding checks plus independent candidate archive verification precede release review.
- Published Engine and binding releases must be independently verified before stable native Composer consumption.
  No App integration is performed here.
---

# Native implementation handoff

## Migration/implementation summary

Engine implements exact decimal batches, immutable formula plans, normalized document preparation/computation/validation, typed value and source-instance handling, normalized report materialization including converted values, and GenericV1 canonical encoding with streaming SHA256. This standalone delivery serves NRM-2026-013/042/044. App source, adapters, configuration and tests remain with their existing owners.

## Public API and responsibility

[ABI contract](docs/abi.md), [public header](include/kumwe/engine/engine.h), [ABI manifest](resources/abi-manifest.json) and [capabilities](resources/capabilities.json) define all twelve ABI 1 exports, status codes, handle ownership and bounded transport. C++ implementation types are private. The original fixed-header C11 client exercises the current shared library without compiling against the new public header. [Architecture](docs/architecture.md) defines the source and dependency boundaries.

## Capability reuse/semantic input review

[Contracts matrix](resources/contracts.json) identifies each semantic owner's exact immutable source, corpus digest and external release evidence. Corpus bytes remain owner-defined; profile strings retain their historical identities even when they contain draft or 0.0.0 tokens. The separate portable-only Computation Phase 1A is a stable-release prerequisite; later native Computation cannot substitute for it. PCRE2 is statically embedded at its reviewed maintained source commit with all applicable backports; ARM64 uses scalar JIT because the legacy SIMD fixes are not backportable. Unicode input and license digests are committed. No host PCRE2/ICU selection or build download is allowed.

## Consumer inventory

The binding consumes the exact exported Engine source archive and links the C ABI statically. Standalone CMake consumers use Kumwe::Engine and the installed C header. Computation owns PHP semantic adapters, and App owns authority, persistence, reference resolution, deployment provisioning and business-runtime cutover. The unchanged App/SDK benchmark oracle is external measurement input and is never imported into Engine runtime.

## Test ownership

Engine owns semantic corpus replay, C ABI layout/export/fixed-client checks, ownership, deterministic budgets, hostile/refusal recovery, threads, sanitizers, fuzz seeds, reproducible source closure and installed CMake consumers. Binding owns PHP-specific marshalling/lifetimes and cross-layer tests. App retains its current tests pending a separately authorized integration. The benchmark compares complete boundary calls and reports all measured results, including slower operations; it claims no automatic production acceleration.

## Next-task execution notes

Follow [release procedure](docs/releasing.md). Replace provisional semantic references only with actual independently verified immutable releases and schema-valid durable evidence. Run all exact-head native quality lanes, then embed that exact candidate in the separate binding and complete its PHP/sanitizer/offline PIE/whole-boundary jobs. Store the independent candidate attestation outside both source trees. After accepted candidate verification, publish Engine from the reviewed merged source, independently verify its actual assets and provenance, and re-embed those exact release bytes in the binding for final stable qualification. App integration is not part of this task.

## Drift check

Manifest/corpus locks, ABI header hashes, complete SPDX source inventory and deterministic raw/published archive hashes detect changed inputs. Frozen ABI tests compile from the preserved original client header. New source requires a new exact candidate cross-build and external record; current-source records cannot inherit passing status from a predecessor commit. Released tags and assets are immutable; corrections require a new release. Owner corpus or API changes are routed through their owning package and a new reviewed Engine release.

## Validation recipe and observed local results

Run the frontmatter verification commands and [release procedure](docs/releasing.md). Local release builds pass all 23 CTest cases and manifest/ownership checks, including the fixed 295-assertion ABI client. Hosted predecessor qualification passed Linux GCC/Clang, ARM64 Clang, fault/consumer and thread-sanitizer lanes. The binding predecessor completed all six whole-boundary workload families with 176 matrix cases, 48 capacity and 12 allocation probes. These are repair evidence; the final exact-source workflow, complete toolchain tuples, archives, corpus/handoff digests and independent attestations remain external and must be regenerated after any source change. The final archive CMake consumer and PIE installation run in actual network namespaces with networking disabled.
