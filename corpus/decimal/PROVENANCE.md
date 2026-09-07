# Decimal corpus source

Semantic owner: `kumwe/conversion`. Behavior source: Conversion `v0.1.2`, commit
`5ccea7f7dc4ebd11bc27b11da8573da42807c196`. That legacy version has not been independently
release-verified under the v2 protocol.

`decimal-v1.tsv` is an unchanged copy of the 108-vector package-owned draft corpus at
`resources/conformance/decimal-v1.tsv` in the concurrent Conversion test-ownership PR.
SHA-256: `635db251898707828e24f12b1abb672273552f5f633186a725cc9f50ac08140c`.
This corpus is not present in the v0.1.2 release; it captures that implementation's behavior
for a forthcoming reviewed release. No claim of an immutable released corpus is made.

The UTF-8 TSV has one header, final LF and nine columns: id, operation, left_hex, right_hex,
precision, scale, rounding, outcome, expected_hex. Byte strings are lowercase hexadecimal;
empty means empty bytes, `-` means unused. Outcomes are value or invalid_argument.

Parse uses the stated precision/scale. Compare/multiply parse operands at their minimal
literal precision/scale. Compare normalizes the sign to -1/0/1 and refuses unequal scales.
Multiply has precision 65 and summed scale. Round uses the stated target precision/scale and
one of half_up, half_down, half_even, ceiling, floor or truncate. Refusal wording is not a
cross-language contract; native invalid_input corresponds to InvalidArgumentException.

Expected vectors were constructed independently using integer coefficient arithmetic and
reviewed boundary constants; Conversion's PHP suite replays them. Engine replays the exact
same file through its C++ implementation and C ABI. No PHP source or oracle is copied here.
This data is Apache-2.0 licensed by Kumwe; the source license is retained as LICENSE.
