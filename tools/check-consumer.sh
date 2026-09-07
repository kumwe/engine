#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cmake -S "$root" -B "$work/build" -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$work/install"
cmake --build "$work/build" --parallel 4
cmake --install "$work/build"
# The downstream C11 project sees installed files alone; source include paths are not passed.
cp -R "$root/tests/consumer" "$work/consumer"
cmake -S "$work/consumer" -B "$work/consumer-build" -DCMAKE_PREFIX_PATH="$work/install" \
    -DCMAKE_C_STANDARD=11 -DCMAKE_C_STANDARD_REQUIRED=ON -DCMAKE_C_EXTENSIONS=OFF \
    '-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror'
cmake --build "$work/consumer-build" --parallel 4
mkdir "$work/fixtures"
"$work/consumer-build/consumer" "$work/fixtures"
# Requests use identities discovered through the installed public ABI; the CLI also comes from that prefix.
cli="$work/install/bin/kumwe-engine-conformance"
"$cli" --capabilities > "$work/capabilities.json"
cp "$work/install/share/kumwe-engine/capabilities.json" "$work/expected-capabilities.json"
printf '\n' >> "$work/expected-capabilities.json"
cmp "$work/expected-capabilities.json" "$work/capabilities.json"
"$cli" --verify-bundle "$root/corpus" > "$work/corpus-result.json"
"$cli" --compile "$work/fixtures/compile.json" > "$work/description.json"
cmp "$work/fixtures/compile.json" "$work/description.json"
"$cli" --execute "$work/fixtures/compile.json" "$work/fixtures/batch.json" > "$work/results.json"
cmp "$work/fixtures/batch-expected.json" "$work/results.json"
"$cli" --canonical "$work/fixtures/canonical.json" > "$work/canonical.json"
cmp "$work/fixtures/canonical-expected.json" "$work/canonical.json"
"$cli" --canonical - < "$work/fixtures/canonical.json" > "$work/piped-canonical.json"
cmp "$work/canonical.json" "$work/piped-canonical.json"
printf '{' > "$work/malformed.json"
status=0
"$cli" --compile "$work/malformed.json" > "$work/refusal.json" || status=$?
test "$status" -eq 1
printf '{"status":1}\n' > "$work/expected-refusal.json"
cmp "$work/expected-refusal.json" "$work/refusal.json"
if find "$work/install" -type f | grep -E '/(src|tests|tools)/|\.(php|py|tsv)$'; then exit 1; fi
printf 'Installed standalone C11 consumer and diagnostic CLI passed\n'
