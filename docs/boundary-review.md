# Independent boundary review

On 2026-09-07 the extraction coordinator reviewed the proposed Engine scope before algorithm code.
The reviewed boundary is an E0/E1 exact-decimal development slice under the v2 native architecture,
using Conversion's approved existing operations and explicit draft semantic provenance. It is not a
full E1 candidate, frozen ABI, human release approval or evidence of all five modules.

A separate reviewer examined the first published C++/ABI implementation. Three refinements followed:

1. Test/corpus ownership paths now require canonical repository files, reject symlinks at every path
   component and reject directories/dot segments. Negative fixtures exercise those refusals.
2. Buffer view documentation now promises clearing data/size only after validating a supported struct
   size. A nonempty output slot is refused unchanged and remains releasable; callers must initialize
   output null for the null-on-refusal guarantee. A regression preserves the existing owned handle.
3. Work charging now covers the maximum normalized multiplication coefficient workspace. Short input
   literals padded to large scales cannot receive an unrealistically small multiplication budget.
   Regression vectors exercise both sides of the exact work boundary.

Native ownership, corpus identity and source manifests are checked mechanically. The exact semantic
release barrier and full candidate binding review remain open as recorded in MIGRATION-HANDOFF.md.
