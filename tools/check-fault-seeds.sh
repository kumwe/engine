#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
for fault in negative-order tie-even magnitude-sign; do
  target="$work/$fault"
  mkdir -p "$target"
  cp -R "$root"/{CMakeLists.txt,CHARTER.md,LICENSE,src,include,tests,resources,cmake,corpus,cli,tools,third_party} "$target/"
  case "$fault" in
    negative-order) sed -i 's/return left_negative ? -result : result;/return result;/' "$target/src/decimal/decimal.cpp" ;;
    tie-even) sed -i 's/(first == 5 \&\& (rest || odd))/(first == 5 \&\& (rest || !odd))/' "$target/src/decimal/decimal.cpp" ;;
    magnitude-sign) sed -i "s/if (negative \&\& !zero)/if (!negative \&\& !zero)/" "$target/src/decimal/decimal.cpp" ;;
  esac
  cmake -S "$target" -B "$target/build" -DCMAKE_BUILD_TYPE=Debug > "$target/configure.log"
  cmake --build "$target/build" --target engine-tests --parallel 4 > "$target/build.log"
  if "$target/build/engine-tests" > "$target/result.log" 2>&1; then
    printf 'Fault seed survived: %s\n' "$fault" >&2
    exit 1
  fi
  printf 'Fault seed killed by behavior/boundary tests: %s\n' "$fault"
done
