# Security policy and threat model

Report vulnerabilities privately using the repository's GitHub security reporting channel when enabled,
or contact Kumwe maintainers privately. Include the exact source/build, minimal non-sensitive input and
sanitizer output. Do not include production document, credential or customer data in a public issue.
Kumwe repository maintainers own advisory triage, coordinated disclosure and security patch releases.
The newest published 1.x release is the supported line; consumers must move to its latest patch
before requesting a fix. Development snapshots and superseded patches are not supported releases.
Security fixes retain ABI 1 compatibility and pass the complete native and binding release gates;
an incompatible correction requires an explicit new ABI major and migration notice. No response
time or support duration is promised by this policy.

Untrusted bytes can target parser overreads, integer truncation, arithmetic overflow, allocation
amplification and long batches. Validate input length, struct/ABI version, fields, precision, scale,
operation, count and budgets before copy/arithmetic. Literal syntax is ASCII and locale independent.
All operations refuse atomically, use fixed arithmetic workspaces and bounded output; error statuses
contain no input text. No host authority, ambient tenant state, filesystem, network or callbacks exist.

Handle abuse can cause use-after-free, double-free or races. Engine allocates and releases through
one domain; result bytes are immutable; release consumes the unique owner's slot. The C caller must
provide valid readable/writable buffers and never release a borrowed/copied/foreign handle. ASan,
UBSan, leak and lifecycle CI cover valid ownership and hostile encoded inputs. Arbitrary address
provenance cannot be validated portably.

PCRE2 10.42 and its SLJIT source closure are statically vendored with every upstream-recommended
security backport; `resources/pcre2-source.json` records exact commits and hashes. Its JIT is qualified
on x86_64 and AArch64. AArch64 disables the optional legacy SIMD optimization because upstream
identifies safety fixes that cannot be backported; scalar JIT preserves the frozen recursion and
budget semantics. Other architectures require separate qualification and are rejected at configure.
Recheck [upstream lifecycle guidance](https://github.com/PCRE2Project/pcre2/blob/main/SUPPORT-LIFECYCLE.md)
and advisories for every release. Licensed Unicode and semantic-owner corpus inputs retain their
recorded provenance and are included in the source SPDX inventory. Source locks do not establish
ongoing advisory coverage. Release qualification still requires signed source provenance, exact
candidate binding safety, sanitizers, retained fuzz evidence and stable ABI compatibility.
