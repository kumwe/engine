# Diagnostic benchmark protocol

Build with `-DCMAKE_BUILD_TYPE=Release -DKUMWE_ENGINE_BENCHMARKS=ON`, then run
`build/engine-benchmark`. Four deterministic workloads contain 1, 32, 256 and 4096 conversion
products. Every sample includes request copy, coarse C ABI invocation, result copy and release.
Each workload has 20 warmups and 100 measured runs; sorted nearest-rank p50/p95/p99 nanoseconds and
request sizes are emitted as JSON lines. CI retains the output with source and runner identity.

Run on an otherwise quiet named host, record CPU/OS/compiler/CMake/build flags and corpus SHA,
and repeat full runs before interpreting a change. Compare distributions and RSS/allocation data,
not one fastest sample. Do not use a hard flaky time threshold in ordinary correctness CI.

This diagnostic has no PHP baseline, definition/document/report workloads, production concurrency,
measured allocation counts or App end-to-end boundary. It therefore supports development only.
It does not demonstrate an App speedup, capacity objective, native cutover readiness or full E1 exit.
The later benchmark suite must add all those measurements, representative bursts and saturation.
