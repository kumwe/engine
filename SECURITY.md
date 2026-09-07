# Security policy and draft threat model

Report vulnerabilities privately using the repository's GitHub security reporting channel when enabled,
or contact Kumwe maintainers privately. Include the exact source/build, minimal non-sensitive input and
sanitizer output. Do not include production document, credential or customer data in a public issue.
No released Engine version or support/backport promise exists yet; maintainers must define supported
release branches and response ownership before Engine 1.0.0.

Untrusted bytes can target parser overreads, integer truncation, arithmetic overflow, allocation
amplification and long batches. Validate input length, struct/ABI version, fields, precision, scale,
operation, count and budgets before copy/arithmetic. Literal syntax is ASCII and locale independent.
All operations refuse atomically, use fixed arithmetic workspaces and bounded output; error statuses
contain no input text. No host authority, ambient tenant state, filesystem, network or callbacks exist.

Handle abuse can cause use-after-free, double-free or races. Engine allocates and releases through
one domain; result bytes are immutable; release consumes the unique owner's slot. The C caller must
provide valid readable/writable buffers and never release a borrowed/copied/foreign handle. ASan,
UBSan, leak and lifecycle CI cover valid ownership and hostile encoded inputs. Arbitrary address
provenance cannot be validated portably, and this draft does not pretend to do so.

No third-party source dependency is vendored. Conversion-owned corpus data is Apache-2.0, identified
by exact digest/provenance. Source SPDX inventory and archive checks are development evidence. The
full release gate still requires pinned toolchains, dependency/license/advisory review, signed
provenance, candidate binding safety, fuzz retention and stable ABI compatibility. No production
release can be inferred from a green development test run.
