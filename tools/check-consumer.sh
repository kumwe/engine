#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cmake -S "$root" -B "$work/build" -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$work/install"
cmake --build "$work/build" --parallel 4
cmake --install "$work/build"
# The downstream project sees installed files alone; source include paths are not passed.
cp -R "$root/tests/consumer" "$work/consumer"
cmake -S "$work/consumer" -B "$work/consumer-build" -DCMAKE_PREFIX_PATH="$work/install"
cmake --build "$work/consumer-build" --parallel 4
"$work/consumer-build/consumer"
if find "$work/install" -type f | grep -E '/(src|tests|tools)/|\.(php|py|tsv)$'; then exit 1; fi
printf 'Installed standalone C consumer passed\n'
