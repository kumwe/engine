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
      version_or_commit: "v0.1.3 at e95d5633722929e77b73005f8c44cfe4d99ac8c3"
      manifest_or_corpus: "resources/conformance/decimal-v1.tsv (published source; independent release attestation pending)"
      sha256: "635db251898707828e24f12b1abb672273552f5f633186a725cc9f50ac08140c"
    -
      owner: "kumwe/business-definition"
      version_or_commit: "v0.1.0 at f3b86f8af1469066637fd1b4b8f71c4df634adf8"
      manifest_or_corpus: "resources/corpus/formula-v1.json (published source; independent release attestation pending)"
      sha256: "11033679b018fdc9a192e954ef11089444a00a1d89c6279d3c192be9252cf42f"
    -
      owner: "kumwe/record-model"
      version_or_commit: "v0.1.0 at d7955e6beae2c3a9fa30052aea5ec3478af7dffc"
      manifest_or_corpus: "resources/conformance/document-profile-v1.json (published source; independent release attestation pending)"
      sha256: "84b6c2e55ae591c921536aa755cbb5a9a40a7a19847fe47415e55dce2614c177"
    -
      owner: "kumwe/reporting"
      version_or_commit: "v0.1.0 at d8cae56726170d39cd6e95e45b0ffcb085920849"
      manifest_or_corpus: "resources/conformance/report-materialization-v1.json (published source; independent release attestation pending)"
      sha256: "975116dc897a0bfdee4a08f9065eb10ccfec06a32f4eb93a48015b08af408c01"
    -
      owner: "kumwe/canonical-json"
      version_or_commit: "v0.1.1 at e7006a2580a49a1c8ab507b0d7b9c3403b4f9f58"
      manifest_or_corpus: "resources/corpus/v1.json (published source; independent release attestation pending)"
      sha256: "84d21b12e7a2bfd752356d9a6e664bcb332e209d19017e7634e7485a4fa4e250"
  examined_dependencies:
    - "Conversion v0.1.3 ExactDecimal, ExactDecimalArithmetic and six Money/Quantity rounding modes; no PHP source copied."
    - "Computation portable program, batch, findings and capability contracts; exact candidate compiler/executor and canonical adapter verified through the Zend binding. This candidate evidence does not establish a published, independently verified portable Phase 1A baseline."
  active_related_pull_requests: []
target:
  repository: "https://github.com/kumwe/engine"
  artifact_identity: "CMake Kumwe::Engine"
  canonical_namespace_or_abi: "kumwe::engine / kumwe_engine_v1_"
  branch: "codex/extraction-readiness-20260907"
  pull_request: "https://github.com/kumwe/engine/pull/7"
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
      path: "resources/abi-manifest.json"
      sha256: "40e27d2a8e7b8f619274e1bce2525a220d704f5e86a4e46f68d53fc6dafd4fb9"
    -
      path: "resources/abi-symbols.txt"
      sha256: "62cf59b8d6feb369f0d801b59b69796ea569570177e02b903c0bec6b01e04d21"
    -
      path: "resources/capabilities.json"
      sha256: "37cb2409bf96234993cf7ff2f2b8a46d17cb699aac7eb47374bf9b0a1811083f"
    -
      path: "resources/contracts.json"
      sha256: "2eab83eb528a2a13cb5a151e494e937ae71e61a426fef57564eff45e489d2193"
    -
      path: "tests/ownership.json"
      sha256: "7fb2450f345d7546b8ff1f425bf9fcc4dc8efb5b25a340fad513cf13c4370f04"
    -
      path: "include/kumwe/engine/engine.h"
      sha256: "d7ebdaa0c03bca629a5370ebbcdd6297ea6e99afa0229c9c13c2907676a94b2f"
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
      - "ABI 1 layout/status/lifetimes are frozen; initial fixed-header dynamic client passes all 295 assertions."
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
      version: "v0.1.3 at e95d5633722929e77b73005f8c44cfe4d99ac8c3; attestation pending"
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
  version_policy: "No release before candidate and immutable-source release gates pass. First App-eligible Engine release requires exactly identified 1.0.0 and frozen ABI 1 after all v2 gates."
  expected_artifact_types:
    - "CMake source archive"
    - "Checksums"
    - "SPDX SBOM"
    - "Signed provenance"
    - "External candidate and stable release attestations"
  required_checks:
    - "E1 formula/document and E2 report/canonical are implemented; verify E3 hardening/freeze on the final source."
    - "Verified immutable semantic releases and corpora, including the separate portable-only Computation Phase 1A contract baseline before the first stable Engine release."
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
    - "Retain all five kernel corpora and ABI/ownership/bounds tests in Engine; downstream binding tests cover PHP-specific behavior."
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
    - "NRM-2026-044"
  completion_claim: false
decisions:
  - "All five kernels have implementation and corpus coverage; first stable release remains gated by independent verification and performance acceptance."
  - "Preserve exact semantic owners and never advertise unavailable native operations."
  - "No production publishing workflow is supplied before the accepted immutable-release gate."
blockers:
  - "Computation Phase 1A portable-only baseline release remains unresolved: published v0.2.0/v0.2.1 and the 0.3.0 adapter candidate require the native extension; none supplies the required independent pre-Engine contract release and attestation."
  - "Final exact-head native checks and independent binding archive verification precede release review."
  - "Exact published semantic source/corpus bytes are matched; independent owner release attestations, ABI freeze and final candidate cross-build acceptance remain outstanding."
  - "No stable Engine or extension release, App cutover or performance objective is claimed."
---

# Native implementation handoff

This reviewable native implementation candidate is not a stable release or App adoption.
The five-module architecture, exact test ownership and downstream release barriers remain mandatory.

## Runtime candidate NRM-2026-042

The candidate implements immutable formula plans, complete normalized document preparation/computation/
validation, typed value and source-instance handling, normalized report materialization including converted
values, and GenericV1 canonical encoding with streaming SHA256. Patched PCRE2 and pinned Unicode data
preserve the owner behavior. The public ABI owns plans, output buffers and cooperative cancellation.
Exact corpus, architecture, lifecycle and independent binding gates remain mandatory. Host normalization,
authority, persistence, provisioning and App cutover remain with their named owners.
