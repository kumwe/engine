# Kumwe Engine charter

One C++20 engine owns deterministic native execution, explicit C ABI memory ownership,
conformance replay, fuzzing, sanitizers and diagnostic measurement. Its approved architecture
contains exact decimal, compiled definition VM, whole-document batch, report and canonical
streaming modules, sharing bounded values, plans and buffers.

The development candidate implements all five modules against frozen owner semantics, including
typed normalized document values, preparation, computed-field normalization and ordered findings.
It does not claim an ABI freeze, stable release or measured App performance improvement.

Semantic meaning belongs to the framework package owning each contract. Exact published source
coordinates and matching corpus hashes are recorded in resources/contracts.json; independent
release verification remains mandatory before production release. Engine has no PHP namespace, Composer/PHP/Zend/PIE dependency,
App service, database, network, authorization, transaction, trust, rendering or delivery behavior.

The sole binding surface is C with the `kumwe_engine_v1_` prefix. Internal C++ uses
`kumwe::engine`. No runtime oracle, fallback, FFI, subprocess integration or callback is present.
The diagnostic executable is a test tool. Installed consumers receive a static CMake library and
public C header; the internal C++ implementation has no stability promise.

Development targets are 64-bit Linux GCC/Clang and macOS AppleClang, tested in CI. Windows,
32-bit targets and stable binary ABI compatibility are not claimed. Linux TSan covers the shared
immutable-plan test; the separate PHP binding runs its own candidate compatibility matrix. Every supported source consumer builds offline from the pinned dependency source closure.

All native behavior, boundary, conformance, architecture, lifecycle and robustness tests belong
here. Binding tests own PHP marshalling/lifecycle parity; App retains only composition,
authority, provisioning/recovery, acceptance and measured full-path performance evidence.

First App-eligible release is Engine 1.0.0 with frozen ABI 1, all five modules and the complete
v2 release gates. Publication is separate from the development candidate. Modules
remain in one repository until an independent consumer, independent cadence, stable contract and
measured benefit justify a split.

The existing bounded pattern validator requires one narrow reviewed third-party dependency: statically embedded, source-pinned maintained PCRE2 10.42 with all required upstream backports. It owns regular-expression matching only. No system-library selection or network build retrieval is allowed. See docs/document-validators.md and resources/pcre2-source.json.

Computed-field casing uses pinned Unicode 17 data; NFC uses Unicode 15.1, matching the frozen PHP
8.5/ICU 74 owner. Generated internal tables are reproducible from licensed source data. These
normalizers are private document operations; no general text-processing API is exported.
