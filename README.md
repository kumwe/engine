# Kumwe Engine

Standalone C++20 native foundation with a coarse C ABI. This draft implements the first
exact-decimal slice: canonical fixed-scale values, exact comparison/multiplication, six rounding
rules and atomic bounded batches. Its 108-vector Conversion-owned corpus runs through C++ and C.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
node tools/check-manifests.mjs build
bash tools/check-consumer.sh
build/kumwe-engine-conformance
```

CMake 3.25+, a C++20 compiler and a 64-bit Linux/macOS environment are required. The library,
conformance executable and C consumer build offline with the standard library alone. Node and
Bash are test/release tooling only; PHP, Composer and App are neither build nor runtime dependencies.

Installed downstream usage:

```cmake
find_package(KumweEngine CONFIG REQUIRED)
target_link_libraries(my_consumer PRIVATE Kumwe::Engine)
```

Read [CHARTER](CHARTER.md), [ABI](docs/abi.md), [architecture](docs/architecture.md),
[consumer integration](docs/consumer.md), [testing/ownership](docs/testing.md),
[security](SECURITY.md), [benchmarking](docs/benchmarking.md) and [release gates](docs/releasing.md).

This is an **E0/E1 development slice**. The approved five-module Engine architecture remains:
exact decimal → definition/formula VM → whole-document batch → reporting and canonical streaming.
Only decimal is implemented here. ABI 1 is a draft proposal; Engine 1.0.0, the extension candidate
cross-build, immutable semantic release verification and App acceleration are not claimed.
