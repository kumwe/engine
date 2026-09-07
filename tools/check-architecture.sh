#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
# Production can include only this reviewed list: no platform/network/host/runtime headers.
sources=()
while IFS= read -r source; do sources+=("$source"); done < <(find src include -type f | LC_ALL=C sort)
headers="$(sed -nE 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]([^>"]+)[>"].*/\1/p' "${sources[@]}" | LC_ALL=C sort -u)"
expected="$(cat resources/allowed-includes.txt)"
test "$headers" = "$expected" || { diff -u <(printf '%s\n' "$expected") <(printf '%s\n' "$headers"); exit 1; }
if find src include -type f | grep -E '\.(php|js|py|sh)$'; then exit 1; fi
if grep -En '\b(system|popen|dlopen|curl|fopen|socket|class_alias|zend_)\b' "${sources[@]}"; then exit 1; fi
# Binary64 is required solely by generic canonical JSON and frozen PHP text comparison.
# Exact monetary and formula arithmetic remains forbidden from using it.
exact_sources=()
while IFS= read -r source; do exact_sources+=("$source"); done < <(printf '%s\n' "${sources[@]}" | grep -Ev '^src/canonical/|^src/value/php_numeric.cpp$')
if grep -En '\b(double|float)\b' "${exact_sources[@]}"; then exit 1; fi
if grep -Ein 'FetchContent|ExternalProject|file\(DOWNLOAD|execute_process|find_package\((PHP|CURL|OpenSSL)' CMakeLists.txt cmake/*; then exit 1; fi
printf 'Architecture boundary passed\n'
