# App / extension performance harness

This Linux harness runs unchanged App methods against the actual loaded PHP extension. It covers
Conversion decimal multiplication, Definition integer formulas, complete normalized document
computation/validation, create preparation including the host `RecordValueCodec`, grouped report
materialization, and canonical encoding/digest on the shared no-float Definition subset. Fixtures are
derived from the frozen owner corpora; sizes 1/32/256/4096 also scale document definition width
1/4/16/64. Hostile variants exercise arithmetic overflow, validation findings, missing report input
and invalid UTF-8. Every measured result and every saturation request must match the PHP digest;
unexpected exceptions or changed findings fail the run. This is synthetic application semantic
work, without HTTP, database, authorization, persistence, crypto or allocation-service traffic.

Use an unchanged App checkout, its matching SDK source checkout and a Composer autoloader with its
actual Conversion and other runtime dependencies. Nothing is installed into App. The extension must
contain the exact Engine source and corpus being benchmarked; its runtime capabilities and embedded
build tuple, the actual `.so` hash, PHP binary hash, source hashes, corpus hashes and host metadata are
recorded. An old extension fails corpus/profile admission rather than silently benchmarking fallback.

```sh
python3 benchmarks/e2e/test_runner.py
python3 benchmarks/e2e/run.py \
  --php /path/to/php --extension /path/to/kumwe_engine.so \
  --app /path/to/unchanged-app --sdk /path/to/matching-sdk \
  --autoload /path/to/installed/vendor/autoload.php \
  --build-dir /path/to/native-build --output /path/to/new-results-directory
```

`--smoke` exercises correctness and instrumentation with only three samples; it cannot support a
performance claim. The full default records ten warmups, thirty samples and raw timings, cold
compilation and execution versus plan reuse, p50/p95/p99, output size, throughput, Zend memory and
process RSS/high-water marks. Native timing includes PHP input construction, host normalization for
preparation, JSON/KED encoding, extension marshalling and copies, C ABI execution, output decoding
and final PHP serialization. The PHP timing includes the equivalent original semantic methods and
final serialization. Cold timings include host definition construction, native compilation and plan
release. Formula/document/preparation workloads use native batches of at most 64 documents. Report
materialization uses the complete row set in one call. These boundaries are part of the measured cost.

Canonical encoding and digest are separate workloads. The App canonical owner limits arrays to 512
members and depth 32; generated maps have at most 256 members and no floating point values. This
comparison does not assert equivalence between that profile and the entire generic canonical profile.

Capacity tests run a fixed backlog of 64 jobs and then keep 1, 2, 4 and 8 independent PHP processes
saturated for three seconds each. They retain queue/completion latency, process-boundary latency,
worker service latency, achieved work units/second and every raw observation. Work units are decimal
products, formula/document/preparation records, report input rows or canonical input entries. Queue
latency starts when the complete burst is submitted; it is distinct from service latency. Saturation
includes harness IPC and correctness checks, so report it separately from worker timings. Compare
scaling and the achieved plateau to the actual deployment's arrival/burst requirements; 58 records/s
is not assumed to be peak demand or a capacity target. Increase `--capacity-seconds` and repeat on a
quiet representative deployment host before operational sizing. Optional
`--demand preparation=YOUR_OBSERVED_RATE` records headroom against explicitly supplied deployment
demand, using the maximum observed saturation point; no arrival rate is invented. Metadata records host load, not
proof of absence of contention.

The separate glibc allocation pass uses `USE_ZEND_ALLOC=0` and a compiled `LD_PRELOAD` probe. It counts
successful interposed allocation calls and requested bytes (including full realloc requests), not
live allocations or underlying custom-arena calls. PHP loads extensions with `RTLD_DEEPBIND`, which can bypass ordinary preload counters. A second,
diagnostic-only interposer removes that flag and records requested/stripped load counts; missing or
partial interception fails the diagnostic. This changes diagnostic symbol lookup, so its timing is
never substituted for normal PHP timing. Matched native allocation fixtures verify interception.
Counts cover the whole fresh worker: startup,
compilation, warmup, measured iterations and shutdown. Primary timing uses the normal Zend allocator.
Probe and native/PHP identities, raw counters and process RSS are retained. Matrix RSS is a process
high-water mark across its earlier workloads; allocation workers provide a fresh per-profile view.

`results.json` is checkpointed after each workload. It contains raw samples, comparisons and complete
identity metadata; failures leave `complete: false`. No speedup or release gate is inferred merely
from successful execution. For a repeat on the same hardware/PHP/App/corpus, pass
`--baseline /path/to/prior/results.json`. A deterministic 2,000-resample independent bootstrap compares
median latency distributions (at least 20 samples each). It fails only when the lower 95% ratio bound
exceeds `1 + --regression-threshold` (default 10%). Report this screening alongside repeated full runs;
it is not a substitute for representative deployment evidence or a significance correction across
many exploratory comparisons. The admission test deliberately corrupts correctness and injects a
known regression to verify those gates without presenting synthetic test numbers as measurements.

PHP oracle files and the entire test-only harness are excluded from Engine source/PIE archives.
The harness is not linked into the native runtime and does not change sanitizer or fuzzing settings.
