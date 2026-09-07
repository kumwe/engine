# Native boundary review

The candidate implements exact decimal, formula, normalized document/preparation, report and
canonical kernels behind the owned C ABI. The initial E0/E1 decimal review has been superseded
by full-kernel corpus, ownership, archive/consumer and Zend boundary checks. This review records
implementation evidence; it does not freeze ABI 1 or replace independent release acceptance.

The original review's ownership refinements remain enforced: repository test/corpus paths reject
symlinks and noncanonical paths; nonempty buffer slots remain unchanged on refusal; struct-size
validation precedes output-view clearing; multiplication work covers maximum normalized precision.

The readiness review corrected exact-fit batch output accounting and long-lived native plan
exhaustion, removed duplicated transport trees/copies, and introduced bounded internal framing.
An independent reviewer replayed all 79 expanded canonical corpus cases through KEC1 encode and
digest (158 operations), plus malformed length/tag/key refusals. The compiled framing review
compared 120 JSON/KEB1 calls across empty and nonempty batches, Unicode/control characters and
exact output budgets; 222 malformed/truncated/overflow/trailing frames refused without a result.
Corresponding corpus, framing, logical byte-budget and malformed-frame tests are committed here;
Zend owns the public PHP array, NUL-terminated JSON slice and lifetime tests in the binding repo.

Framing preserves ordered PHP key types, raw string bytes and IEEE-754 bits. Canonical admission
charges immediate keys before sorting and children, and applies profile budgets before caller-sized
allocations. Compiled input/output budgets charge their equivalent original JSON envelopes. The
fuzzer now reaches canonical and compiled execution as well as decimal and capabilities, with
valid binary seeds and owned-buffer status invariants.

Current PR CI must pass again after every source change. The candidate still requires independent
semantic-release verification, an accepted ABI/version and supported deployment matrix, immutable
release provenance and representative whole-call performance acceptance before stable publication.
