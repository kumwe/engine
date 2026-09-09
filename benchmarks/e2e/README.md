# Whole-boundary benchmark evidence

This directory retains historical whole-boundary measurements only. The harness that
produced them now lives in the PHP binding repository, where it belongs to the code it
measures: `kumwe/kumwe-engine` `tools/benchmark-runtime.php` orchestrates the comparison,
`tools/benchmark/worker.php` executes the unchanged App methods against the loaded
extension, and `tools/benchmark/allocation_probe.c` and `tools/benchmark/deepbind_probe.c`
are the glibc allocation diagnostics. The binding's `whole-boundary-benchmarks` CI lane
runs the complete matrix against every tested module and retains the results as an
artifact.

The retained evidence under `evidence/` explains the transport changes made while the
native candidate was prepared. It is not a release attestation and does not describe a
later Engine or binding commit. See `evidence/readiness-20260907/README.md` for the exact
modules that were measured.

The standalone C++ diagnostic benchmark remains `build/engine-benchmark`; see
[benchmarking](../../docs/benchmarking.md). Nothing in this directory ships in the source
archive.
