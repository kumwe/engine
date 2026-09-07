# Draft C boundary

Status: development proposal, not frozen ABI 1. The final Computation compiler/executor framing
remains separate. This slice tests a coarse native decimal batch, not per-operation bindings.

Only the public C header is installed. All records use natural platform alignment: view offsets
on supported 64-bit platforms are 0/4/8/16 and size 24. `struct_size` must be between 24 and 4096 inclusive and
`abi_major` exactly 1. Trailing extension bytes are ignored; no unsupported behavior is inferred.
Input/output memory must be valid for its declared size. Null data is valid only with zero size.
Input bytes are borrowed only during the call, never retained. Public records are not wire bytes.

Functions return fixed status numbers: 0 success, 1 invalid_input, 2 unsupported_version,
3 incompatible_capability, 4 incompatible_corpus, 5 invalid_program, 6 exhausted_limit,
7 cancelled, 8 internal_failure. Only statuses relevant to implemented capabilities are emitted.
No C++ exception escapes. Errors contain no caller data and produce no partial batch.

`capabilities` accepts ASCII `KEC1` followed by little-endian u32 required ABI (1) and u32 required
capability mask (bit 0 decimal batch; zero permits discovery). Unknown bits refuse with
incompatible_capability and any other ABI refuses with unsupported_version. It returns bounded JSON metadata describing this
DEVELOPMENT build, implemented draft capability, source profile and corpus digest. This is not
an assertion that the upstream release was independently verified.

`decimal_batch` accepts binary `KED1`, then little-endian u32 row count (1..4096), u32 caller
output byte limit (1..1048576), and u64 instruction budget (1..1000000000). Input is bounded to
1 MiB before parsing. Each row consists of six u8 values: operation, left precision, left scale,
target/right precision, target/right scale, rounding; then two little-endian u16 literal lengths,
then the left and right ASCII literals. Each literal is at most 68 bytes. No trailing bytes,
whitespace, alternate encoding, unknown operation or ignored nonzero field is accepted.

Operations: 0 normalize; 1 compare; 2 multiply; 3 round. Normalize requires target precision,
target scale, rounding and right literal length all zero. Compare/multiply require rounding zero
and use target/right precision and scale for the right operand. Round uses target precision/scale,
requires an empty right literal, and rounding 0 half_up, 1 half_down, 2 half_even, 3 ceiling,
4 floor, 5 truncate. Compare requires equal operand scales. Multiply returns precision 65 with
summed scale. No ad hoc policy, rates, units or money provider enters the batch.

Success bytes are `KER1`, little-endian u32 row count, then for every input row in order a u32
byte length plus ASCII result. Decimal results keep exact scale. Compare returns `-1`, `0` or `1`.
The output byte budget includes framing. Any row failure refuses the entire batch. Budgets use
conservative deterministic work units: each row costs 512 plus left-length times right-length;
work is charged before parsing/arithmetic. This is a bounded decimal work budget, not a VM
instruction model or a wall-clock deadline. There is no advertised cancellation capability yet.

Buffers are Engine-owned immutable opaque handles; callers must never allocate/copy/free their
layout. `buffer_view` returns a borrowed view valid until release. The owner passes its pointer
slot to `buffer_release`; release consumes and clears the slot, and repeated cleanup on that
cleared slot is harmless. Access/release must not race. Concurrent reads of an unreleased buffer
and independent calls are safe. Copies of a raw handle do not acquire ownership; releasing a
copied/dangling/foreign pointer is forbidden. Portable C cannot validate arbitrary address
provenance; fuzz tests exercise valid owned handles, malformed envelopes and null/cleared slots.
The binding must keep exactly one owner and clean it up on every exception/request shutdown.
