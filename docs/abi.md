# Draft C boundary

Status: development proposal, not frozen ABI 1. The implemented surface includes exact decimal
batches, immutable formula/document/report plans, cooperative cancellation, and canonical encoding/digests.

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
conservative deterministic work units: each row costs 512 plus 4356 for multiplication (the maximum 66-by-66 normalized
coefficient workspace);
work is charged before parsing/arithmetic. These logical work units conservatively charge digit-loop capacity, not actual CPU
instructions. Decimal framing has its own deterministic work budget; plan execution additionally supports deadlines and cancellation.

Output handle slots must initially be null; conforming calls always leave null on refusal.
A nonempty output slot violates that precondition and is refused unchanged, so the existing
owner remains releasable. Output view data/size clear on failure only after a supported struct
size has been validated; undersized records are never accessed past the size field.

Buffers are Engine-owned immutable opaque handles; callers must never allocate/copy/free their
layout. `buffer_view` returns a borrowed view valid until release. The owner passes its pointer
slot to `buffer_release`; release consumes and clears the slot, and repeated cleanup on that
cleared slot is harmless. Access/release must not race. Concurrent reads of an unreleased buffer
and independent calls are safe. Copies of a raw handle do not acquire ownership; releasing a
copied/dangling/foreign pointer is forbidden. Portable C cannot validate arbitrary address
provenance; fuzz tests exercise valid owned handles, malformed envelopes and null/cleared slots.
The binding must keep exactly one owner and clean it up on every exception/request shutdown.

## Immutable plans and whole batches

`compile` accepts a UTF-8 JSON object with `wire_version: 1`, `profile`, `corpus_digest`,
`program`, and optional `limits`. The profile and exact SHA-256 must match the runtime
`computation.contracts` capability entry. The program is either a JSON object or an opaque JSON
string parsed by the Engine. Opaque strings preserve number spellings across language bindings.
Private VM instructions are never a persisted/public format. The caller owns the returned plan
until `plan_release`; `plan_describe` returns the normalized, independently owned compile envelope.

`execute` takes that plan, an optional live cancellation handle, and a JSON object containing
`wire_version: 1`, `documents`, and `limits`. Each document contains a unique `correlation` and
either `fields`/`lines`, or an opaque `input` JSON string containing those two keys. Correlations
are returned in input order. Formula/document fields are normalized values; report fields contain
only `rows`, whose authorization and normalization have already been resolved by the caller.

Limits contain exactly six positive integers: `max_input_bytes` and `max_output_bytes` (at most
64 MiB), `max_documents` (4,096), `max_findings` (65,536), `max_instructions` (1,000,000,000),
and `max_milliseconds` (600,000). Compilation additionally limits program bytes to 16 MiB and
applies each profile's AST bounds. The input parser, work, retained output, final serialization and
deadline checks all participate in refusal. Limits are ceilings, not promises to allocate that much.

Success is `{ "wire_version": 1, "results": [...] }`. Each result has `correlation`, the typed
`result`, its exact `result_json` string, and ordered portable `findings`. Findings carry the
Computation code/severity/path/location/parameters shape. A business finding is successful native
execution; invalid programs/envelopes, resource exhaustion and cancellation are transport statuses.
Any such failure refuses the entire batch without exposing earlier results.

Plans are immutable and support concurrent execution while their owner remains alive. Release
must not race any borrowed use. `cancellation_create` returns a separately owned sticky token;
`cancellation_request` may run concurrently with execution. Keep the token alive until all its
executions return. Cancellation is checked between complete items and before returning output.
It does not poison a reusable plan or permit partial output.

## Canonical operation

`canonical` accepts the owner-defined tagged PHP value representation with `wire_version: 1`,
`profile: "kumwe-canonical-json/generic-v1"`, the exact `corpus_digest`, `operation: "encode"`
or `"digest"`, `input`, and optional owner-profile `limits`. Tagged values preserve ordered mixed
array keys, binary64 bits and string bytes without invoking user callbacks. The normative tags,
limit spelling and finding precedence belong to the exact Canonical JSON corpus/profile.
The result is `{ "output": "..." }`, `{ "sha256": "..." }`, or a semantic `{ "finding": ... }`.
Malformed tags/envelopes return an ABI status. Canonical transport expansion has its own finite
64 MiB/2,000,000-node/512-depth envelope; it does not broaden the semantic profile's budgets.

## Diagnostic CLI

The installed `kumwe-engine-conformance` supports `--capabilities`, `--verify-bundle corpus-directory`,
`--compile request.json`, `--execute compile.json batch.json`, and `--canonical request.json`.
Use `-` for one JSON input from standard input. Native refusals emit `{"status":N}` and exit `N`;
success emits the Engine response. File/usage errors go to stderr. Input reads are bounded to 64 MiB.
The existing positional decimal TSV corpus replay remains available. This executable is a diagnostic
consumer of the same C ABI; the PHP application uses the extension directly.
