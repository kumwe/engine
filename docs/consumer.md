# Native consumption

Build/install with CMake, then consume only the installed `Kumwe::Engine` target. The consumer smoke
project is plain C and uses the public header; CMake selects the C++ linker for the standard library.
No network operation occurs during configure, build, install or consumer compilation. The source
archive smoke test proves this using an isolated unpacked source tree and installed prefix.

Initialize each view with its true allocated struct size, ABI 1, pointer and byte length. Initialize
result handle slots to null. Check the status before reading a result, borrow its immutable bytes
with `buffer_view`, and call `buffer_release(&owner)` once cleanup is due. Release clears the slot;
calling release again on that same cleared slot is safe. The library never frees caller input.

The exact framing and golden binary results are in [ABI](abi.md), `tests/support.hpp`,
`tests/engine_test.cpp` and `tools/fuzz-seeds.mjs`. Only `tests/consumer` is a supported integration
example; internal C++ headers are private implementation details.

Consume a published release tag, never a moving branch. Every `vMAJOR.MINOR.PATCH` release
ships `kumwe-engine-source.tar.gz` with checksums, an SPDX inventory and GitHub OIDC build
provenance (see [releasing](releasing.md)); verify the digest and attestation before building.
ABI 1 has a frozen header/client compatibility gate; all five kernel corpora and owned
behavior/boundary tests are mandatory for every release. The PHP binding
(`kumwe/kumwe-engine`) embeds that exact unmodified archive, owns only Zend
marshalling/lifecycle, contains no algorithm or fallback, and is published under the same
version. App provisioning and the Computation-owned runtime cutover follow separately.
