---
schema: "kumwe-migration-handoff/v2"
artifact_kind: "native_cpp"
migration_id: "KUMWE-MIG-2026-011"
change_set: "KUMWE-CS-2026-011"
state: "draft_pr_open"
source:
  app:
    repository: "https://github.com/kumwe/app"
    baseline_commit: null
    examined_paths: []
    old_namespace_roots: []
    capability_index_sha256: null
  semantic_inputs:
    -
      owner: "kumwe/conversion"
      version_or_commit: "ef3f2bae09ddab839497b2d296141581d70ba059"
      manifest_or_corpus: "resources/conformance/decimal-v1.tsv (reviewed draft; behavior source v0.1.2 at 5ccea7f7dc4ebd11bc27b11da8573da42807c196)"
      sha256: "635db251898707828e24f12b1abb672273552f5f633186a725cc9f50ac08140c"
  examined_dependencies:
    - "Conversion v0.1.2 ExactDecimal, ExactDecimalArithmetic and six Money/Quantity rounding modes; no PHP source copied."
    - "Computation portable program, batch, findings and capability contracts; exact candidate compiler/executor and canonical adapter verified through the Zend binding."
  active_related_pull_requests:
    - "https://github.com/kumwe/conversion/pull/5"
target:
  repository: "https://github.com/kumwe/engine"
  artifact_identity: "CMake Kumwe::Engine"
  canonical_namespace_or_abi: "kumwe::engine / kumwe_engine_v1_"
  branch: "agent/native-kernels-v2"
  pull_request: "https://github.com/kumwe/engine/pull/2"
ownership:
  responsibility: "Standalone exact decimal, compiled formula/document/report execution and generic canonical encoding/digests through one owned C ABI."
  non_responsibilities:
    - "PHP/Zend/PIE, Composer, App services and runtime fallback."
    - "Database, network, authorization, transactions, trust, rendering and delivery."
  allowed_dependency_ceiling:
    - "C++20 standard library and exact language-neutral semantic corpus data."
    - "Statically embedded maintained PCRE2 10.42 source closure and licensed Unicode 17 casing / 15.1 NFC data; exact hashes, no build downloads or system selection."
  implementation_owner: "kumwe/engine"
  next_consumer: "kumwe/kumwe-engine"
  public_manifests:
    -
      path: "resources/abi-symbols.txt"
      sha256: "62cf59b8d6feb369f0d801b59b69796ea569570177e02b903c0bec6b01e04d21"
    -
      path: "resources/capabilities.json"
      sha256: "b51c3af0f26702d10c0bd7766a312614357c57298a93479d8f6274ee11c1ba8e"
    -
      path: "resources/contracts.json"
      sha256: "50c74cc2a70b05a73f973d012f1ea903a1ad65a74c551aa8703763404fcea782"
    -
      path: "tests/ownership.json"
      sha256: "04034d0c586e0427a5641c87c336575b8898fb2def48ab87b2d01c44ef70c73e"
    -
      path: "include/kumwe/engine/engine.h"
      sha256: "ec031a4636a87ebc80e653513df1a8db228efc1d500c25c122702c69389280ee"
  intentionally_excluded:
    - "No App classes, dependency adoption or test removal."
    - "No PHP binding implementation is owned here; the separate kumwe/kumwe-engine candidate consumes this exact source."
framework_php: null
native_cpp:
  cpp_namespace: "kumwe::engine"
  cmake_targets:
    - "Kumwe::Engine"
  public_headers:
    - "include/kumwe/engine/engine.h"
  cpp_api:
    classes: []
    functions: []
  c_abi:
    major: 1
    symbol_prefix: "kumwe_engine_v1_"
    headers:
      - "include/kumwe/engine/engine.h"
    functions:
      - "kumwe_engine_v1_buffer_release"
      - "kumwe_engine_v1_buffer_view"
      - "kumwe_engine_v1_capabilities"
      - "kumwe_engine_v1_decimal_batch"
      - "kumwe_engine_v1_canonical"
      - "kumwe_engine_v1_compile"
      - "kumwe_engine_v1_execute"
      - "kumwe_engine_v1_plan_describe"
      - "kumwe_engine_v1_plan_release"
      - "kumwe_engine_v1_cancellation_create"
      - "kumwe_engine_v1_cancellation_request"
      - "kumwe_engine_v1_cancellation_release"
    structs:
      - "kumwe_engine_v1_view: naturally aligned 24 bytes on supported 64-bit targets; struct_size 24..4096 and ABI 1."
    enums_and_codes:
      - "0 success; 1 invalid_input; 2 unsupported_version; 3 incompatible_capability; 4 incompatible_corpus; 5 invalid_program; 6 exhausted_limit; 7 cancelled; 8 internal_failure."
      - "ABI/layout/status vocabulary is a development proposal, not frozen ABI 1."
    opaque_handles:
      -
        name: "kumwe_engine_v1_buffer"
        create_functions:
          - "kumwe_engine_v1_capabilities"
          - "kumwe_engine_v1_decimal_batch"
        use_functions:
          - "kumwe_engine_v1_buffer_view"
        release_function: "kumwe_engine_v1_buffer_release"
        ownership: "Unique Engine-allocated immutable result; release consumes/nulls owner slot. Caller owns input and initializes output null. Foreign/copied/dangling handles are forbidden."
        thread_safety: "Independent calls and live immutable reads are reentrant; release/access must not race."
      -
        name: "kumwe_engine_v1_plan"
        create_functions: ["kumwe_engine_v1_compile"]
        use_functions: ["kumwe_engine_v1_execute", "kumwe_engine_v1_plan_describe"]
        release_function: "kumwe_engine_v1_plan_release"
        ownership: "Unique immutable plan; owns normalized source independently from compile input."
        thread_safety: "Concurrent execution allowed; no release/use race."
      -
        name: "kumwe_engine_v1_cancellation"
        create_functions: ["kumwe_engine_v1_cancellation_create"]
        use_functions: ["kumwe_engine_v1_cancellation_request", "kumwe_engine_v1_execute"]
        release_function: "kumwe_engine_v1_cancellation_release"
        ownership: "Unique sticky cancellation token, alive until every borrowing execution returns."
        thread_safety: "Request may race execution; release may not race use."
  capabilities:
    - "decimal-batch-draft/1"
    - "formula-draft/1"
    - "normalized-document-draft/1"
    - "normalized-preparation-draft/1"
    - "report-materialization-draft/1"
    - "kumwe-canonical-json/generic-v1"
  corpora:
    -
      owner: "kumwe/conversion"
      version: "draft source ef3f2bae09ddab839497b2d296141581d70ba059, not a release"
      profile: "decimal-v1.tsv existing Conversion exact decimal semantics"
      sha256: "635db251898707828e24f12b1abb672273552f5f633186a725cc9f50ac08140c"
  limits_and_errors:
    - "Precision 1..65, scale 0..precision, literal at most 68 bytes."
    - "Input/output at most 1 MiB; count 1..4096; deterministic work 1..1000000000 logical units."
    - "Each row costs 512, plus 4356 for multiplication before arithmetic; atomic refusal produces no partial batch."
  candidate_cross_build_expectations:
    source_archive_recipe: "Commit reviewed source; bash tools/source-archive.sh; exact commit/tree/archive identity must be recorded externally."
    network_free_build: true
    required_extension_smoke_surface:
      - "Thin Zend capability/ABI/corpus handshake, all supported coarse calls, bounds, ownership and exception cleanup."
    required_platform_and_php_matrix:
      - "Select and prove a maintainable PHP 8.5 production-Linux tuple matrix; declare NTS/ZTS only when each passes. No PHP tuple is claimed by this draft."
    required_abi_capability_corpus_checks:
      - "All five completed modules, exact verified semantic inputs, frozen ABI/header/exports, cross-layer corpus and lifecycle evidence."
    attestation_schema: "kumwe-engine-candidate-attestation/v1"
    attestation_storage: "External immutable CI/check artifact outside both tested trees, linked from Engine PR; no passing attestation exists yet."
    publishing_permitted: false
php_extension: null
tests:
  moved_or_added:
    - "Engine-owned behavior/boundary, shared semantic corpus replay, C ABI, 10000 lifecycle cycles, concurrent reads, plain C, archive/install consumers, architecture/export checks, fault seeds and sanitizer/fuzz CI."
  remain_in_app_or_consumer:
    - "App retains current execution tests until verified Computation cutover, then composition/security/storage/provisioning/recovery/acceptance and measured end-to-end performance."
    - "Binding owns PHP/Zend lifecycle and cross-layer marshalling/corpus tests."
  split_tests: []
  prohibited_duplicates:
    - "No PHP oracle or App implementation is copied into Engine."
  corpora:
    - "corpus/decimal/decimal-v1.tsv"
documentation:
  charter: "CHARTER.md"
  readme: "README.md"
  public_api: "docs/abi.md"
  architecture: "docs/architecture.md"
  integration_or_consumer: "docs/consumer.md"
  examples:
    - "tests/consumer/main.c"
    - "tests/support.hpp"
  changelog_record: "CHANGELOG.md / Unreleased"
release_expectations:
  version_policy: "No release from this incomplete draft. First App-eligible Engine release requires exactly identified 1.0.0 and frozen ABI 1 after all v2 gates."
  expected_artifact_types:
    - "CMake source archive"
    - "Checksums"
    - "SPDX SBOM"
    - "Signed provenance"
    - "External candidate and stable release attestations"
  required_checks:
    - "Complete E1 formula/document, E2 report/canonical and E3 hardening/freeze."
    - "Verified immutable semantic releases and corpora."
    - "Native behavior/boundary/conformance/fuzz/sanitizer/lifecycle/ABI/consumer/benchmark/supply-chain gates."
    - "Non-publishing exact Engine/extension candidate cross-build before ready-for-review status."
  required_registry_or_installer: null
  required_external_attestation: true
next_task:
  phase_name: "Verify the completed native profiles and final exact source archive through native and binding CI."
  permitted_only_when:
    - "Draft implementation uses reviewed owner semantics and exact corpus/source identity."
    - "Candidate cross-build and release readiness wait for all five modules plus verified immutable semantic releases."
  consumer_repository: "https://github.com/kumwe/engine"
  dependency_or_native_change: "Verify completed native profiles against semantic owners and the exact binding; freeze the full C ABI only at E3."
  namespace_or_api_replacements: []
  files_to_update:
    - "src"
    - "tests"
    - "resources/contracts.json"
    - "resources/capabilities.json"
    - "MIGRATION-HANDOFF.md"
  files_to_remove: []
  tests_to_remove: []
  tests_to_retain_or_add:
    - "Retain decimal corpus/ABI tests; add every future kernel behavior, boundary, semantic corpus and performance test in Engine."
  di_or_provisioning_changes: []
  capability_index_changes: []
  changelog_and_evidence_changes:
    - "CHANGELOG.md; NRM-2026-013 remains enabling evidence with completion_claim false."
  verification_commands:
    - "cmake -S . -B build -DCMAKE_BUILD_TYPE=Release"
    - "cmake --build build --parallel 4"
    - "ctest --test-dir build --output-on-failure"
    - "node tools/check-manifests.mjs build"
    - "bash tools/check-archive.sh"
    - "bash tools/check-fault-seeds.sh"
concurrency:
  likely_conflict_files:
    - "resources/contracts.json"
    - "resources/capabilities.json"
    - "include/kumwe/engine/engine.h"
    - "MIGRATION-HANDOFF.md"
  related_migrations:
    - "KUMWE-MIG-2026-008"
    - "KUMWE-MIG-2026-010"
  ownership_conflicts: []
  integration_train: null
  resolution_rule: "semantic-preservation"
governance:
  roadmap_source_sha256: "a202155ef1a65f5ab293d4f8397ebf4ac430db7f1e877c776bbe7851e6fe18d8"
  roadmap_refs: []
  non_roadmap_refs:
    - "NRM-2026-013"
    - "NRM-2026-042"
  completion_claim: false
decisions:
  - "Root independent pre-code boundary review approved the draft exact-decimal slice on 2026-09-07; full E1/candidate remains unclaimed."
  - "Preserve exact semantic owners and never advertise unavailable native operations."
  - "No production publishing workflow is supplied for an incomplete Engine."
blockers:
  - "Final exact-head native checks and independent binding archive verification precede release review."
  - "Exact verified semantic releases, full E3 gates and non-publishing PHP candidate cross-build are not yet available."
  - "No stable Engine release, extension binding, App cutover or performance objective is claimed."
---

# Native draft handoff

This reviewable native implementation candidate is not a stable release or App adoption.
The five-module architecture, exact test ownership and downstream release barriers remain mandatory.

## Runtime candidate NRM-2026-042

The candidate implements immutable formula plans, complete normalized document preparation/computation/
validation, typed value and source-instance handling, normalized report materialization including converted
values, and GenericV1 canonical encoding with streaming SHA256. Patched PCRE2 and pinned Unicode data
preserve the owner behavior. The public ABI owns plans, output buffers and cooperative cancellation.
Exact corpus, architecture, lifecycle and independent binding gates remain mandatory. Host normalization,
authority, persistence, provisioning and App cutover remain with their named owners.
