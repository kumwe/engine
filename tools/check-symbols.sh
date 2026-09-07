#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
actual="$(mktemp)"
trap 'rm -f "$actual"' EXIT
nm -D --defined-only "$1" | awk '{print $3}' | LC_ALL=C sort > "$actual"
diff -u "$root/resources/abi-symbols.txt" "$actual"
