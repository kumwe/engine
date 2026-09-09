#!/usr/bin/env bash
# The declared Engine version lives in resources/capabilities.json only; CMake reads it from there.
#
# Policy: every change to released source (everything the source archive exports; .gitattributes lists what
# is excluded) declares a new version in that file, in the same change. The default-branch workflow releases
# whatever version the file declares and never asks a person to tag, bump, delete or re-run anything.
#
#   version.sh get           print the declared version
#   version.sh set X.Y.Z     declare a new version and refresh the handoff manifest digest
#   version.sh next          print the next patch above the declared version and every published vMAJOR.MINOR.* tag
#   version.sh digest [REV]  print the released-source identity of a commit (exported paths with their blob ids)
#   version.sh check         pull requests: fail when released source changed without declaring a new version
#   version.sh resolve       GitHub Actions default branch: write sha/version/tag/release/followup/bump/test outputs;
#                            complete a tag whose GitHub release is missing, release the declared version when
#                            it is unreleased, and declare a patch bump when released source changed without one
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
capabilities="$root/resources/capabilities.json"
handoff="$root/MIGRATION-HANDOFF.md"
repository="${GITHUB_REPOSITORY:-kumwe/engine}"
semver='^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$'

fail() { printf '::error::%s\n' "$*" >&2; exit 1; }
notice() { printf '::notice::%s\n' "$*"; }
require_semver() { [[ "$1" =~ $semver ]] || fail "Not a MAJOR.MINOR.PATCH version: $1"; }
declared() { jq -er '.version' "$capabilities"; }
declared_at() { git -C "$root" show "$1:resources/capabilities.json" | jq -er '.version'; }

# The commit a published tag identifies (annotated tags are peeled), or empty when it does not exist.
remote_tag_commit() {
  local listing
  listing="$(git -C "$root" ls-remote --tags origin "refs/tags/$1" "refs/tags/$1^{}")" || fail 'Cannot query published tags'
  printf '%s\n' "$listing" \
    | awk '$2 ~ /\^\{\}$/ { peeled = $1 } $2 !~ /\^\{\}$/ && $2 != "" { plain = $1 }
           END { if (peeled != "") print peeled; else if (plain != "") print plain }'
}

# Every vMAJOR.MINOR.PATCH tag on origin, lowest first.
remote_version_tags() {
  git -C "$root" ls-remote --tags --refs origin 'refs/tags/v*' | sed 's|.*refs/tags/||' \
    | grep -E '^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$' | sort -V || true
}

# Every tag with a published (non-draft) GitHub release. Requires gh with a token.
published_release_tags() {
  gh release list --repo "$repository" --limit 1000 --json tagName,isDraft \
    | jq -r '.[] | select(.isDraft | not) | .tagName' || fail 'Cannot list published releases'
}

# The lowest version tag without a published release: an earlier run failed after tagging, or left a draft.
dangling_tag() {
  comm -23 <(remote_version_tags | LC_ALL=C sort) <(published_release_tags | LC_ALL=C sort) | sort -V | head -n 1
}

# Whether the GitHub release for a tag exists; without gh or a token this reports "no" and the
# idempotent publisher then verifies or completes the release itself.
release_exists() {
  command -v gh > /dev/null 2>&1 && gh release view "$1" --repo "$repository" --json id > /dev/null 2>&1
}

# Identity of the released source of a commit: every exported path with its blob id. Commits that differ
# only in export-ignored files (workflow, release tooling, oracles) share it; the archive prefix, pax
# header and compression play no part, so it is comparable across commits and clones.
export_digest() {
  local commit="$1" exported
  git -C "$root" cat-file -e "$commit^{commit}" 2> /dev/null \
    || git -C "$root" fetch --quiet origin "$commit" 2> /dev/null \
    || fail "Commit $commit is available neither locally nor from origin"
  exported="$(mktemp)"
  git -C "$root" archive --format=tar "$commit" | tar -t | grep -v '/$' | grep -vx 'pax_global_header' | LC_ALL=C sort > "$exported"
  git -C "$root" ls-tree -r "$commit" \
    | awk -F '\t' -v list="$exported" 'BEGIN { while ((getline line < list) > 0) keep[line] = 1 } ($2 in keep)' \
    | sha256sum | cut -d ' ' -f 1
  rm -f "$exported"
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

# Pull requests: the declared version must be unreleased, or the change must leave released source untouched.
check() {
  local version sha tag published next
  version="$(declared)"
  require_semver "$version"
  tag="v$version"
  sha="$(git -C "$root" rev-parse HEAD)"
  published="$(remote_tag_commit "$tag")"
  if [ -z "$published" ]; then
    printf '%s is not published yet; merging releases this source as %s.\n' "$tag" "$tag"
  elif [ "$(export_digest "$sha")" = "$(export_digest "$published")" ]; then
    printf '%s is published from %s and this change leaves released source untouched; merging publishes nothing new.\n' "$tag" "$published"
  else
    next="$(next_version "$version")"
    fail "This change alters released source while resources/capabilities.json still declares $version, which is published from $published. Declare the next version in the same change: bash tools/version.sh set $next (or a minor/major version), then commit resources/capabilities.json and MIGRATION-HANDOFF.md. See docs/releasing.md."
  fi
}

# Declare the next patch on top of the tested commit and push it; every later job tests that commit.
bump() {
  local next
  next="$(next_version "$version")"
  notice "Declaring v$next for this run ($tag is $1)."
  set_version "$next"
  git -C "$root" -c "user.name=${GIT_AUTHOR_NAME:?}" -c "user.email=${GIT_AUTHOR_EMAIL:?}" \
    commit --quiet --all --message "Release v$next"
  if git -C "$root" push origin "HEAD:refs/heads/${RELEASE_BRANCH:?}"; then
    version="$next"
    tag="v$next"
    sha="$(git -C "$root" rev-parse HEAD)"
    release=true
  else
    git -C "$root" reset --quiet --hard "$sha"
    fail "Released source changed without a declared version ($tag is $1) and the patch bump could not be pushed to $RELEASE_BRANCH. Allow GitHub Actions to push to $RELEASE_BRANCH, or declare the next version in a pull request: bash tools/version.sh set $next."
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
  [ "$(declared_at "$tip")" = "$pending" ] || return 1
  [ -z "$(remote_tag_commit "v$pending")" ] || return 1
  notice "Adopting the pending bump commit $tip (v$pending) pushed on top of this commit by an earlier run."
  sha="$tip"
  version="$pending"
  tag="v$pending"
  release=true
}

resolve() {
  local version sha head tag release=false followup=false test=true published dangling
  version="$(declared)"
  require_semver "$version"
  head="$(git -C "$root" rev-parse HEAD)"
  sha="$head"
  tag="v$version"
  if [ "${RELEASE_ENABLED:-false}" = "true" ]; then
    dangling="$(dangling_tag)"
    if [ -n "$dangling" ]; then
      # Complete it from the tagged commit with this run's tooling. Tags are never moved or deleted; the
      # default-branch tip is evaluated again by a follow-up run once the missing release is published.
      published="$(remote_tag_commit "$dangling")"
      [ -n "$published" ] || fail "Cannot resolve the commit of $dangling"
      version="$(declared_at "$published")"
      require_semver "$version"
      [ "v$version" = "$dangling" ] || fail "$dangling identifies $published, which declares $version"
      tag="$dangling"
      sha="$published"
      release=true
      if [ "$sha" = "$head" ]; then
        notice "$tag identifies this exact commit but its GitHub release is not published; the release job completes it."
      else
        # The tagged commit passed every lane before it was tagged; the current lanes belong to this tree.
        test=false
        followup=true
        notice "$tag identifies $sha but its GitHub release is not published (an earlier run failed after tagging). This run completes that release from the tagged commit; a follow-up run then evaluates this commit ($head)."
      fi
    else
      published="$(remote_tag_commit "$tag")"
      if [ -z "$published" ]; then
        release=true
        notice "$tag is not published yet; this commit is released as $tag once every quality lane passes."
      elif [ "$published" = "$sha" ]; then
        if [ "${FORCE_BUMP:-false}" = "true" ]; then
          bump 'already published from this exact commit; a patch bump was requested'
        else
          notice "$tag already identifies this exact commit and its release is published; nothing new to release."
        fi
      elif adopt_pending_bump; then
        :
      elif [ "${FORCE_BUMP:-false}" != "true" ] && [ "$(export_digest "$sha")" = "$(export_digest "$published")" ]; then
        notice "$tag is published from $published and this commit leaves released source untouched; nothing new to release."
      else
        bump "already published from $published"
      fi
    fi
  fi
  {
    printf 'sha=%s\n' "$sha"
    printf 'version=%s\n' "$version"
    printf 'tag=%s\n' "$tag"
    printf 'release=%s\n' "$release"
    printf 'followup=%s\n' "$followup"
    printf 'bump=%s\n' "${FORCE_BUMP:-false}"
    printf 'test=%s\n' "$test"
  } >> "${GITHUB_OUTPUT:-/dev/stdout}"
}

case "${1:-}" in
  get) declared ;;
  set) set_version "${2:?usage: version.sh set X.Y.Z}" ;;
  next) next_version "$(declared)" ;;
  digest) export_digest "$(git -C "$root" rev-parse "${2:-HEAD}")" ;;
  check) check ;;
  resolve) resolve ;;
  *) fail 'usage: version.sh get | set X.Y.Z | next | digest [REV] | check | resolve' ;;
esac
