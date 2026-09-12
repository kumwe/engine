---
schema: kumwe-package-release-record/v1
artifact_kind: native_cpp
migration_id: KUMWE-MIG-2026-011
change_set: KUMWE-CS-2026-011
source:
  app:
    repository: https://github.com/kumwe/app
    baseline_commit: null
    examined_paths: []
    old_namespace_roots: []
    capability_index_sha256: null
  semantic_inputs:
  - owner: kumwe/conversion
    version_or_commit: v0.1.5 at b291f3a31314644fd88150dc9a9e911fe0617fe7
    manifest_or_corpus: resources/conformance/decimal-v1.tsv (published source; schema-valid durable independent
      release attestation recorded in resources/contracts.json)
    sha256: 635db251898707828e24f12b1abb672273552f5f633186a725cc9f50ac08140c
  - owner: kumwe/business-definition
    version_or_commit: v0.1.2 at 1cb59229f311067e8cfdd773a34e34cfeb501791
    manifest_or_corpus: resources/corpus/formula-v1.json (published source; schema-valid durable independent release
      attestation recorded in resources/contracts.json)
    sha256: 11033679b018fdc9a192e954ef11089444a00a1d89c6279d3c192be9252cf42f
  - owner: kumwe/record-model
    version_or_commit: v0.1.3 at 8d20cf75ee6ab915102bbf6a17567087971e9dfe
    manifest_or_corpus: resources/conformance/document-profile-v1.json (published source; schema-valid durable
      independent release attestation recorded in resources/contracts.json)
    sha256: 84b6c2e55ae591c921536aa755cbb5a9a40a7a19847fe47415e55dce2614c177
  - owner: kumwe/reporting
    version_or_commit: v0.1.3 at b521b3113bd97afc2de6ab74fcb71a36fe7d0890
    manifest_or_corpus: resources/conformance/report-materialization-v1.json (published source; schema-valid durable
      independent release attestation recorded in resources/contracts.json)
    sha256: 975116dc897a0bfdee4a08f9065eb10ccfec06a32f4eb93a48015b08af408c01
  - owner: kumwe/canonical-json
    version_or_commit: v0.1.1 at e7006a2580a49a1c8ab507b0d7b9c3403b4f9f58
    manifest_or_corpus: resources/corpus/v1.json (published source; schema-valid durable independent release attestation
      recorded in resources/contracts.json)
    sha256: 84d21b12e7a2bfd752356d9a6e664bcb332e209d19017e7634e7485a4fa4e250
  examined_dependencies:
  - Conversion v0.1.5 ExactDecimal, ExactDecimalArithmetic and six Money/Quantity rounding modes; unchanged corpus,
    no PHP source copied.
  - Computation portable contracts version 0.1.1 at fc9d049f8b675c8e19fd1672d49b5e206c9ad52a is independently
    verified. Its exact archive, canonical manifests, portable corpus and schema-valid durable release attestation
    are recorded in resources/contracts.json.
target:
  repository: https://github.com/kumwe/engine
  artifact_identity: CMake Kumwe::Engine
  canonical_namespace_or_abi: kumwe::engine / kumwe_engine_v1_
ownership:
  responsibility: Standalone exact decimal, compiled formula/document/report execution and generic canonical encoding/digests
    through one owned C ABI.
  non_responsibilities:
  - PHP/Zend/PIE, Composer, App services and runtime fallback.
  - Database, network, authorization, transactions, trust, rendering and delivery.
  allowed_dependency_ceiling:
  - C++20 standard library and exact language-neutral semantic corpus data.
  - Statically embedded maintained PCRE2 10.42 source closure and licensed Unicode 17 casing / 15.1 NFC data;
    exact hashes, no build downloads or system selection.
  implementation_owner: kumwe/engine
  next_consumer: kumwe/kumwe-engine
  public_manifests:
  - path: "resources/abi-manifest.json"
    sha256: "c34277b1bf5b74a61bcf5a78d1e6452c6c2525634789333c990298bdb70665c0"
  - path: "resources/abi-symbols.txt"
    sha256: "62cf59b8d6feb369f0d801b59b69796ea569570177e02b903c0bec6b01e04d21"
  - path: "resources/capabilities.json"
    sha256: "82f8b8c1efc1d5f38a945e319e51983af6b94d613d0310ce2843449805a1baf6"
  - path: "resources/contracts.json"
    sha256: "089f7e9a40f6da991654ca33937a864d6e28a5325b57b8da457c9d4492b818f0"
  - path: "tests/ownership.json"
    sha256: "2a8b54da277777706b6c65430cbf08d4e24b28b313a57e2f4f29212929aae4e3"
  - path: "include/kumwe/engine/engine.h"
    sha256: "d7ebdaa0c03bca629a5370ebbcdd6297ea6e99afa0229c9c13c2907676a94b2f"
  intentionally_excluded:
  - No App classes, dependency adoption or test removal.
  - No PHP binding implementation is owned here; kumwe/kumwe-engine consumes the exact published source archive.
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
    - 'kumwe_engine_v1_view: naturally aligned 24 bytes on supported 64-bit targets; struct_size 24..4096 and
      ABI 1.'
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
    version: 0.1.5 at b291f3a31314644fd88150dc9a9e911fe0617fe7; independently release-verified
    profile: decimal-batch-draft/1
    sha256: 635db251898707828e24f12b1abb672273552f5f633186a725cc9f50ac08140c
  - owner: kumwe/business-definition
    version: 0.1.2 at 1cb59229f311067e8cfdd773a34e34cfeb501791; independently release-verified
    profile: formula-draft/1
    sha256: 11033679b018fdc9a192e954ef11089444a00a1d89c6279d3c192be9252cf42f
  - owner: kumwe/record-model
    version: 0.1.3 at 8d20cf75ee6ab915102bbf6a17567087971e9dfe; independently release-verified
    profile: normalized-document-draft/1
    sha256: 84b6c2e55ae591c921536aa755cbb5a9a40a7a19847fe47415e55dce2614c177
  - owner: kumwe/reporting
    version: 0.1.3 at b521b3113bd97afc2de6ab74fcb71a36fe7d0890; independently release-verified
    profile: report-materialization-draft/1
    sha256: 975116dc897a0bfdee4a08f9065eb10ccfec06a32f4eb93a48015b08af408c01
  - owner: kumwe/canonical-json
    version: 0.1.1 at e7006a2580a49a1c8ab507b0d7b9c3403b4f9f58; independently release-verified
    profile: kumwe-canonical-json/generic-v1
    sha256: 84d21b12e7a2bfd752356d9a6e664bcb332e209d19017e7634e7485a4fa4e250
  - owner: kumwe/record-model
    version: 0.1.3 at 8d20cf75ee6ab915102bbf6a17567087971e9dfe; independently release-verified
    profile: normalized-preparation-draft/1
    sha256: 4eb1543929fcf5470eb8cae882127556cd57c02518109545483d8780266b561a
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
    - 'Engine standalone CMake: Linux x86_64 GCC/Clang and macOS ARM64 Clang. Binding: PHP 8.5 NTS and ZTS, Linux
      x86_64, with exact patch/build tuple recorded.'
    required_abi_capability_corpus_checks:
    - All five completed modules, exact recorded semantic inputs, frozen ABI/header/exports, cross-layer corpus
      and lifecycle evidence.
    attestation_schema: kumwe-engine-candidate-attestation/v1
    attestation_storage: GitHub OIDC build provenance attached to every published Engine release; no external
      candidate record gates publication.
    publishing_permitted: true
php_extension: null
tests:
  moved_or_added:
  - Engine-owned behavior/boundary, shared semantic corpus replay, C ABI, 10000 lifecycle cycles, concurrent reads,
    plain C, archive/install consumers, architecture/export checks, fault seeds and sanitizer/fuzz CI.
  remain_in_app_or_consumer:
  - Core owns composition, security, storage, provisioning, recovery, acceptance and measured end-to-end performance.
  - Binding owns PHP/Zend lifecycle and cross-layer marshalling/corpus tests.
  split_tests: []
  prohibited_duplicates:
  - No PHP oracle or App implementation is copied into Engine.
  corpora:
  - corpus/decimal/decimal-v1.tsv
  - corpus/canonical/generic-v1.json
  - corpus/definition/formula-v1.json
  - corpus/document/computed-normalization-v1.json
  - corpus/document/document-profile-v1.json
  - corpus/document/normalized-values-v1.json
  - corpus/document/preparation-v1.json
  - corpus/document/unicode-normalization-oracle-v1.json
  - corpus/document/validation-v1.json
  - corpus/document/validator-edges-v1.json
  - corpus/document/validator-extension-v1.json
  - corpus/reporting/materialization-v1.json
documentation:
  charter: CHARTER.md
  readme: README.md
  public_api: docs/abi.md
  architecture: docs/architecture.md
  integration_or_consumer: docs/consumer.md
  examples:
  - tests/consumer/main.c
  - tests/support.hpp
  changelog_record: CHANGELOG.md
release_expectations:
  version_policy: Every default-branch commit that passes the complete Native quality workflow is tagged and published
    as vMAJOR.MINOR.PATCH from the version declared in resources/capabilities.json; every change to released
    source declares its version, and the workflow completes any release an earlier run left unfinished. The binding is published under the same version (docs/releasing.md).
  expected_artifact_types:
  - CMake source archive
  - Checksums
  - SPDX SBOM
  - Signed provenance
  required_checks:
  - All five module corpora and frozen ABI checks must pass on the exact released source.
  - Exact published semantic-owner sources and corpora recorded in resources/contracts.json, with any independent
    verification receipts retained as metadata.
  - Native behavior/boundary/conformance/fuzz/sanitizer/lifecycle/ABI/consumer/benchmark/supply-chain gates.
  required_registry_or_installer: null
  required_external_attestation: false
consumer_contract:
  permitted_only_when:
  - Verify published archive checksums and GitHub OIDC build provenance.
  - Qualify the exact source, ABI, capabilities, corpora and supported platform tuple before deployment.
  consumer_repository: https://github.com/kumwe/kumwe-engine
  dependency_or_native_change: Embed the immutable Engine source archive in the PHP binding and publish both under the same version.
  namespace_or_api_replacements: []
  files_to_update: []
  files_to_remove: []
  tests_to_remove: []
  tests_to_retain_or_add:
  - Retain native semantic corpora, ABI, lifetime, bounds, sanitizer, fuzz and installed-consumer tests.
  - Core retains composition, authorization, persistence, provisioning, recovery and acceptance coverage.
  di_or_provisioning_changes: []
  capability_index_changes: []
  changelog_and_evidence_changes:
  - Release source records and independent verification bind exact commits and artifact digests.
  verification_commands:
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  - cmake --build build --parallel 4
  - ctest --test-dir build --output-on-failure
  - node tools/check-manifests.mjs build
  - bash tools/check-archive.sh
  - bash tools/check-fault-seeds.sh
governance:
  completion_claim: false
decisions:
- All five kernels have implementation and corpus coverage. ABI 1 header, layout, status and lifetime behavior
  are frozen; the fixed historical C11 client runs against the current shared library.
- Preserve exact semantic owners and report all whole-boundary performance results honestly, including slower
  native workloads. Production App workload acceptance is a later independent step.
- The release job publishes every default-branch commit that passes all quality lanes, with GitHub OIDC build
  provenance and no external attestation gate; it never moves a tag or replaces an asset.
blockers: []
---

# Native Engine release record

## Package contract

Engine owns bounded deterministic execution behind C ABI 1. See [Core contract](core-contract.md). The retained migration and change-set IDs identify existing independent attestations; they do not describe pending extraction work.

## Public API and responsibility

The [public header](../include/kumwe/engine/engine.h), [ABI](abi.md) and [manifest](../resources/abi-manifest.json) define the supported surface. Internal C++ types are private.

## Dependencies and semantic inputs

The [contracts matrix](../resources/contracts.json) pins semantic-owner sources, corpus digests and verification evidence. PCRE2 and Unicode source closures are pinned and built offline.

## Consumer contract

The PHP binding embeds the exact released source; standalone CMake consumers link Kumwe::Engine. Core owns authority, persistence, reference resolution, transactions, provisioning and recovery. Package publication does not establish Core integration or workload acceptance.

## Test ownership

Engine owns semantic replay, C ABI, frozen client, lifetime, bounds, concurrency, sanitizers, fuzzing and reproducible installed-consumer checks. The binding owns PHP-specific coverage.

## Consumer verification

Follow [release verification and automation](releasing.md). Verify immutable source checksums and provenance, then qualify the complete supported tuple before deployment. Roll back the entire known-good deployment tuple.

## Compatibility and drift

Public manifest hashes above remain executable verification inputs. ABI 1 symbols, statuses, layout and lifetime guarantees are frozen. Released tags and assets are immutable; changed source requires a new release.

## Validation

Run the verification commands above and the complete hosted quality workflow. CI evidence applies only to its tested commit and platform; performance measurements include slower workloads and do not imply production acceleration.
