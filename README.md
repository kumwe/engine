# Kumwe Engine

[![Release](https://img.shields.io/github/v/release/kumwe/engine)](https://github.com/kumwe/engine/releases)
[![Native quality](https://github.com/kumwe/engine/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/kumwe/engine/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](#build)
[![Linux and macOS](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-blue)](CHARTER.md)
[![License: Apache-2.0](https://img.shields.io/github/license/kumwe/engine)](LICENSE)

Standalone C++20 execution engine with a coarse C ABI. Engine 1.x implements exact
decimals, immutable formula plans, complete normalized document preparation and validation,
report materialization, and generic canonical encoding with streaming SHA-256 behind the
frozen C ABI 1.

## Build

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
nor runtime dependencies.

Installed downstream usage:

```cmake
find_package(KumweEngine CONFIG REQUIRED)
target_link_libraries(my_consumer PRIVATE Kumwe::Engine)
```

Read [CHARTER](CHARTER.md), [ABI](docs/abi.md), [architecture](docs/architecture.md),
[consumer integration](docs/consumer.md), [testing/ownership](docs/testing.md),
[security](SECURITY.md), [benchmarking](docs/benchmarking.md) and
[versioning and releases](docs/releasing.md), [Core contract](docs/core-contract.md) and
[release record](docs/release-record.md).

## Releases

Releases are fully automated; people only merge. The declared version lives in
`resources/capabilities.json`, and every change to released source (everything the source
archive exports) declares a new version in the same change with
`bash tools/version.sh set X.Y.Z`; the pull-request check refuses a released-source change
that keeps an already published version. When a merge to `main` passes the complete `Native
quality` workflow, the declared version is tagged and published as an immutable source
release `vMAJOR.MINOR.PATCH` with a reproducible archive, SPDX inventory, checksums and
GitHub OIDC build provenance; a release an earlier run left unfinished is completed by the
next run, and nobody ever creates, moves or deletes a tag by hand. Each release notifies
`kumwe/kumwe-engine`, which embeds the exact archive and publishes the PHP extension under the
same version. Consume a published tag, never a moving branch.

All five modules have executable owner-corpus tests. ABI 1 is frozen with an independent
fixed-header dynamic consumer regression gate. `resources/contracts.json` records the exact
published semantic-owner sources, corpora and any independent verification receipts; those
records are printed in every release's notes.

The explicit PCRE2 dependency and normalized validator/decimal transport are documented in
[document validators](docs/document-validators.md). Upstream maintained 10.42 security
backports, per-file hashes and licenses are packaged; installation never selects a system
PCRE library.
