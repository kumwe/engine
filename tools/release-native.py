#!/usr/bin/env python3
"""Publish only verified stable native source bundles from a successful default-branch CI run."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from urllib.parse import quote


class ReleaseError(RuntimeError):
    """A release prerequisite or observed immutable identity disagrees."""


def decode_http(raw: bytes, returncode: int, allow_missing: bool = False):
    """Only an actual HTTP 404 authorizes creation; error-message text cannot."""
    first = raw.partition(b'\n')[0]
    match = re.fullmatch(rb'HTTP/[0-9.]+ ([0-9]{3})(?: .*?)?\r?', first)
    if match is None:
        raise ReleaseError('GitHub response has no verifiable HTTP status.')
    status = int(match[1])
    if status == 404 and returncode != 0 and allow_missing:
        return None
    if returncode != 0 or status not in (200, 201):
        raise ReleaseError(f'GitHub request failed with HTTP {status}.')
    marker = b'\r\n\r\n' if b'\r\n\r\n' in raw else b'\n\n'
    try:
        value = json.loads(raw.split(marker, 1)[1])
    except (IndexError, ValueError) as error:
        raise ReleaseError('GitHub returned malformed response JSON.') from error
    if not isinstance(value, dict):
        raise ReleaseError('GitHub response must be an object.')
    return value


class Publisher:
    """Orchestrate existing source gates; never replace a tag, release or asset."""

    def __init__(self, root: Path, environment=None):
        self.root = root.resolve()
        self.env = dict(os.environ if environment is None else environment)
        self.repo = self.env.get('GITHUB_REPOSITORY', '')
        self.sha = self.env.get('GITHUB_SHA', '')
        self.ref = self.env.get('GITHUB_REF', '')
        self.branch = self.env.get('DEFAULT_BRANCH', '')
        self.run_id = self.env.get('QUALITY_RUN_ID', '')
        if self.repo not in ('kumwe/engine', 'kumwe/kumwe-engine'):
            raise ReleaseError('Only the two declared Kumwe native repositories may publish.')
        if not re.fullmatch('[a-f0-9]{40}', self.sha):
            raise ReleaseError('The workflow source must be an exact commit.')
        if not re.fullmatch('[1-9][0-9]*', self.run_id):
            raise ReleaseError('An actual successful quality workflow run is required.')
        if not self.branch or self.ref != 'refs/heads/' + self.branch:
            raise ReleaseError('Only the repository default branch may publish.')
        self.archive = ('kumwe-engine-source.tar.gz' if self.repo == 'kumwe/engine'
                        else 'kumwe-engine-php-source.tar.gz')
        self.files = [self.archive, 'source.spdx.json', 'source.json', 'source.provenance.json', 'SHA256SUMS']
        self.signature = 'build-provenance.sigstore.json'

    def command(self, *args: str, cwd: Path | None = None, data: bytes | None = None,
                check: bool = True):
        result = subprocess.run(args, cwd=cwd or self.root, env=self.env, input=data,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        if check and result.returncode != 0:
            raise ReleaseError(f'Command failed ({args[0]}): {result.stderr.decode(errors="replace").strip()}')
        return result

    def api(self, endpoint: str, *, missing: bool = False, method: str = 'GET', payload=None):
        args = ['gh', 'api', '--include', '--method', method, f'repos/{self.repo}/{endpoint}']
        data = None
        if payload is not None:
            args += ['--input', '-']
            data = json.dumps(payload).encode()
        response = self.command(*args, data=data, check=False)
        return decode_http(response.stdout, response.returncode, missing)

    def check_context(self):
        self.command('git', 'check-ref-format', self.ref)
        if self.command('git', 'rev-parse', 'HEAD').stdout.decode().strip() != self.sha:
            raise ReleaseError('Checkout does not equal the exact workflow commit.')
        self.command('git', 'diff', '--quiet', 'HEAD', '--')
        branch = self.api('branches/' + quote(self.branch, safe=''))
        if branch.get('name') != self.branch or branch.get('commit', {}).get('sha') != self.sha:
            raise ReleaseError('The default branch advanced; qualify its new exact commit first.')
        observed = self.api('actions/runs/' + self.run_id)
        expected_name = 'Native quality' if self.repo == 'kumwe/engine' else 'Native binding candidate'
        if (observed.get('head_sha') != self.sha or observed.get('head_branch') != self.branch
                or observed.get('head_repository', {}).get('full_name') != self.repo
                or observed.get('event') not in ('push', 'workflow_dispatch')
                or observed.get('path') != '.github/workflows/ci.yml'
                or observed.get('name') != expected_name
                or observed.get('status') != 'completed' or observed.get('conclusion') != 'success'):
            raise ReleaseError('Quality evidence is not a successful exact-source default-branch native run.')
        jobs = []
        for page in range(1, 101):
            response = self.api(f'actions/runs/{self.run_id}/jobs?per_page=100&page={page}')
            batch = response.get('jobs')
            if not isinstance(batch, list) or type(response.get('total_count')) is not int:
                raise ReleaseError('Quality job inventory is malformed.')
            jobs.extend(batch)
            if len(jobs) >= response['total_count']:
                if len(jobs) != response['total_count']:
                    raise ReleaseError('Quality job inventory exceeds its declared count.')
                break
            if not batch:
                raise ReleaseError('Quality job inventory ended before the declared count.')
        else:
            raise ReleaseError('Quality job inventory exceeds its bounded verification limit.')
        if not jobs or any(not isinstance(j, dict) or j.get('status') != 'completed'
                           or j.get('conclusion') != 'success' or type(j.get('id')) is not int
                           or not isinstance(j.get('name'), str) for j in jobs):
            raise ReleaseError('Every native quality job must pass; skipped jobs do not qualify.')
        if len({j['id'] for j in jobs}) != len(jobs):
            raise ReleaseError('Quality job inventory repeats an identity.')
        names = [j.get('name', '') for j in jobs]
        if self.repo == 'kumwe/engine':
            required = ['sanitizers-fuzz', 'thread-sanitizer', 'archive-and-faults',
                        'native (ubuntu-24.04, gcc, g++)', 'native (ubuntu-24.04, clang, clang++)',
                        'native (macos-14, clang, clang++)']
        else:
            required = ['source-release-preparation', 'binding', 'address-undefined-sanitizers',
                        'clean-pie', 'whole-boundary-benchmarks']
        if any(name not in names for name in required):
            raise ReleaseError('A required native quality lane is missing.')

    def source_bundle(self, action: str, destination: Path, *, root=None, sha=None):
        selected_root, selected_sha = root or self.root, sha or self.sha
        self.command(sys.executable, str(selected_root / 'tools/release-source.py'), action,
                     str(destination), '--expected-commit', selected_sha, '--require-stable', cwd=selected_root)
        record = json.loads((destination / 'source.json').read_text())
        if (record.get('package') != self.repo or record.get('source', {}).get('commit') != selected_sha
                or record.get('archive', {}).get('name') != self.archive
                or record.get('stable_source_blockers') != []):
            raise ReleaseError('Stable source bundle differs from the expected repository/source.')
        version = record.get('identity', {}).get('version', '')
        if not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', version) or version == '0.0.0':
            raise ReleaseError('Only a recorded stable native version may publish.')
        return record

    def candidate_gate(self):
        if self.repo != 'kumwe/engine':
            return
        result = self.command('node', str(self.root / 'tools/release-validation/candidate-gate.mjs'),
                              str(self.root), self.sha)
        proof = json.loads(result.stdout)
        if proof.get('status') != 'passed' or proof.get('merged_engine_commit') != self.sha:
            raise ReleaseError('The external candidate gate did not verify this merged Engine source.')

    def release_notes(self, record):
        source_url = f'https://github.com/{self.repo}/blob/{self.sha}'
        lines = [f"Source release {record['identity']['version']} from `{self.sha}`.", '',
                 'The attached source archive, complete SPDX inventory and checksums have verified '
                 'GitHub OIDC source provenance. Independent published-release verification remains separate.', '',
                 f'[Release procedure]({source_url}/docs/releasing.md) · '
                 f'[Security policy]({source_url}/SECURITY.md) · [Source changes]({source_url}/CHANGELOG.md)', '',
                 '### Exact dependencies and semantic contracts', '']
        for dependency in record.get('dependencies', []):
            version = dependency.get('version') or dependency.get('release') or 'recorded source'
            lines.append(f"- `{dependency.get('repository', 'unknown')}` {version}, source `{dependency['commit']}`.")
            for field in ('archive_sha256', 'api_digest', 'capability_digest', 'service_map_digest'):
                if dependency.get(field):
                    lines.append(f"  - {field}: `{dependency[field]}`.")
            corpora = dict(dependency.get('corpus_digests', {}))
            if dependency.get('corpus_path'):
                corpora[dependency['corpus_path']] = dependency['corpus_sha256']
            for corpus, digest in sorted(corpora.items()):
                lines.append(f'  - Corpus `{corpus}`: `{digest}`.')
        baseline = record.get('computation_baseline')
        if baseline:
            lines += ['', f"Portable-only prerequisite: `{baseline['repository']}` {baseline['version']} "
                      f"at `{baseline['commit']}`; its exact manifests/corpora and independent evidence are retained in `source.json`."]
        if self.repo == 'kumwe/engine':
            capabilities = json.loads((self.root / 'resources/capabilities.json').read_text()) if (self.root / 'resources/capabilities.json').is_file() else {}
            lines += ['', '### Capabilities and limits', '',
                      'C ABI 1 retains its fixed header, twelve exports, status codes and ownership contract. '
                      'The bounded protocols require explicit profiles, corpus identities and resource budgets.', '',
                      *['- `' + profile + '`' for profile in capabilities.get('capabilities', [])], '',
                      f'[Exact ABI limits and ownership]({source_url}/docs/abi.md) · '
                      f'[Supported platforms and responsibilities]({source_url}/CHARTER.md)', '',
                      'Historical draft/0.0.0 semantic profile tokens retain their original meaning. '
                      'Database, authorization, normalization outside the declared profiles, rendering and delivery stay with their owners.']
        else:
            lines += ['', '### Supported binding and limits', '',
                      'PHP 8.5 NTS, Linux x86_64/glibc, exact PHP patch and configured build tuple. '
                      'A Runtime owns at most 64 plans and 16 MiB encoded source; released and foreign IDs refuse. '
                      'Other PHP versions, ZTS and platforms are unsupported.', '',
                      f'[Complete native PHP API]({source_url}/resources/api/v1.json) · '
                      f'[Handle ownership and transport limits]({source_url}/docs/memory.md)']
        lines += ['', 'Whole-boundary measurements retain slower native workloads as well as speedups. '
                  'Source qualification does not claim universal acceleration, App integration or production capacity.', '',
                  '### Source changes and security', '']
        changelog = self.root / 'CHANGELOG.md'
        if changelog.is_file():
            text = changelog.read_text()
            section = re.search(r'^## Unreleased\s*\n(.*?)(?=^## |\Z)', text, flags=re.M | re.S)
            if section:
                lines.append(section.group(1).strip())
        return '\n'.join(lines).rstrip() + '\n'

    def tag_commit(self, tag: str):
        observed = self.api('git/ref/tags/' + tag, missing=True)
        if observed is None:
            return None
        for _ in range(16):
            obj = observed.get('object', {})
            sha = obj.get('sha', '')
            if not re.fullmatch('[a-f0-9]{40}', sha):
                raise ReleaseError('Version tag has an invalid object identity.')
            if obj.get('type') == 'commit':
                return sha
            if obj.get('type') != 'tag':
                raise ReleaseError('Version tag does not identify a commit.')
            observed = self.api('git/tags/' + sha)
        raise ReleaseError('Annotated tag nesting exceeds the verification limit.')

    def verify_signatures(self, bundle: Path, signature: Path, sha: str):
        if not signature.is_file() or signature.is_symlink():
            raise ReleaseError('The OIDC-signed provenance bundle is missing or unsafe.')
        for name in self.files:
            self.command('gh', 'attestation', 'verify', str(bundle / name), '--repo', self.repo,
                         '--bundle', str(signature), '--source-digest', sha, '--source-ref', self.ref,
                         '--cert-identity', f'https://github.com/{self.repo}/.github/workflows/release.yml@{self.ref}',
                         '--deny-self-hosted-runners')

    def download(self, tag: str, name: str, destination: Path):
        destination.mkdir(parents=True, exist_ok=True)
        self.command('gh', 'release', 'download', tag, '--repo', self.repo, '--dir', str(destination),
                     '--pattern', name)
        path = destination / name
        if not path.is_file() or path.is_symlink():
            raise ReleaseError('Expected release asset is missing or unsafe: ' + name)
        return path

    def require_draft(self, release: dict, tag: str, release_id: int | None = None):
        if (release.get('tag_name') != tag or release.get('draft') is not True
                or release.get('prerelease') is not False or type(release.get('id')) is not int
                or release['id'] <= 0 or (release_id is not None and release['id'] != release_id)):
            raise ReleaseError('Only the same matching unpublished stable draft may receive assets.')

    def verify_draft_assets(self, tag: str, release: dict, bundle: Path):
        names = [entry.get('name') for entry in release.get('assets', [])]
        if sorted(names) != sorted(self.files + [self.signature]):
            raise ReleaseError('Draft must contain exactly the source bundle and signed provenance before publication.')
        with tempfile.TemporaryDirectory(prefix='kumwe-native-draft-verify-') as temporary:
            scratch = Path(temporary)
            for name in self.files:
                actual = self.download(tag, name, scratch)
                if actual.read_bytes() != (bundle / name).read_bytes():
                    raise ReleaseError('Uploaded draft source asset differs from the verified source: ' + name)
            signature = self.download(tag, self.signature, scratch)
            self.verify_signatures(bundle, signature, self.sha)

    def verify_published(self, tag: str, release: dict, *, expected_version: str):
        if (release.get('tag_name') != tag or release.get('draft') is not False
                or release.get('prerelease') is not False or not release.get('published_at')):
            raise ReleaseError('Existing release is not a published stable record.')
        tag_sha = self.tag_commit(tag)
        if tag_sha is None:
            raise ReleaseError('Published release has no version tag.')
        self.command('git', 'merge-base', '--is-ancestor', tag_sha, self.sha)
        assets = release.get('assets', [])
        names = [entry.get('name') for entry in assets]
        if sorted(names) != sorted(self.files + [self.signature]):
            raise ReleaseError('Published release must retain the exact source and signed-provenance assets.')
        with tempfile.TemporaryDirectory(prefix='kumwe-native-release-verify-') as temporary:
            scratch = Path(temporary)
            bundle = scratch / 'bundle'
            for name in self.files:
                self.download(tag, name, bundle)
            signature = self.download(tag, self.signature, scratch)
            source = scratch / 'source'
            self.command('git', 'clone', '--quiet', '--shared', '--no-checkout', str(self.root), str(source))
            self.command('git', 'checkout', '--quiet', '--detach', tag_sha, cwd=source)
            record = self.source_bundle('verify', bundle, root=source, sha=tag_sha)
            if record['identity']['version'] != expected_version:
                raise ReleaseError('Published source version does not match its tag.')
            self.verify_signatures(bundle, signature, tag_sha)
        if self.tag_commit(tag) != tag_sha:
            raise ReleaseError('Published tag changed during verification.')

    def prepare(self, destination: Path):
        self.check_context()
        record = self.source_bundle('prepare', destination)
        version = record['identity']['version']
        tag = 'v' + version
        existing = self.api('releases/tags/' + tag, missing=True)
        already_published = existing is not None and existing.get('draft') is False
        if already_published:
            self.verify_published(tag, existing, expected_version=version)
        else:
            self.candidate_gate()
        return {'version': version, 'tag': tag, 'already_published': str(already_published).lower()}

    def publish(self, destination: Path, signature: Path):
        self.check_context()
        record = self.source_bundle('verify', destination)
        version, tag = record['identity']['version'], 'v' + record['identity']['version']
        existing = self.api('releases/tags/' + tag, missing=True)
        if existing is not None and existing.get('draft') is False:
            self.verify_published(tag, existing, expected_version=version)
            return
        self.candidate_gate()
        self.verify_signatures(destination, signature, self.sha)
        target = self.tag_commit(tag)
        if target is not None and target != self.sha:
            raise ReleaseError('An unpublished version tag must identify this exact tested commit.')
        if target is None:
            if existing is not None:
                raise ReleaseError('A draft release without its expected version tag is inconsistent.')
            created = self.api('git/refs', method='POST', payload={
                'ref': 'refs/tags/' + tag, 'sha': self.sha,
            })
            if (created.get('ref') != 'refs/tags/' + tag
                    or created.get('object', {}).get('type') != 'commit'
                    or created.get('object', {}).get('sha') != self.sha):
                raise ReleaseError('Created tag differs from the verified commit.')
        if existing is None:
            existing = self.api('releases', method='POST', payload={
                'tag_name': tag, 'target_commitish': self.sha, 'name': tag, 'draft': True, 'prerelease': False,
                'body': self.release_notes(record),
            })
        self.require_draft(existing, tag)
        release_id = existing.get('id')
        with tempfile.TemporaryDirectory(prefix='kumwe-native-release-assets-') as temporary:
            scratch = Path(temporary)
            for name in self.files + [self.signature]:
                current = self.api(f'releases/{release_id}')
                self.require_draft(current, tag, release_id)
                matches = [entry for entry in current.get('assets', []) if entry.get('name') == name]
                if len(matches) > 1:
                    raise ReleaseError('Duplicate release asset identity: ' + name)
                expected = signature if name == self.signature else destination / name
                if matches:
                    downloaded = self.download(tag, name, scratch / name.replace('.', '-'))
                    if name == self.signature:
                        self.verify_signatures(destination, downloaded, self.sha)
                    elif hashlib.sha256(downloaded.read_bytes()).digest() != hashlib.sha256(expected.read_bytes()).digest():
                        raise ReleaseError('Existing draft asset differs; it cannot be overwritten: ' + name)
                else:
                    upload = scratch / name
                    shutil.copyfile(expected, upload)
                    self.command('gh', 'release', 'upload', tag, str(upload), '--repo', self.repo)
        current = self.api(f'releases/{release_id}')
        self.require_draft(current, tag, release_id)
        self.verify_draft_assets(tag, current, destination)
        self.check_context()
        if self.tag_commit(tag) != self.sha:
            raise ReleaseError('Version tag changed before publication.')
        self.api(f'releases/{release_id}', method='PATCH', payload={'draft': False})
        observed = self.api('releases/tags/' + tag)
        self.verify_published(tag, observed, expected_version=version)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare', 'publish'))
    parser.add_argument('directory', type=Path)
    parser.add_argument('--provenance', type=Path)
    args = parser.parse_args()
    publisher = Publisher(Path(__file__).resolve().parents[1])
    if args.action == 'prepare':
        outputs = publisher.prepare(args.directory.resolve())
        if os.environ.get('GITHUB_OUTPUT'):
            with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
                for key, value in outputs.items():
                    output.write(f'{key}={value}\n')
        print(json.dumps(outputs, sort_keys=True))
    else:
        if args.provenance is None:
            parser.error('publish requires --provenance')
        publisher.publish(args.directory.resolve(), args.provenance.resolve())
        print('Published native source and verified signed provenance; no release attestation was invented.')


if __name__ == '__main__':
    try:
        main()
    except (ReleaseError, OSError, ValueError, KeyError) as error:
        sys.exit('Native release refused: ' + str(error))
