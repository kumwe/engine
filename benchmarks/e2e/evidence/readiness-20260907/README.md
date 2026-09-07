# Readiness investigation evidence

These are historical development measurements, retained to explain the transport changes. They
are not a release attestation or evidence that a later Engine/binding commit was measured. Final
candidate measurements must identify the final module and independently generated source tuple
and remain outside both tested trees, avoiding a source/evidence identity cycle.

The prior SDK candidate is binding `81a30990f24767b61f3b0d1cfe4c073389e09b97`, embedding Engine
`24d43dd6b4755010f1eb8fea9c8f1d6373cc06fa`. The before-framing comparison uses binding
`b28b119214e4c29f1a3b75af952009711202e7e4`, embedding Engine
`587c09a39c5654cef9a8c206cd34ee43111d0b9f`. Its module SHA256 is
`86d99aff29677fb0c5040d848f18446d37b909f6be9cc3432db3c71ff849a997`.
The canonical-framed prototype embeds Engine `5997575c8e7168f90bb63ab794db4c0d1a7a79c3`
with the working binding frame encoder; its module SHA256 is
`49f5155fa4de933a08234679cdcfa6a40a0350463ce7394ecc18fb20610fe6cf`.
This prototype's binding source had not yet been published. Neither module is a stable release.

The interleaved runs use 60 rounds, 10 warmups and deterministically shuffled PHP/old/new order,
checking the exact output digest on every complete call. The complete matrix uses 30 samples,
valid and hostile inputs, warm/cold execution, and 1/2/4-worker burst and closed-loop saturation.
The worker executes unchanged App PHP implementations as the reference, creates inputs,
marshals through the actual extension, decodes results and serializes the caller's output.
Canonical comparisons use the shared App/native subset documented in the worker, including
arrays of at most 256 members. Shared-host load varies; raw medians are not production SLOs.

At 256 items, the canonical framed prototype measured 0.2400 ms encode versus 1.1123 ms for
the old module and 0.2117 ms for PHP; digest measured 0.2299/1.2342/0.2158 ms respectively.
At 32 items, native remained slower than PHP. Before framing, document and preparation calls
also remained slower than PHP despite improvements; those negative profiles are preserved in
full. No representative acceleration gate is closed by these intermediate measurements.

Callgrind files profile the before-framing module. Inclusive instruction counts show canonical
binding execution at 354M instructions, JSON parsing at 159M and tagged decode at 117M.
Document binding execution used 519M, with native JSON parsing 106M, PHP output decoding 113M,
native JSON encoding 95M and document execution 152M. Counts are inclusive and overlap: do not
sum them or treat instruction counts as elapsed latency. Startup and correctness hashes exist
outside measured complete-call timing and are included in the process-wide Callgrind capture.
These findings motivated bounded KEC1 canonical and KEB1/KER2 compiled transports.

Compressed raw files use deterministic gzip metadata; `digests.json` records compressed and
uncompressed SHA256 values. `../../interleaved.py` is the reusable current interleaved driver;
`../../run.py` owns the complete warm/cold, hostile, capacity and allocation matrix. Re-run both
against the exact final artifact before accepting performance for a supported deployment tuple.
