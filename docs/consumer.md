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

Do not integrate this draft with App or PHP yet. The next reviewed native steps are complete E1
formula/document semantics and corpora, E2 report/canonical modules, E3 freeze/hardening, exact
candidate cross-build in `kumwe/kumwe-engine`, human Engine merge/release, independent stable release
verification, then the thin Zend/PIE binding. The binding must own its source-embedded immutable
Engine archive, use this C ABI alone and clean handles under every PHP lifecycle path. No algorithm
or fallback belongs in the binding. App provisioning and Computation-owned runtime cutover follow
separately; App's existing execution and tests remain until that cutover.
