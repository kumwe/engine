# Core integration contract

Engine provides deterministic native execution through C ABI 1. The PHP binding embeds the
exact immutable Engine source archive; Computation owns PHP semantic adapters. Core consumes
those adapters and retains authority over its business runtime.

| Owner | Responsibility |
|---|---|
| Engine | Exact decimal, immutable formula plans, document preparation and validation, report materialization, canonical encoding and digests |
| Semantic libraries | Meaning of profiles, owner corpora, findings and domain interfaces |
| PHP binding | Zend marshalling, request-local handles, module identity and PIE source installation |
| Computation | Semantic adapters, compiled-program association and cleanup |
| Core | Authorization, persistence, transactions, references, provisioning, recovery and end-to-end acceptance |

The [ABI contract](abi.md) defines all twelve exports, statuses, limits, buffer ownership,
plan lifetimes and cancellation. Inputs remain caller-owned. Output slots start null; successful
calls return Engine-owned immutable data whose owner releases it through the corresponding ABI
function. Release cannot race borrowing reads or execution. No callback, database, network or
host authorization crosses this boundary.

`resources/contracts.json` and `resources/capabilities.json` identify semantic owners and exact
corpus digests. Historical `-draft/1` profile names are wire identities and remain unchanged.
ABI 1 symbols, statuses, view layout and ownership are frozen; breaking changes require a new
ABI major. Internal C++ implementation types are private.

Consumers verify source checksums and GitHub OIDC provenance, negotiate the complete ABI,
capability and corpus tuple, and qualify their supported platform before deployment. The
binding version equals the Engine version. Package release qualification does not establish
Core adoption, production performance or workload capacity.

Native conformance, ABI, bounds, lifetime, thread, sanitizer, fuzz and installed-consumer tests
remain here. Core retains composition, authority, storage, provisioning, recovery and acceptance
coverage. Recovery restores the full known-good deployment image and dependency tuple; it never
replaces a loaded native library in place.

See [consumer setup](consumer.md), [test ownership](testing.md), [security](../SECURITY.md) and
[release record](release-record.md) for the durable verification inputs and release requirements.
