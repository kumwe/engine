#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mutate_once() {
  python3 - "$1" "$2" "$3" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
source = path.read_text()
before, after = sys.argv[2:]
matches = source.count(before)
if matches != 1:
    raise SystemExit(f"Fault mutation must match exactly once in {path}: found {matches}")
path.write_text(source.replace(before, after, 1))
PY
}
for fault in negative-order tie-even magnitude-sign required-validation finding-order; do
  target="$work/$fault"
  mkdir -p "$target"
  cp -R "$root"/{CMakeLists.txt,CHARTER.md,LICENSE,src,include,tests,resources,cmake,corpus,cli,tools,third_party,docs} "$target/"
  test_target=engine-tests
  arguments=()
  case "$fault" in
    negative-order) mutate_once "$target/src/decimal/decimal.cpp" 'return left_negative ? -result : result;' 'return result;' ;;
    tie-even) mutate_once "$target/src/decimal/decimal.cpp" '(first == 5 && (rest || odd))' '(first == 5 && (rest || !odd))' ;;
    magnitude-sign) mutate_once "$target/src/decimal/decimal.cpp" 'if (negative && !zero)' 'if (!negative && !zero)' ;;
    required-validation)
      mutate_once "$target/src/document/document.cpp" 'if (field.required) finding(field.handle, "required");' 'if (field.required) continue;'
      test_target=document-tests
      arguments=("$target/corpus/document/validation-v1.json")
      ;;
    finding-order)
      mutate_once "$target/src/document/document.cpp" 'findings.emplace_back(std::move(next));' 'findings.insert(findings.begin(), std::move(next));'
      test_target=document-tests
      arguments=("$target/corpus/document/validation-v1.json")
      ;;
  esac
  cmake -S "$target" -B "$target/build" -DCMAKE_BUILD_TYPE=Debug > "$target/configure.log"
  cmake --build "$target/build" --target "$test_target" --parallel 4 > "$target/build.log"
  status=0
  "$target/build/$test_target" "${arguments[@]}" > "$target/result.log" 2>&1 || status=$?
  if [ "$status" -eq 0 ]; then
    printf 'Fault seed survived: %s\n' "$fault" >&2
    exit 1
  fi
  if [ "$status" -ne 1 ]; then
    printf 'Fault seed did not produce a test assertion failure: %s (status %s)\n' "$fault" "$status" >&2
    cat "$target/result.log" >&2
    exit 1
  fi
  if [ "$test_target" = document-tests ]; then
    # Both fixed source faults must fail this actual owner case, not merely fail to run.
    if ! grep -q '^required_and_null expected .* actual ' "$target/result.log"; then
      printf 'Document fault was not killed by its frozen owner case: %s\n' "$fault" >&2
      cat "$target/result.log" >&2
      exit 1
    fi
    cat "$target/result.log"
  fi
  printf 'Fault seed killed by behavior/boundary tests: %s\n' "$fault"
done
