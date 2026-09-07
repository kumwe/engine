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
    - "Computation reviewed native-boundary-draft.md and boundary-review.md; no compiler/executor capability claimed."
  active_related_pull_requests:
    - "https://github.com/kumwe/conversion/pull/5"
target:
  repository: "https://github.com/kumwe/engine"
  artifact_identity: "CMake Kumwe::Engine"
  canonical_namespace_or_abi: "kumwe::engine / kumwe_engine_v1_"
  branch: "agent/native-kernels-v2"
  pull_request: "https://github.com/kumwe/engine/pull/1"
ownership:
  responsibility: "Standalone deterministic native execution; this E0/E1 slice implements exact decimal batching only."
  non_responsibilities:
    - "PHP/Zend/PIE, Composer, App services and runtime fallback."
    - "Database, network, authorization, transactions, trust, rendering and delivery."
  allowed_dependency_ceiling:
    - "C++20 standard library and exact language-neutral semantic corpus data."
  implementation_owner: "kumwe/engine"
  next_consumer: "kumwe/kumwe-engine"
  public_manifests:
    -
      path: "resources/abi-symbols.txt"
      sha256: "62cf59b8d6feb369f0d801b59b69796ea569570177e02b903c0bec6b01e04d21"
    -
      path: "resources/capabilities.json"
      sha256: "84ab3e2cce2aa2519f9804b269d1fb36db8646cae0e0ee3539442771335b44fd"
    -
      path: "resources/contracts.json"
      sha256: "e2c8b257e0f2f4cc52f3c76d3a9892b6759d67a050a7dd4ed99701e9e7fe366f"
    -
      path: "tests/ownership.json"
      sha256: "39ae37fa8e07b3f213f900f5cd0553c25814b4b45434d4bcd0ff14364f36705f"
    -
      path: "include/kumwe/engine/engine.h"
      sha256: "ec031a4636a87ebc80e653513df1a8db228efc1d500c25c122702c69389280ee"
  intentionally_excluded:
    - "No App classes, dependency adoption or test removal."
    - "No VM, document, report, canonical, cache, compiler, cancellation or PHP-binding implementation is advertised."
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
  capabilities:
    - "decimal-batch-draft/1"
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
      - "Future thin Zend capability/ABI/corpus handshake, all supported coarse calls, bounds, ownership and exception cleanup."
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
  phase_name: "Continue Engine Phase 1 E1 formula/document implementation; later run the separate exact-candidate cross-build."
  permitted_only_when:
    - "Draft implementation uses reviewed owner semantics and exact corpus/source identity."
    - "Candidate cross-build and release readiness wait for all five modules plus verified immutable semantic releases."
  consumer_repository: "https://github.com/kumwe/engine"
  dependency_or_native_change: "Add complete reviewed formula/document native execution before report/canonical expansion; freeze the full C ABI only at E3."
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
  completion_claim: false
decisions:
  - "Root independent pre-code boundary review approved the draft exact-decimal slice on 2026-09-07; full E1/candidate remains unclaimed."
  - "Preserve exact semantic owners and never advertise unavailable native operations."
  - "No production publishing workflow is supplied for an incomplete Engine."
blockers:
  - "Full formula/document/report/canonical modules and shared immutable plan infrastructure remain to implement."
  - "Exact verified semantic releases, full E3 gates and non-publishing PHP candidate cross-build are not yet available."
  - "No stable Engine release, extension binding, App cutover or performance objective is claimed."
---

# Native draft handoff

This reviewable E0/E1 decimal slice is not a release candidate or App adoption.
The five-module architecture, exact test ownership and downstream release barriers remain mandatory.

## Runtime candidate NRM-2026-042

The candidate implements immutable formula plans, normalized scalar document computation and ordered findings,
normalized report materialization, and GenericV1 canonical encoding with streaming SHA256. The public ABI
owns plans, output buffers and cooperative cancellation. Exact semantic corpora and architecture/lifetime gates
remain mandatory. The contract matrix explicitly identifies unimplemented document codec/validator and report
converted-formula profiles; this candidate is not a completed acceleration release or an App cutover.
