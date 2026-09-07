#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
bash tools/source-archive.sh
cp artifacts/kumwe-engine-source.tar.gz "$work/first.tar.gz"
bash tools/source-archive.sh
cmp "$work/first.tar.gz" artifacts/kumwe-engine-source.tar.gz
if tar -tzf artifacts/kumwe-engine-source.tar.gz | grep -E '(^|/)(vendor|build|node_modules|\.git)/|\.(php|phar|pem|key)$'; then exit 1; fi
tar -xzf artifacts/kumwe-engine-source.tar.gz -C "$work"
bash "$work/kumwe-engine/tools/check-consumer.sh"
printf 'Reproducible source archive consumer passed\n'
