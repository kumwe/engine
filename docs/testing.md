# Test ownership and evidence

Every implemented native behavior, boundary and conformance test belongs in Engine. `tests/ownership.json`
maps every exported C function to real discovered CTest IDs. The validator compares header exports,
symbol manifest and test inventory, requires behavior/boundary ownership, verifies the exact corpus
digest and rejects nine intentionally weakened declarations. Adding an ABI export without coverage
cannot pass CI. Future kernels must extend their own tests and semantic corpus before advertising a
capability. Downstream suites do not substitute for missing native package tests.

- `behavior-boundary`: exact values, all six modes, wide products, independent integer differential
  identities, malformed bytes, precision/scale limits, atomic output, work/output caps, version and
  capability refusal, 10,000 allocation cycles and concurrent immutable reads/independent calls.
- `conversion-corpus`: all 108 owner vectors through both C++ and C ABI, including byte/refusal parity.
- `plain-c-ownership`: C compilation/linking, struct layout, capability response and repeated cleanup.
- `architecture`: production include/dependency and forbidden-operation checks.
- `exported-symbols`: Linux test shared-library actual export equality with the C ABI allowlist.
- `check-consumer.sh` and `check-archive.sh`: isolated install and reproducible committed source build.
- `check-fault-seeds.sh`: three independently rebuilt arithmetic faults must be killed by existing tests.
- Clang fuzz CI mutates real encoded seed batches and capability envelopes; ASan/UBSan/LeakSanitizer
  run the suite and fuzz harness. Retain minimized regressions when found.

GCC, Clang and AppleClang release jobs build and run the package tests. Linux Clang sanitizer/fuzz,
source archive and installed consumer jobs are required. TSan, allocator fault injection, candidate
PHP/Zend lifecycle, stable ABI backward compatibility and the complete five-module suite are future
release gates, not evidence produced by this first slice. Invalid foreign pointer dereferences are
outside valid C memory preconditions; fuzzing does not fabricate unsafe addresses and call that
memory-safety evidence. Owned, null, consumed and valid concurrently read buffers are exercised.

Conversion owns the semantic corpus itself; Engine owns native replay and ABI behavior. The extension
will retain cross-layer conformance and marshalling/lifecycle tests. App retains composition, security,
storage, provisioning/recovery, acceptance and whole-path performance. This draft removes no App tests.

The Linux test-only shared export probe omits `-z,defs` in sanitizer builds because Clang links the
ASan runtime into the final executable; release probes retain it. This follows the
[Clang AddressSanitizer usage documentation](https://clang.llvm.org/docs/AddressSanitizer.html#usage).
No sanitizer diagnostic is suppressed.
