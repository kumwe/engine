#!/usr/bin/env bash
# Self-test for tools/version.sh: version declaration, released-source identity, the pull-request check
# and every default-branch resolution outcome, against a disposable origin and a simulated gh.
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
export GIT_AUTHOR_NAME=Lemuel GIT_AUTHOR_EMAIL=lemuel@vdm.to GIT_COMMITTER_NAME=Lemuel GIT_COMMITTER_EMAIL=lemuel@vdm.to
export GITHUB_REPOSITORY=kumwe/engine RELEASE_BRANCH=main GH_STATE="$work/gh"
export GH_TOKEN=simulated
unset FORCE_BUMP RELEASE_ENABLED GITHUB_OUTPUT
mkdir -p "$work/bin"
export PATH="$work/bin:$PATH"
passed=0

# Simulated gh: a file release.TAG under GH_STATE is a published release; content "true" marks a draft.
cat > "$work/bin/gh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
state="${GH_STATE:?}"
case "${1:-} ${2:-}" in
  "release list")
    printf '['
    first=true
    for file in "$state"/release.*; do
      [ -e "$file" ] || continue
      draft=false
      [ "$(cat "$file")" = "true" ] && draft=true
      $first || printf ','
      first=false
      printf '{"tagName":"%s","isDraft":%s}' "${file##*/release.}" "$draft"
    done
    printf ']\n' ;;
  "release view") [ -e "$state/release.$3" ] ;;
  *) printf 'unsupported gh %s\n' "$*" >&2; exit 96 ;;
esac
EOF
chmod +x "$work/bin/gh"

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
ok() { passed=$((passed + 1)); printf 'ok %2d %s\n' "$passed" "$1"; }
expect() { [ "$2" = "$3" ] || fail "$1: expected '$3', got '$2'"; }
output() { grep "^$1=" "$GITHUB_OUTPUT" | tail -n 1 | cut -d = -f 2-; }
head_sha() { git -C "$work/clone" rev-parse HEAD; }
origin_main() { git -C "$work/origin.git" rev-parse refs/heads/main; }
resolve() { : > "$GITHUB_OUTPUT"; RELEASE_ENABLED=true bash tools/version.sh resolve; }
release() { printf '%s' "${2:-false}" > "$GH_STATE/release.$1"; }
tag_head() { git tag --annotate --message "Kumwe Engine ${1#v}" "$1" HEAD && git push --quiet origin "refs/tags/$1"; }
change_released() { printf 'int main(void) { return %s; }\n' "$1" > src.c && git commit --quiet --all --message "Change released source ($1)"; }
change_ignored() { mkdir -p .github && printf 'lane: %s\n' "$1" > .github/ci.yml && git add .github && git commit --quiet --message "Change ignored source ($1)"; }

# Seed repository: the real version.sh, a declared version, the handoff digest line and export rules.
git init --quiet --initial-branch=main "$work/seed"
cd "$work/seed"
mkdir -p resources tools
printf '{\n  "version": "1.0.0",\n  "computation": {\n    "engine_version": "1.0.0"\n  }\n}\n' > resources/capabilities.json
printf '  public_manifests:\n  - path: "resources/capabilities.json"\n    sha256: "%064d"\n' 0 > MIGRATION-HANDOFF.md
printf '/.github export-ignore\n/.gitattributes export-ignore\n/tools/version.sh export-ignore\n' > .gitattributes
cp "$here/tools/version.sh" tools/version.sh
printf 'int main(void) { return 0; }\n' > src.c
git add --all
git commit --quiet --message 'Seed'
bash tools/version.sh set 1.0.0 > /dev/null
git commit --quiet --all --message 'Declare 1.0.0'

# Every scenario starts from a fresh origin, clone and gh state.
fresh() {
  cd "$work"
  rm -rf "$work/origin.git" "$work/clone" "$GH_STATE"
  mkdir -p "$GH_STATE"
  git clone --quiet --bare "$work/seed" "$work/origin.git"
  git clone --quiet "$work/origin.git" "$work/clone"
  cd "$work/clone"
  export GITHUB_OUTPUT="$work/outputs"
  : > "$GITHUB_OUTPUT"
}

fresh
expect 'get' "$(bash tools/version.sh get)" '1.0.0'
bash tools/version.sh set 1.2.3 > /dev/null
expect 'set version' "$(jq -r .version resources/capabilities.json)" '1.2.3'
expect 'set engine_version' "$(jq -r .computation.engine_version resources/capabilities.json)" '1.2.3'
expect 'set handoff digest' "$(grep -o '[a-f0-9]\{64\}' MIGRATION-HANDOFF.md)" "$(sha256sum resources/capabilities.json | cut -d ' ' -f 1)"
git checkout --quiet -- .
ok 'set declares the version in capabilities.json and refreshes the handoff digest'

before="$(bash tools/version.sh digest)"
change_ignored one
expect 'digest after ignored change' "$(bash tools/version.sh digest)" "$before"
change_released 1
[ "$(bash tools/version.sh digest)" != "$before" ] || fail 'digest must change with released source'
expect 'digest of an older commit' "$(bash tools/version.sh digest HEAD~2)" "$before"
ok 'digest ignores export-ignored files and follows released source'

fresh
tag_head v1.0.0
git tag --annotate --message x v1.0.7 HEAD && git push --quiet origin refs/tags/v1.0.7
git tag --annotate --message x v1.1.2 HEAD && git push --quiet origin refs/tags/v1.1.2
expect 'next' "$(bash tools/version.sh next)" '1.0.8'
ok 'next is one patch above the declared line and every published tag of that line'

fresh
expect 'check unreleased' "$(bash tools/version.sh check)" 'v1.0.0 is not published yet; merging releases this source as v1.0.0.'
ok 'check accepts an unreleased declared version'

fresh
tag_head v1.0.0
change_ignored one
bash tools/version.sh check | grep -q 'leaves released source untouched' || fail 'check must accept ignored-only changes'
ok 'check accepts a change that leaves released source untouched'

fresh
tag_head v1.0.0
change_released 1
if bash tools/version.sh check > "$work/check.log" 2>&1; then fail 'check must refuse released changes without a version'; fi
grep -q '::error::This change alters released source while resources/capabilities.json still declares 1.0.0' "$work/check.log" || fail "unexpected check output: $(cat "$work/check.log")"
grep -q 'bash tools/version.sh set 1.0.1' "$work/check.log" || fail 'check must name the next version'
bash tools/version.sh set 1.0.1 > /dev/null
git commit --quiet --all --message 'Declare 1.0.1'
expect 'check after bump' "$(bash tools/version.sh check)" 'v1.0.1 is not published yet; merging releases this source as v1.0.1.'
ok 'check refuses released changes without a new version and accepts the declared bump'

fresh
bash tools/version.sh resolve
expect 'pull request release' "$(output release)" 'false'
expect 'pull request sha' "$(output sha)" "$(head_sha)"
ok 'resolve outside the default branch only reports the commit and version'

fresh
resolve > /dev/null
expect 'unreleased release' "$(output release)" 'true'
expect 'unreleased sha' "$(output sha)" "$(head_sha)"
expect 'unreleased tag' "$(output tag)" 'v1.0.0'
expect 'unreleased followup' "$(output followup)" 'false'
expect 'unreleased test' "$(output test)" 'true'
ok 'resolve releases an unreleased declared version from the tested commit'

fresh
tag_head v1.0.0
release v1.0.0
resolve > /dev/null
expect 'published release' "$(output release)" 'false'
ok 'resolve publishes nothing when the tag identifies this commit and its release exists'

fresh
tag_head v1.0.0
resolve > /dev/null
expect 'dangling here release' "$(output release)" 'true'
expect 'dangling here sha' "$(output sha)" "$(head_sha)"
expect 'dangling here followup' "$(output followup)" 'false'
expect 'dangling here test' "$(output test)" 'true'
ok 'resolve completes a tag on this commit whose release is missing'

fresh
tag_head v1.0.0
tagged="$(head_sha)"
change_released 1
bash tools/version.sh set 1.0.1 > /dev/null
git commit --quiet --all --message 'Declare 1.0.1'
git push --quiet origin main
resolve > "$work/resolve.log"
expect 'dangling older release' "$(output release)" 'true'
expect 'dangling older sha' "$(output sha)" "$tagged"
expect 'dangling older version' "$(output version)" '1.0.0'
expect 'dangling older tag' "$(output tag)" 'v1.0.0'
expect 'dangling older followup' "$(output followup)" 'true'
expect 'dangling older test' "$(output test)" 'false'
grep -q 'a follow-up run then evaluates this commit' "$work/resolve.log" || fail 'missing follow-up notice'
release v1.0.0 true
resolve > /dev/null
expect 'draft is dangling' "$(output sha)" "$tagged"
release v1.0.0
resolve > /dev/null
expect 'after completion release' "$(output release)" 'true'
expect 'after completion sha' "$(output sha)" "$(head_sha)"
expect 'after completion tag' "$(output tag)" 'v1.0.1'
expect 'after completion followup' "$(output followup)" 'false'
ok 'resolve completes the lowest tag without a published release first, then the tip in a follow-up'

fresh
tag_head v1.0.0
release v1.0.0
change_ignored one
git push --quiet origin main
resolve > /dev/null
expect 'ignored-only release' "$(output release)" 'false'
expect 'ignored-only origin' "$(origin_main)" "$(head_sha)"
ok 'resolve publishes nothing when released source is unchanged since the published tag'

fresh
tag_head v1.0.0
release v1.0.0
change_released 1
git push --quiet origin main
tested="$(head_sha)"
resolve > "$work/resolve.log"
expect 'bump version' "$(output version)" '1.0.1'
expect 'bump tag' "$(output tag)" 'v1.0.1'
expect 'bump release' "$(output release)" 'true'
expect 'bump sha' "$(output sha)" "$(head_sha)"
expect 'bump pushed' "$(origin_main)" "$(head_sha)"
expect 'bump parent' "$(git rev-parse HEAD^)" "$tested"
expect 'bump subject' "$(git log -1 --format=%s)" 'Release v1.0.1'
expect 'bump author' "$(git log -1 --format='%an <%ae>')" 'Lemuel <lemuel@vdm.to>'
expect 'bump declared' "$(bash tools/version.sh get)" '1.0.1'
ok 'resolve declares and pushes a patch bump when released source changed without one'

fresh
tag_head v1.0.0
release v1.0.0
change_released 1
git push --quiet origin main
tested="$(head_sha)"
printf '#!/bin/sh\necho protected >&2\nexit 1\n' > "$work/origin.git/hooks/pre-receive"
chmod +x "$work/origin.git/hooks/pre-receive"
if resolve > "$work/resolve.log" 2>&1; then fail 'resolve must fail when the bump cannot be pushed'; fi
grep -q '::error::Released source changed without a declared version' "$work/resolve.log" || fail "unexpected failure output: $(cat "$work/resolve.log")"
expect 'refused push keeps the tested commit' "$(head_sha)" "$tested"
expect 'refused push keeps the tree clean' "$(git status --porcelain)" ''
expect 'refused push leaves origin' "$(origin_main)" "$tested"
ok 'resolve fails loudly, without releasing, when the bump push is refused'

fresh
tag_head v1.0.0
release v1.0.0
change_released 1
git push --quiet origin main
tested="$(head_sha)"
bash tools/version.sh set 1.0.1 > /dev/null
git commit --quiet --all --message 'Release v1.0.1'
git push --quiet origin main
pending="$(head_sha)"
git reset --quiet --hard "$tested"
resolve > /dev/null
expect 'adopt sha' "$(output sha)" "$pending"
expect 'adopt version' "$(output version)" '1.0.1'
expect 'adopt release' "$(output release)" 'true'
ok 'resolve adopts a pending bump commit pushed by an earlier run instead of stacking another'

fresh
tag_head v1.0.0
release v1.0.0
FORCE_BUMP=true resolve > /dev/null
expect 'forced version' "$(output version)" '1.0.1'
expect 'forced release' "$(output release)" 'true'
expect 'forced pushed' "$(origin_main)" "$(head_sha)"
ok 'resolve declares the next patch on request when the tip is already released'

printf 'version.sh self-test: %d scenarios passed\n' "$passed"
