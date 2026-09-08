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

Use an independently verified immutable Engine release for production bindings. ABI 1
now has a frozen initial header/client compatibility gate; all five kernel corpora and
owned behavior/boundary tests remain mandatory. The exact candidate archive must pass
the independent Zend cross-build before Engine publication, followed by external release
verification. The binding embeds that exact unmodified archive and owns only Zend
marshalling/lifecycle; it contains no algorithm or fallback. App provisioning and the
Computation-owned runtime cutover follow separately, after verified native publication.
