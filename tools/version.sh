#!/usr/bin/env bash
# The declared Engine version lives in resources/capabilities.json only; CMake reads it from there.
#
#   version.sh get          print the declared version
#   version.sh set X.Y.Z    declare a new version and refresh the handoff manifest digest
#   version.sh next         print the next patch above the declared version and every published vMAJOR.MINOR.* tag
#   version.sh resolve      GitHub Actions: write sha/version/tag/release outputs; on the release branch,
#                           declare and push a patch bump when the declared version is already published
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
capabilities="$root/resources/capabilities.json"
handoff="$root/MIGRATION-HANDOFF.md"
semver='^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$'

fail() { printf '%s\n' "$*" >&2; exit 1; }
require_semver() { [[ "$1" =~ $semver ]] || fail "Not a MAJOR.MINOR.PATCH version: $1"; }
declared() { jq -er '.version' "$capabilities"; }

# The commit a published tag identifies (annotated tags are peeled), or empty when it does not exist.
remote_tag_commit() {
  git -C "$root" ls-remote --tags origin "refs/tags/$1" "refs/tags/$1^{}" \
    | awk '$2 ~ /\^\{\}$/ { peeled = $1 } $2 !~ /\^\{\}$/ { plain = $1 }
           END { if (peeled != "") print peeled; else if (plain != "") print plain }'
}

set_version() {
  local version="$1" temporary digest
  require_semver "$version"
  temporary="$(mktemp)"
  jq --arg version "$version" '.version = $version | .computation.engine_version = $version' "$capabilities" > "$temporary"
  mv "$temporary" "$capabilities"
  # The native handoff binds the public manifest digests; keep its capabilities entry truthful.
  digest="$(sha256sum "$capabilities" | cut -d ' ' -f 1)"
  temporary="$(mktemp)"
  awk -v digest="$digest" '
    pending && /sha256: "[0-9a-f]{64}"/ { sub(/sha256: "[0-9a-f]{64}"/, "sha256: \"" digest "\""); pending = 0 }
    /path: "resources\/capabilities.json"/ { pending = 1 }
    { print }' "$handoff" > "$temporary"
  mv "$temporary" "$handoff"
  printf 'Declared Engine version %s\n' "$version"
}

next_version() {
  local version="$1" major minor patch highest tag
  require_semver "$version"
  IFS=. read -r major minor patch <<< "$version"
  highest="$patch"
  while IFS= read -r tag; do
    [[ "$tag" =~ $semver ]] || continue
    IFS=. read -r _ _ patch <<< "$tag"
    if (( patch > highest )); then highest="$patch"; fi
  done < <(git -C "$root" ls-remote --tags --refs origin "refs/tags/v$major.$minor.*" | sed -E 's|^.*refs/tags/v||')
  printf '%s.%s.%s\n' "$major" "$minor" "$((highest + 1))"
}

resolve() {
  local version sha tag release=false published next
  version="$(declared)"
  require_semver "$version"
  sha="$(git -C "$root" rev-parse HEAD)"
  tag="v$version"
  if [ "${RELEASE_ENABLED:-false}" = "true" ]; then
    published="$(remote_tag_commit "$tag")"
    if [ -z "$published" ]; then
      release=true
      printf '::notice::%s is not published yet; this commit is released as %s once every quality lane passes.\n' "$tag" "$tag"
    elif [ "$published" = "$sha" ]; then
      printf '::notice::%s already identifies this exact commit; nothing new to release.\n' "$tag"
    else
      next="$(next_version "$version")"
      printf '::notice::%s already identifies %s; declaring %s for this merge.\n' "$tag" "$published" "$next"
      set_version "$next"
      git -C "$root" -c "user.name=${GIT_AUTHOR_NAME:?}" -c "user.email=${GIT_AUTHOR_EMAIL:?}" \
        commit --quiet --all --message "Release v$next"
      if git -C "$root" push origin "HEAD:refs/heads/${RELEASE_BRANCH:?}"; then
        version="$next"
        tag="v$next"
        sha="$(git -C "$root" rev-parse HEAD)"
        release=true
      else
        printf '::warning::Could not push the version bump to %s (the branch advanced or direct pushes are protected). The quality lanes still test %s; the newer commit, or a version bump made in a pull request, releases next.\n' "$RELEASE_BRANCH" "$sha"
        git -C "$root" reset --quiet --hard "$sha"
      fi
    fi
  fi
  {
    printf 'sha=%s\n' "$sha"
    printf 'version=%s\n' "$version"
    printf 'tag=%s\n' "$tag"
    printf 'release=%s\n' "$release"
  } >> "${GITHUB_OUTPUT:-/dev/stdout}"
}

case "${1:-}" in
  get) declared ;;
  set) set_version "${2:?usage: version.sh set X.Y.Z}" ;;
  next) next_version "$(declared)" ;;
  resolve) resolve ;;
  *) fail 'usage: version.sh get | set X.Y.Z | next | resolve' ;;
esac
