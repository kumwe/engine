# Kumwe Engine charter

One C++20 engine owns deterministic native execution, explicit C ABI memory ownership,
conformance replay, fuzzing, sanitizers and diagnostic measurement. Its approved architecture
contains exact decimal, compiled definition VM, whole-document batch, report and canonical
streaming modules, sharing bounded values, plans and buffers.

This first development slice implements exact decimal batching against Conversion's existing
semantics. It is an E0/E1 draft, not an Engine candidate, ABI freeze, stable release or App
performance improvement claim. Formula/document compilation must follow before reporting and
canonical expansion. Unsupported modules have no pretend implementation or advertised capability.

Semantic meaning belongs to the framework package owning each contract. Draft source coordinates
permit development only; verified immutable semantic releases and corpora are mandatory before
candidate cross-build and release. Engine has no PHP namespace, Composer/PHP/Zend/PIE dependency,
App service, database, network, authorization, transaction, trust, rendering or delivery behavior.

The sole binding surface is C with the `kumwe_engine_v1_` prefix. Internal C++ uses
`kumwe::engine`. No runtime oracle, fallback, FFI, subprocess integration or callback is present.
The diagnostic executable is a test tool. Installed consumers receive a static CMake library and
public C header; the internal C++ implementation has no stability promise.

Development targets are 64-bit Linux GCC/Clang and macOS AppleClang, tested in CI. Windows,
32-bit targets, stable binary ABI compatibility, TSan and PHP binding compatibility are not
claimed. Every supported source consumer builds without network retrieval or third-party libraries.

All native behavior, boundary, conformance, architecture, lifecycle and robustness tests belong
here. Binding tests later own PHP marshalling/lifecycle parity; App retains only composition,
authority, provisioning/recovery, acceptance and measured full-path performance evidence.

First App-eligible release is Engine 1.0.0 with frozen ABI 1, all five modules and the complete
v2 release gates. No publishing workflow exists in this incomplete development slice. Modules
remain in one repository until an independent consumer, independent cadence, stable contract and
measured benefit justify a split.
