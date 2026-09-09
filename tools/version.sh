#!/usr/bin/env bash
# The declared Engine version lives in resources/capabilities.json only; CMake reads it from there.
#
#   version.sh get          print the declared version
#   version.sh set X.Y.Z    declare a new version and refresh the handoff manifest digest
#   version.sh next         print the next patch above the declared version and every published vMAJOR.MINOR.* tag
#   version.sh resolve      GitHub Actions: write sha/version/tag/release outputs; on the release branch,
#                           declare and push a patch bump when the declared version is already published
#                           (or when FORCE_BUMP=true), adopt a pending bump commit pushed by an earlier run,
#                           and complete a release whose tag exists without its GitHub release
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
  local listing
  listing="$(git -C "$root" ls-remote --tags origin "refs/tags/$1" "refs/tags/$1^{}")" || fail 'Cannot query published tags'
  printf '%s\n' "$listing" \
    | awk '$2 ~ /\^\{\}$/ { peeled = $1 } $2 !~ /\^\{\}$/ && $2 != "" { plain = $1 }
           END { if (peeled != "") print peeled; else if (plain != "") print plain }'
}

# Whether the GitHub release for a tag exists; without gh or a token this reports "no" and the
# idempotent publisher then verifies or completes the release itself.
release_exists() {
  command -v gh > /dev/null 2>&1 && gh release view "$1" --json id > /dev/null 2>&1
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
  local version="$1" major minor patch highest line tag tags
  require_semver "$version"
  IFS=. read -r major minor patch <<< "$version"
  highest="$patch"
  tags="$(git -C "$root" ls-remote --tags --refs origin "refs/tags/v$major.$minor.*")" || fail 'Cannot list published tags'
  while IFS= read -r line; do
    tag="${line##*refs/tags/v}"
    [[ "$tag" =~ $semver ]] || continue
    IFS=. read -r _ _ patch <<< "$tag"
    if (( patch > highest )); then highest="$patch"; fi
  done <<< "$tags"
  printf '%s.%s.%s\n' "$major" "$minor" "$((highest + 1))"
}

# Declare the next patch on top of the tested commit and push it; every later job tests that commit.
bump() {
  local next
  next="$(next_version "$version")"
  printf '::notice::Declaring %s for this run (%s is %s).\n' "v$next" "$tag" "$1"
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
}

# An earlier run may have pushed "Release vN" on top of this commit and then failed before tagging it.
# Re-running that earlier run must finish that release instead of stacking another bump.
adopt_pending_bump() {
  local tip parent subject pending
  tip="$(git -C "$root" ls-remote --heads origin "refs/heads/${RELEASE_BRANCH:?}" | cut -f 1)"
  [ -n "$tip" ] && [ "$tip" != "$sha" ] || return 1
  git -C "$root" fetch --quiet origin "$RELEASE_BRANCH"
  parent="$(git -C "$root" rev-parse --verify --quiet "$tip^" || true)"
  [ "$parent" = "$sha" ] || return 1
  subject="$(git -C "$root" log -1 --format=%s "$tip")"
  [[ "$subject" =~ ^Release\ v((0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*))$ ]] || return 1
  pending="${BASH_REMATCH[1]}"
  [ "$(git -C "$root" show "$tip:resources/capabilities.json" | jq -er '.version')" = "$pending" ] || return 1
  [ -z "$(remote_tag_commit "v$pending")" ] || return 1
  printf '::notice::Adopting the pending bump commit %s (%s) pushed on top of this commit by an earlier run.\n' "$tip" "v$pending"
  sha="$tip"
  version="$pending"
  tag="v$pending"
  release=true
}

resolve() {
  local version sha tag release=false published
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
      if [ "${FORCE_BUMP:-false}" = "true" ]; then
        bump 'already published from this exact commit; a patch bump was requested'
      elif release_exists "$tag"; then
        printf '::notice::%s already identifies this exact commit and its release exists; nothing new to release.\n' "$tag"
      else
        release=true
        printf '::notice::%s already identifies this exact commit but its GitHub release is missing; the release job completes it.\n' "$tag"
      fi
    elif ! adopt_pending_bump; then
      bump "already published from $published"
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
