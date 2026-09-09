# Kumwe Engine

Standalone C++20 execution engine with a coarse C ABI. Engine 1.x implements exact
decimals, immutable formula plans, complete normalized document preparation and validation,
report materialization, and generic canonical encoding with streaming SHA-256 behind the
frozen C ABI 1.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
node tools/check-manifests.mjs build
bash tools/check-consumer.sh
build/kumwe-engine-conformance
```

CMake 3.25+, a C++20 compiler and a 64-bit Linux/macOS environment are required. The library,
conformance executable and C consumer build offline with the standard library and the exact
vendored PCRE2 source closure. Bash and Node are test and release tooling only and the Node
scripts are not part of the published source archive; PHP, Composer and App are neither build
nor runtime dependencies. No Python is used anywhere in this repository.

Installed downstream usage:

```cmake
find_package(KumweEngine CONFIG REQUIRED)
target_link_libraries(my_consumer PRIVATE Kumwe::Engine)
```

Read [CHARTER](CHARTER.md), [ABI](docs/abi.md), [architecture](docs/architecture.md),
[consumer integration](docs/consumer.md), [testing/ownership](docs/testing.md),
[security](SECURITY.md), [benchmarking](docs/benchmarking.md) and
[versioning and releases](docs/releasing.md).

## Releases

Every merge to `main` that passes the complete `Native quality` workflow is tagged and
published as an immutable source release `vMAJOR.MINOR.PATCH` with a reproducible archive,
SPDX inventory, checksums and GitHub OIDC build provenance. The declared version lives in
`resources/capabilities.json`; when a merge does not change it the workflow bumps the patch
automatically, and minor or major releases are declared in the pull request with
`bash tools/version.sh set X.Y.Z`. Each release notifies `kumwe/kumwe-engine`, which embeds
the exact archive and publishes the PHP extension under the same version. Consume a
published tag, never a moving branch.

All five modules have executable owner-corpus tests. ABI 1 is frozen with an independent
fixed-header dynamic consumer regression gate. `resources/contracts.json` records the exact
published semantic-owner sources, corpora and any independent verification receipts; those
records are printed in every release's notes.

The explicit PCRE2 dependency and normalized validator/decimal transport are documented in
[document validators](docs/document-validators.md). Upstream maintained 10.42 security
backports, per-file hashes and licenses are packaged; installation never selects a system
PCRE library.
