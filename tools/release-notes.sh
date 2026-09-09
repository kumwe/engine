#!/usr/bin/env bash
# Render the GitHub release notes for an assembled source bundle (see release-bundle.sh) to stdout.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
bundle="${1:?usage: release-notes.sh BUNDLE_DIRECTORY}"
record="$bundle/source.json"
[ -f "$record" ] || { printf 'Missing %s\n' "$record" >&2; exit 1; }
cd "$root"
version="$(jq -er '.version' "$record")"
tag="$(jq -er '.tag' "$record")"
commit="$(jq -er '.source.commit' "$record")"
source_url="https://github.com/kumwe/engine/blob/$commit"
previous="$(git describe --tags --abbrev=0 --match 'v[0-9]*' "$commit^" 2>/dev/null || true)"

printf 'Kumwe Engine %s: immutable source release from `%s`.\n\n' "$version" "$commit"
printf 'Every quality lane of the `Native quality` workflow passed on this exact commit before the tag was created. '
printf 'The attached archive, SPDX inventory and checksums carry GitHub OIDC build provenance; verify a download with '
printf '`sha256sum --check SHA256SUMS` and `gh attestation verify kumwe-engine-source.tar.gz --repo kumwe/engine`.\n\n'
printf '[Versioning and releases](%s/docs/releasing.md) · [Security policy](%s/SECURITY.md) · [Changelog](%s/CHANGELOG.md) · [ABI](%s/docs/abi.md)\n\n' \
  "$source_url" "$source_url" "$source_url" "$source_url"

printf '### Source identity\n\n'
printf '| Field | Value |\n|---|---|\n'
printf '| Version | `%s` |\n' "$version"
printf '| Commit | `%s` |\n' "$commit"
printf '| Tree | `%s` |\n' "$(jq -er '.source.tree' "$record")"
printf '| Archive | `%s` (%s bytes) |\n' "$(jq -er '.archive.name' "$record")" "$(jq -er '.archive.bytes' "$record")"
printf '| Archive SHA-256 | `%s` |\n' "$(jq -er '.archive.sha256' "$record")"
printf '| C ABI | `%s.%s` (%s) |\n\n' "$(jq -er '.abi.major' "$record")" "$(jq -er '.abi.minor' "$record")" "$(jq -er '.abi.status' "$record")"

printf '### Capabilities\n\n'
jq -r '.capabilities[] | "- `" + . + "`"' "$record"
printf '\n'

printf '### Semantic inputs\n\n'
printf '| Module | Owner | Release | Commit | Corpus SHA-256 | Independently verified |\n|---|---|---|---|---|---|\n'
jq -r '.semantic_inputs[] | "| " + .module + " | `" + .repository + "` | " + .tag + " | `" + .commit[0:12] + "` | `" + .corpus_sha256[0:12] + "…` | " + (if .release_verified then "yes" else "no" end) + " |"' "$record"
printf '\n'

printf '### Changes\n\n'
if [ -n "$previous" ]; then
  printf 'Commits since %s:\n\n' "$previous"
  git log --no-merges --format='- %s (%h)' "$previous..$commit"
else
  printf 'First published release of this line.\n'
fi
printf '\n'
printf 'The downstream PHP extension `kumwe/kumwe-engine` embeds this exact archive and is published under the same version.\n'
