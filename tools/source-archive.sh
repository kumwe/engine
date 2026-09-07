#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
mkdir -p artifacts
# Tracked committed source only: no local build cache or credential-bearing configuration.
# Running this twice on the same commit must produce identical bytes.
git archive --format=tar --prefix=kumwe-engine/ HEAD | gzip -n > artifacts/kumwe-engine-source.tar.gz
sha256sum artifacts/kumwe-engine-source.tar.gz > artifacts/SHA256SUMS
