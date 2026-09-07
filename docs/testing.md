# Test ownership and evidence

Every native behavior, boundary and conformance test belongs in Engine. `tests/ownership.json`
maps all twelve exported C functions to discovered CTest IDs. The manifest gate checks headers,
exported symbols, exact corpora and negative ownership fixtures; a new export without its own
behavior and boundary coverage fails the gate. Downstream suites do not replace native tests.

| Owned suite | Responsibility |
|---|---|
| Decimal behavior/boundary and Conversion corpus | Exact values, six rounding modes, wide arithmetic, differential identities, all 108 owner vectors, budgets and 10,000 lifecycle cycles |
| Formula corpus and compiled-plan ownership | Immutable compilation, typed execution, malformed profiles, opaque payloads, cancellation, concurrent execution, exact output byte limits and reuse after refusal |
| Document/preparation/normalization/validator corpora | Complete normalized preparation, computed values, ordered findings, Unicode behavior and hostile inputs |
| Reporting and numeric-string corpora | Typed materialization, aggregation, ordering, converted values and exact refusal semantics |
| Canonical corpus and lossless JSON transport | GenericV1 byte/digest parity, raw value kinds, escaping, UTF-8, duplicates, bounds and malformed long string spans |
| C consumer, CLI, architecture and exported symbols | Actual C linkage/ownership, all diagnostic routes, package boundaries and exact Linux ABI export allowlist |
| Archive/installed consumer and fault seeds | Reproducible committed source, independent consumption and deliberate arithmetic/validation fault detection |

CI runs GCC/Clang Linux and AppleClang macOS builds, ASan/UBSan with leak detection and corpus-seeded
libFuzzer, and the shared immutable-plan test under TSan. The exact workflow result is evidence for
its tested source/platform only. Stable old-client ABI compatibility, final independent candidate
attestation and release acceptance remain outstanding. Foreign fabricated pointers are outside the
C memory preconditions; null, owned, consumed and valid concurrently borrowed handles are tested.

The extension owns Zend marshalling, request lifecycle and cross-layer replay. App retains composition,
authorization, persistence, provisioning/recovery, acceptance and whole-path performance tests. No
App implementation or unit test is removed by this package change. Test-only PHP oracle/benchmark
sources are excluded from native source and extension distribution archives.

The test-only shared export probe omits `-z,defs` with sanitizers because Clang resolves its sanitizer
runtime from the final executable; release probes retain it. No sanitizer diagnostic is suppressed.
