#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
# Production can include only this reviewed list: no platform/network/host/runtime headers.
headers="$(sed -nE 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]([^>"]+)[>"].*/\1/p' src/*.cpp src/*.hpp src/decimal/* include/kumwe/engine/* | LC_ALL=C sort -u)"
expected="$(cat resources/allowed-includes.txt)"
test "$headers" = "$expected" || { diff -u <(printf '%s\n' "$expected") <(printf '%s\n' "$headers"); exit 1; }
if find src include -type f | grep -E '\.(php|js|py|sh)$'; then exit 1; fi
if grep -En '\b(double|float|system|popen|dlopen|curl|fopen|socket|class_alias|zend_)\b' src/*.cpp src/*.hpp src/decimal/*; then exit 1; fi
if grep -Ein 'FetchContent|ExternalProject|file\(DOWNLOAD|execute_process|find_package\((PHP|CURL|OpenSSL)' CMakeLists.txt cmake/*; then exit 1; fi
printf 'Architecture boundary passed\n'
