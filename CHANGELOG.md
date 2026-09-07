# Changelog

- Candidate canonical KEC1 framing removes tagged JSON/base64 expansion inside the existing ABI and PHP API; identical semantic corpus, limits and finding order are retained.

## Unreleased

- Implement all five deterministic kernels: exact decimal batches, compiled formula VM,
  normalized document preparation/computation/validation, report materialization and GenericV1
  canonical encoding with streaming SHA-256. All native behavior/boundary/conformance tests
  reside in Engine; the binding owns PHP-specific tests and App retains host acceptance.
- Correct exact output-byte accounting: valid nonempty batches now fit a budget equal to
  their complete serialized size; one-byte-short limits still refuse atomically.
- Remove direct-document tree copies and intermediate output-item buffers; parse ordinary
  JSON string spans in blocks while retaining UTF-8, escape, numeric and bound semantics.
- Reuse canonical tagged-decoder admission/order directly during emission instead of building
  and sorting a second normalized tree; decode base64 using a fixed lookup table.
- Match frozen corpora to exact published Conversion, Definition, Record Model, Reporting and
  Canonical JSON coordinates; preserve the independent release-attestation barrier.
- Retain C/ABI/CLI, corpus, fuzz/sanitizer/thread, lifecycle, archive/install, fault-seed and
  whole PHP/Zend benchmark gates. Pin all CI actions to reviewed source commits.
- NRM-2026-013 / NRM-2026-042 / NRM-2026-044 are enabling implementation evidence.
  ABI freeze, verified native release, full-path acceleration acceptance and App cutover
  remain outstanding; no functional roadmap completion is claimed.
