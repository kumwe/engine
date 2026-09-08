# Kumwe Engine

Standalone C++20 execution engine with a coarse C ABI. The Engine 1.0.0 source candidate implements exact
decimals, immutable formula plans, complete normalized document preparation and validation,
report materialization, and generic canonical encoding with streaming SHA-256.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
node tools/check-manifests.mjs build
bash tools/check-consumer.sh
build/kumwe-engine-conformance
```

CMake 3.25+, a C++20 compiler and a 64-bit Linux/macOS environment are required. The library,
conformance executable and C consumer build offline with the standard library and the exact vendored PCRE2 source closure. Node and
Bash and Python are test/release tooling only; PHP, Composer and App are neither build nor runtime dependencies.

Installed downstream usage:

```cmake
find_package(KumweEngine CONFIG REQUIRED)
target_link_libraries(my_consumer PRIVATE Kumwe::Engine)
```

Read [CHARTER](CHARTER.md), [ABI](docs/abi.md), [architecture](docs/architecture.md),
[consumer integration](docs/consumer.md), [testing/ownership](docs/testing.md),
[security](SECURITY.md), [benchmarking](docs/benchmarking.md) and [release gates](docs/releasing.md).

All five modules have executable owner-corpus tests. ABI 1 is frozen with an independent
fixed-header dynamic consumer regression gate. Engine 1.0.0 publication and immutable release
verification remain gated on verified semantic inputs and the exact independent binding
cross-build. App acceleration acceptance remains a separate measured integration outcome.

The explicit PCRE2 dependency and normalized validator/decimal transport are documented in [document validators](docs/document-validators.md). Upstream maintained10.42 security backports, per-file hashes and licenses are packaged; installation never selects a system PCRE library.
