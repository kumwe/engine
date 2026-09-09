#!/usr/bin/env bash
# Tag the exact tested commit and publish the assembled source bundle as a GitHub release.
# Tags are never moved and assets are never replaced; a rerun verifies existing bytes and
# uploads only what is missing. Requires gh with GH_TOKEN (contents: write).
# Exit status 3 means the tag was published from another commit while this run was in flight;
# the workflow then starts a fresh run on the default branch, which declares the next patch.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
bundle="${1:?usage: release-publish.sh BUNDLE_DIRECTORY [PROVENANCE_BUNDLE]}"
provenance="${2:-}"
fail() { printf '%s\n' "$*" >&2; exit 1; }
cd "$root"
record="$bundle/source.json"
version="$(jq -er '.version' "$record")"
tag="$(jq -er '.tag' "$record")"
commit="$(jq -er '.source.commit' "$record")"
[ "$(git rev-parse HEAD)" = "$commit" ] || fail "The checkout ($(git rev-parse HEAD)) is not the bundled commit $commit"
(cd "$bundle" && sha256sum --check --strict SHA256SUMS)
assets=(kumwe-engine-source.tar.gz SHA256SUMS source.json source.spdx.json)
signature='build-provenance.sigstore.json'
if [ -n "$provenance" ]; then
  [ -f "$provenance" ] || fail "Missing provenance bundle $provenance"
  cp "$provenance" "$bundle/$signature"
  assets+=("$signature")
fi

peeled() {
  git ls-remote --tags origin "refs/tags/$1" "refs/tags/$1^{}" \
    | awk '$2 ~ /\^\{\}$/ { peeled = $1 } $2 !~ /\^\{\}$/ && $2 != "" { plain = $1 }
           END { if (peeled != "") print peeled; else if (plain != "") print plain }'
}
published="$(peeled "$tag")"
if [ -z "$published" ]; then
  git -c "user.name=${GIT_AUTHOR_NAME:?}" -c "user.email=${GIT_AUTHOR_EMAIL:?}" \
    tag --annotate --message "Kumwe Engine $version" "$tag" "$commit"
  git push origin "refs/tags/$tag"
  printf 'Created %s at %s\n' "$tag" "$commit"
elif [ "$published" != "$commit" ]; then
  printf '::warning::%s already identifies %s; it was published while this run tested %s, which is superseded. A published tag is never moved.\n' "$tag" "$published" "$commit"
  exit 3
else
  printf '%s already identifies %s\n' "$tag" "$commit"
fi

if ! gh release view "$tag" --json id > /dev/null 2>&1; then
  notes="$(mktemp)"
  bash tools/release-notes.sh "$bundle" > "$notes"
  (cd "$bundle" && gh release create "$tag" --verify-tag --title "Kumwe Engine $version" --notes-file "$notes" "${assets[@]}")
  printf 'Published release %s\n' "$tag"
else
  printf 'Release %s exists; verifying its assets\n' "$tag"
  existing="$(gh release view "$tag" --json assets --jq '.assets[].name')"
  scratch="$(mktemp -d)"
  for asset in "${assets[@]}"; do
    if printf '%s\n' "$existing" | grep -qx "$asset"; then
      if [ "$asset" = "$signature" ]; then
        # Every attestation run signs fresh bytes; the first published bundle is the record.
        printf 'Provenance bundle already attached; keeping the published one\n'
        continue
      fi
      gh release download "$tag" --pattern "$asset" --dir "$scratch/$asset.d"
      cmp "$scratch/$asset.d/$asset" "$bundle/$asset" || fail "Published asset $asset differs from this bundle; assets are never replaced"
    else
      (cd "$bundle" && gh release upload "$tag" "$asset")
      printf 'Uploaded missing asset %s\n' "$asset"
    fi
  done
  rm -rf "$scratch"
fi
gh release view "$tag" --json tagName,isDraft,isPrerelease,url --jq '"Release " + .tagName + " published=" + (.isDraft | not | tostring) + " " + .url'
