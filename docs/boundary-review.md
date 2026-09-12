# Native boundary invariants

All five kernels use the owned C ABI and the frozen ABI 1 header/client baseline. Native tests
enforce these behavior and ownership invariants on every tested source.

- Repository test/corpus paths reject symlinks and noncanonical paths.
- Nonempty buffer slots remain unchanged on refusal; struct-size validation precedes output clearing.
- Multiplication work covers maximum normalized precision, and batch output accounting accepts exact fits.
- Owned plans and bounded framing avoid duplicated transport trees and enforce source/lifetime budgets.
- Canonical framing preserves ordered PHP key types, raw string bytes and IEEE-754 bits. Admission
  charges immediate keys before sorting/children and applies profile budgets before caller-sized allocation.
- Compiled input/output budgets charge equivalent original JSON envelopes. Malformed, truncated,
  overflowed and trailing frames refuse without returning a result.

Committed corpus, framing, logical byte-budget and malformed-frame suites exercise these guarantees.
The fuzzer reaches canonical, compiled, decimal and capabilities calls with valid binary seeds and
owned-buffer status invariants. Zend owns public PHP arrays, JSON slices and request lifetimes in the
binding repository.

The complete workflow qualifies each changed source independently. Release checksums and provenance
bind the published artifact; representative Core workload acceptance remains a consumer responsibility.
