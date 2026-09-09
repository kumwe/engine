#!/usr/bin/env bash
# Assemble the immutable source release bundle for the committed HEAD into a new directory:
#   kumwe-engine-source.tar.gz  reproducible committed export (git archive, prefix kumwe-engine/, gzip -n)
#   source.spdx.json            complete SPDX inventory of exactly that export
#   source.json                 exact source, archive, ABI, capability and semantic-input identities
#   SHA256SUMS                  digests of the three files above
# It never tags, signs or publishes; the workflow does that after every quality lane passes.
set -euo pipefail
# KUMWE_ROOT points the tooling at another checkout of this repository: the workflow completes a release
# for an older tagged commit with the current scripts by checking that commit out separately.
root="${KUMWE_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
out="${1:?usage: release-bundle.sh NEW_DIRECTORY}"
fail() { printf '%s\n' "$*" >&2; exit 1; }
[ ! -e "$out" ] || fail "Refusing to overwrite $out"
cd "$root"
[ -z "$(git status --porcelain --untracked-files=no)" ] || fail 'Commit tracked changes before assembling a release bundle'
commit="$(git rev-parse HEAD)"
tree="$(git rev-parse 'HEAD^{tree}')"
committed_at="$(TZ=UTC git show -s --date=iso-strict-local --format=%cd HEAD | sed 's/+00:00$/Z/')"
version="$(bash tools/version.sh get)"
archive='kumwe-engine-source.tar.gz'
mkdir -p "$out"
out="$(cd "$out" && pwd)"

# Reproducible bytes: the committed export must not depend on the working tree or the clock.
bash tools/source-archive.sh
cp "artifacts/$archive" "$out/$archive"
bash tools/source-archive.sh
cmp "artifacts/$archive" "$out/$archive"

# Content policy: build residue, development-only tooling, oracles and credential-like files never ship.
entries="$(tar -tzf "$out/$archive")"
if printf '%s\n' "$entries" | grep -vE '^kumwe-engine/'; then fail 'Every archive entry must live under kumwe-engine/'; fi
if printf '%s\n' "$entries" | grep -E '(^|/)(\.git|\.github|vendor|build|node_modules|artifacts|__pycache__)/|\.(php|phar|py|mjs|cjs|js|ts|pem|key)$'; then
  fail 'The source archive contains material that is not source distribution content'
fi

node tools/source-sbom.mjs > "$out/source.spdx.json"
# The SPDX inventory must describe exactly the exported archive.
diff <(printf '%s\n' "$entries" | grep -v '/$' | sed 's|^kumwe-engine/||' | LC_ALL=C sort) \
     <(jq -r '.files[].fileName' "$out/source.spdx.json" | sed 's|^\./||' | LC_ALL=C sort) \
  || fail 'SPDX inventory differs from the archive contents'

archive_sha256="$(sha256sum "$out/$archive" | cut -d ' ' -f 1)"
archive_bytes="$(wc -c < "$out/$archive" | tr -d ' ')"
sbom_sha256="$(sha256sum "$out/source.spdx.json" | cut -d ' ' -f 1)"
jq -n --arg version "$version" --arg commit "$commit" --arg tree "$tree" --arg committed_at "$committed_at" \
  --arg archive "$archive" --arg archive_sha256 "$archive_sha256" --argjson archive_bytes "$archive_bytes" \
  --arg sbom_sha256 "$sbom_sha256" \
  --slurpfile capabilities resources/capabilities.json \
  --slurpfile abi resources/abi-manifest.json \
  --slurpfile contracts resources/contracts.json '
  if $capabilities[0].version != $version then error("capabilities version disagrees with the declared version") else . end
  | {
    schema: "kumwe-engine-source-release/v1",
    package: "kumwe/engine",
    version: $version,
    tag: ("v" + $version),
    source: {
      repository: "https://github.com/kumwe/engine",
      commit: $commit,
      tree: $tree,
      committed_at: $committed_at
    },
    archive: {
      name: $archive,
      prefix: "kumwe-engine/",
      sha256: $archive_sha256,
      bytes: $archive_bytes,
      recipe: "git archive --format=tar --prefix=kumwe-engine/ COMMIT | gzip -n"
    },
    sbom: {name: "source.spdx.json", sha256: $sbom_sha256},
    abi: {major: $abi[0].abi_major, minor: $abi[0].abi_minor, status: $abi[0].status},
    capabilities: $capabilities[0].capabilities,
    corpora: $capabilities[0].corpora,
    semantic_inputs: [$contracts[0].modules[] | {
      module: .module,
      repository: .semantic_release.repository,
      version: .semantic_release.version,
      tag: .semantic_release.tag,
      commit: .semantic_release.commit,
      corpus: .corpus,
      corpus_sha256: .corpus_sha256,
      release_verified: .release_verified,
      external_attestation: .semantic_release.external_attestation
    }],
    computation_baseline: $contracts[0].computation_baseline
  }' > "$out/source.json"
(cd "$out" && sha256sum "$archive" source.spdx.json source.json > SHA256SUMS)
printf 'Assembled kumwe/engine %s source bundle for %s in %s (%s bytes, sha256 %s)\n' \
  "$version" "$commit" "$out" "$archive_bytes" "$archive_sha256"
