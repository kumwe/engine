# Frozen ABI 1 consumer

`client.c` and its private `include/kumwe/engine/engine.h` are exact snapshots from
Engine commit `db28497ee2bc083a8eaba76574c4b0f9af425daf`. `baseline.json` records both
SHA256 digests and the source coordinate. This is the initial ABI 1 compatibility
baseline, not a claim that an earlier stable Engine release existed.

On supported ELF/Linux builds CMake compiles that fixed C11 client without the
current public header, dynamically links the candidate's export-restricted DSO,
and executes the full 295-assertion ownership, layout, status, capability, decimal,
canonical, compiled-plan, buffer and cancellation contract. The same target runs
under the normal release and ASan/UBSan configurations. macOS retains its existing
installed C11/static consumer evidence; the dynamic compatibility fixture is not
advertised as macOS evidence.

Future ABI 1 releases must continue to pass this client unchanged. Do not regenerate
its header from the current include directory or replace it merely to make a changed
ABI pass. A deliberate breaking boundary requires a new ABI major and coordinated
binding release. Add a separate baseline for a new major; retain this one while ABI 1
is supported.
