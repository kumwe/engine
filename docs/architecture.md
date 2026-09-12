# Architecture and ownership

`src/decimal` owns native exact digit operations. Its immutable value has canonical bytes and scale;
precision is checked at construction. Multiplication uses a fixed 132-entry digit workspace, never
binary floating point. Decimal precision is at most 65, so a signed fractional literal can be 68
bytes including `-0.`. Zero retains scale and loses a negative sign. Comparison requires equal scales.
The six built-in round modes implement Conversion's released vocabulary; Engine admits no callback
or registry of caller code. `src/vm` separately preserves Definition's 4,096-digit arithmetic
semantics, immutable typed formulas, dependency ordering and exact refusal behavior.

`src/batch` is the bounded native transport, whole-call work/output budget and atomic output assembly.
The `KED1` diagnostic transport is Engine-owned and does not duplicate the semantic decimal contract.
`src/abi` contains every exported function, exception containment and allocation boundary. The caller
provides byte views; Engine owns result buffers; the library retains no input, global cache or request
state. Buffers are immutable until their one owner releases them. All other C++ symbols are hidden.

The installed product is a static library, exported CMake target and C header. A test-only shared
library verifies the actual ELF export allowlist. Internal C++ types are private and not installed.
The CLI replays the owner corpus through both internal C++ and C ABI; it is not an App runtime route.
Architecture checks allowlist production includes and forbid network, file I/O and PHP. Binary64 is
confined to the generic canonical profile and frozen PHP numeric-string comparison; exact monetary
and formula arithmetic never uses it.

`resources/contracts.json` records all five module owners and truthful implementation states.
`resources/capabilities.json` advertises implemented semantic profiles and exact corpus digests.
`src/document` owns normalized preparation, typed value projections, computed-field normalization,
bounded validators and ordered findings. `src/reporting` owns computation-only materialization and
converted value validation. `src/canonical` owns generic canonical bytes and streaming digests.
`src/plan` binds immutable profile plans to explicit limits, cancellation and portable batch findings.
Native semantic input never includes database queries, authorization, raw secrets or callbacks.

The buffer-release pointer-to-owner-slot convention clears the owning slot, supporting repeated
exception cleanup. A copied raw pointer remains non-owning and cannot safely be released. The
PHP binding follows this ownership agreement; the frozen ABI contract and independent old-client
compatibility gate preserve it across ABI 1 releases.
