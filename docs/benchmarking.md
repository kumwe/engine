# Diagnostic benchmark protocol

Build with `-DCMAKE_BUILD_TYPE=Release -DKUMWE_ENGINE_BENCHMARKS=ON`, then run
`build/engine-benchmark`. Deterministic workloads contain 1, 32, 256 and 4096 conversion
products, formula/document batches, complete preparation batches, grouped report rows and canonical
values. Formula, document, preparation and report measurements separately cover compilation plus
execution and reuse of an immutable plan. Preparation uses a successful frozen owner fixture with
computed fields and conditions. Canonical encoding and streaming digest are measured separately.
Every sample includes request copy, coarse C ABI invocation, result copy and release.
Each workload has 20 warmups and 100 measured runs; sorted nearest-rank p50/p95/p99 nanoseconds and
request sizes are emitted as JSON lines. CI retains the output with source and runner identity.

Run on an otherwise quiet named host, record CPU/OS/compiler/CMake/build flags and corpus SHA,
and repeat full runs before interpreting a change. Compare distributions and RSS/allocation data,
not one fastest sample. Do not use a hard flaky time threshold in ordinary correctness CI.

An optional profile argument selects one workload family for diagnosis. CI runs every family.

The C++ diagnostic remains a development measurement. The separate whole-boundary harness
lives in the PHP binding repository (`kumwe/kumwe-engine` `tools/benchmark-runtime.php` with
`tools/benchmark/worker.php` and the allocation probes) and runs in that repository's CI. It
compares all six semantic workload families with unchanged App/PHP methods through the actual
Zend extension, verifies matching results, and records allocation/RSS, cold versus reused plans,
bursts and multi-process saturation. [`benchmarks/e2e`](https://github.com/kumwe/engine/blob/main/benchmarks/e2e/README.md) retains
historical evidence only. Neither synthetic suite establishes HTTP/database capacity or
automatically authorizes native cutover.

Output accounting retains the original logical JSON byte contract even when binary frames
carry fewer bytes. The result encoder collects its quoted-string size while emitting bytes;
document execution finalizes the exact totals already checked before each retained value
and finding. These optimizations preserve refusal timing and do not relax output limits.
Compare the complete caller against the preceding immutable candidate to evaluate them;
the removal of a counting traversal is not itself an acceleration acceptance result.
