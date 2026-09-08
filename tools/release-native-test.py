#!/usr/bin/env python3
"""Offline publication regressions: immutable identities, actual CI, signed assets and safe retries."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('native_publisher', Path(__file__).with_name('release-native.py'))
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
Publisher, ReleaseError = MODULE.Publisher, MODULE.ReleaseError
SHA, OTHER = 'a' * 40, 'b' * 40
ENV = {'GITHUB_REPOSITORY': 'kumwe/engine', 'GITHUB_SHA': SHA, 'GITHUB_REF': 'refs/heads/main',
       'DEFAULT_BRANCH': 'main', 'QUALITY_RUN_ID': '123'}
ENGINE_JOBS = ['native (ubuntu-24.04, gcc, g++)', 'native (ubuntu-24.04, clang, clang++)',
               'native (macos-14, clang, clang++)', 'sanitizers-fuzz', 'thread-sanitizer', 'archive-and-faults']
BINDING_JOBS = ['source-release-preparation', 'binding', 'address-undefined-sanitizers', 'clean-pie',
                'whole-boundary-benchmarks']


class Fixture(Publisher):
    """Fake only the process/API/source-tool boundaries; run the real release orchestration."""
    def __init__(self, root, environment=None):
        super().__init__(root, environment or ENV)
        self.calls, self.assets = [], {}
        self.tag, self.release = None, None
        self.head = self.branch_head = self.sha
        self.signature_valid, self.stable, self.ancestor = True, True, True
        self.fail_upload, self.extra_before_finalize, self.advance_before_finalize = None, False, False
        self.context_calls = 0
        self.quality = {'head_sha': self.sha, 'head_branch': self.branch,
                        'head_repository': {'full_name': self.repo}, 'event': 'push',
                        'path': '.github/workflows/ci.yml',
                        'name': 'Native quality' if self.repo == 'kumwe/engine' else 'Native binding candidate',
                        'status': 'completed', 'conclusion': 'success'}
        names = ENGINE_JOBS if self.repo == 'kumwe/engine' else BINDING_JOBS
        self.jobs = [{'id': i + 1, 'name': name, 'status': 'completed', 'conclusion': 'success'}
                     for i, name in enumerate(names)]

    def command(self, *args, cwd=None, data=None, check=True):
        self.calls.append(('command', args))
        output = b''
        if args[:3] == ('git', 'rev-parse', 'HEAD'):
            output = self.head.encode()
        if args[:3] == ('git', 'merge-base', '--is-ancestor') and not self.ancestor:
            raise ReleaseError('Unrelated published commit')
        if args[:3] == ('git', 'clone', '--quiet'):
            Path(args[-1]).mkdir()
        if args[:3] == ('gh', 'attestation', 'verify'):
            signature = Path(args[args.index('--bundle') + 1])
            digest = args[args.index('--source-digest') + 1]
            if not self.signature_valid or signature.read_bytes() != ('signed:' + digest).encode():
                raise ReleaseError('Invalid signature/source identity')
        if args[:3] == ('gh', 'release', 'upload'):
            path = Path(args[4])
            if path.name == self.fail_upload:
                raise ReleaseError('Interrupted asset transfer')
            if path.name in self.assets:
                raise ReleaseError('Immutable upload collision')
            self.assets[path.name] = path.read_bytes()
            if self.extra_before_finalize and path.name == self.signature:
                self.assets['unexpected.bin'] = b'unreviewed'
            if self.advance_before_finalize and path.name == self.signature:
                self.branch_head = OTHER
        return subprocess.CompletedProcess(args, 0, output, b'')

    def api(self, endpoint, *, missing=False, method='GET', payload=None):
        self.calls.append(('api', method, endpoint, copy.deepcopy(payload)))
        if endpoint.startswith('branches/'):
            self.context_calls += 1
            return {'name': self.branch, 'commit': {'sha': self.branch_head}}
        if endpoint == 'actions/runs/' + self.run_id:
            return copy.deepcopy(self.quality)
        if endpoint.startswith('actions/runs/' + self.run_id + '/jobs?'):
            return {'total_count': len(self.jobs), 'jobs': copy.deepcopy(self.jobs)}
        if endpoint.startswith('git/ref/tags/'):
            return None if self.tag is None else {'object': {'type': 'commit', 'sha': self.tag}}
        if endpoint == 'git/refs' and method == 'POST':
            if self.tag is not None:
                raise ReleaseError('Tag creation collision')
            self.tag = payload['sha']
            return {'ref': payload['ref'], 'object': {'type': 'commit', 'sha': self.tag}}
        if endpoint == 'releases' and method == 'POST':
            self.release = dict(payload, id=42)
        elif endpoint == 'releases/42' and method == 'PATCH':
            self.release.update(payload)
            self.release['published_at'] = '2026-09-08T00:00:00Z'
        elif not (endpoint.startswith('releases/tags/') or endpoint == 'releases/42'):
            raise AssertionError('Unexpected API request: ' + endpoint)
        if self.release is None:
            if missing:
                return None
            raise ReleaseError('Missing release')
        return dict(copy.deepcopy(self.release), assets=[{'name': name} for name in self.assets])

    def source_bundle(self, action, destination, *, root=None, sha=None):
        if not self.stable:
            raise ReleaseError('Stable source prerequisite refused')
        if action == 'prepare':
            destination.mkdir(parents=True)
            for name in self.files:
                (destination / name).write_bytes(('source:' + name).encode())
            record = {'identity': {'version': '1.0.0'}, 'source': {'commit': sha or self.sha}}
            (destination / 'source.json').write_text(json.dumps(record))
        record = json.loads((destination / 'source.json').read_text())
        if record['source']['commit'] != (sha or self.sha):
            raise ReleaseError('Source helper rejected commit mismatch')
        return record

    def download(self, tag, name, destination):
        self.calls.append(('download', tag, name))
        destination.mkdir(parents=True, exist_ok=True)
        path = destination / name
        path.write_bytes(self.assets[name])
        return path

    def mutations(self):
        return [call for call in self.calls if (call[0] == 'api' and call[1] != 'GET')
                or (call[0] == 'command' and call[1][:3] == ('gh', 'release', 'upload'))]


class NativePublicationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.publisher = Fixture(self.root)
        self.bundle = self.root / 'bundle'
        self.signature = self.root / 'signed.json'
        self.signature.write_text('signed:' + SHA)

    def tearDown(self):
        self.temporary.cleanup()

    def prepare(self):
        return self.publisher.prepare(self.bundle)

    def publish(self):
        self.publisher.publish(self.bundle, self.signature)

    def test_actual_http_status_required(self):
        self.assertIsNone(MODULE.decode_http(b'HTTP/2.0 404 Not Found\r\n\r\n{"message":"missing"}', 1, True))
        for raw, code in [(b'HTTP/2.0 403 Forbidden\n\n{"message":"404"}', 1),
                          (b'HTTP/2.0 500 Error\n\n{}', 1), (b'404 Not Found', 1),
                          (b'HTTP/2.0 404 Not Found\n\n{}', 0), (b'HTTP/2.0 200 OK\n\n[]', 0),
                          (b'HTTP/2.0 200 OK\n\nnot-json', 0)]:
            with self.subTest(raw=raw), self.assertRaises(ReleaseError):
                MODULE.decode_http(raw, code, True)
        self.assertEqual({}, MODULE.decode_http(b'HTTP/2.0 201 Created\n\n{}', 0))

    def test_context_refuses_untrusted_or_inexact_inputs(self):
        for key, value in [('GITHUB_REPOSITORY', 'fork/engine'), ('GITHUB_SHA', 'main'),
                           ('QUALITY_RUN_ID', '0'), ('GITHUB_REF', 'refs/heads/feature'),
                           ('DEFAULT_BRANCH', '')]:
            with self.subTest(key=key), self.assertRaises(ReleaseError):
                Fixture(self.root, dict(ENV, **{key: value}))

    def test_dynamic_default_branch_is_supported(self):
        publisher = Fixture(self.root, dict(ENV, DEFAULT_BRANCH='stable/source', GITHUB_REF='refs/heads/stable/source'))
        publisher.check_context()
        self.assertTrue(any(call[0:3] == ('api', 'GET', 'branches/stable%2Fsource') for call in publisher.calls))

    def test_both_full_native_quality_inventories_pass(self):
        self.publisher.check_context()
        Fixture(self.root, dict(ENV, GITHUB_REPOSITORY='kumwe/kumwe-engine')).check_context()

    def test_wrong_quality_evidence_never_mutates(self):
        for key, value in [('head_sha', OTHER), ('head_branch', 'feature'), ('event', 'pull_request'),
                           ('head_repository', {'full_name': 'fork/engine'}), ('conclusion', 'failure'),
                           ('status', 'in_progress'), ('path', '.github/workflows/other.yml'), ('name', 'Other')]:
            with self.subTest(key=key):
                publisher = Fixture(self.root)
                publisher.quality[key] = value
                with self.assertRaises(ReleaseError):
                    publisher.prepare(self.root / ('missing-' + key))
                self.assertEqual([], publisher.mutations())

    def test_checkout_and_current_default_branch_must_match(self):
        for attribute in ['head', 'branch_head']:
            with self.subTest(attribute=attribute):
                publisher = Fixture(self.root)
                setattr(publisher, attribute, OTHER)
                with self.assertRaises(ReleaseError):
                    publisher.check_context()

    def test_skipped_missing_or_substituted_matrix_lane_refused(self):
        for mode in ['skip', 'missing', 'duplicate', 'substitute']:
            with self.subTest(mode=mode):
                publisher = Fixture(self.root)
                if mode == 'skip':
                    publisher.jobs[0]['conclusion'] = 'skipped'
                elif mode == 'missing':
                    publisher.jobs.pop()
                elif mode == 'duplicate':
                    publisher.jobs.append(publisher.jobs[0])
                else:
                    publisher.jobs[2]['name'] = 'native (ubuntu-24.04, other, other)'
                with self.assertRaises(ReleaseError):
                    publisher.check_context()

    def test_stable_source_refusal_precedes_publication(self):
        self.publisher.stable = False
        with self.assertRaises(ReleaseError):
            self.prepare()
        self.assertEqual([], self.publisher.mutations())

    def test_prepare_has_no_mutations(self):
        self.assertEqual({'version': '1.0.0', 'tag': 'v1.0.0', 'already_published': 'false'}, self.prepare())
        self.assertEqual([], self.publisher.mutations())

    def test_new_release_verifies_every_signed_source_before_finalizing(self):
        self.prepare()
        self.publish()
        self.assertFalse(self.publisher.release['draft'])
        self.assertEqual(SHA, self.publisher.tag)
        self.assertEqual(set(self.publisher.files + [self.publisher.signature]), set(self.publisher.assets))
        first_write = next(i for i, call in enumerate(self.publisher.calls) if call in self.publisher.mutations())
        signed = [call for call in self.publisher.calls[:first_write] if call[0] == 'command'
                  and call[1][:3] == ('gh', 'attestation', 'verify')]
        self.assertEqual(len(self.publisher.files), len(signed))
        for call in signed:
            self.assertIn('--deny-self-hosted-runners', call[1])
            self.assertIn('--cert-identity', call[1])
            self.assertIn('--source-digest', call[1])
        self.assertEqual(3, self.publisher.context_calls)

    def test_invalid_signature_cannot_create_tag(self):
        self.prepare()
        self.publisher.signature_valid = False
        with self.assertRaises(ReleaseError):
            self.publish()
        self.assertEqual([], self.publisher.mutations())

    def test_existing_unpublished_tag_cannot_move(self):
        self.prepare()
        self.publisher.tag = OTHER
        with self.assertRaises(ReleaseError):
            self.publish()
        self.assertEqual([], self.publisher.mutations())
        self.assertEqual(OTHER, self.publisher.tag)

    def test_interrupted_draft_retries_missing_assets_without_replacement(self):
        self.prepare()
        self.publisher.fail_upload = 'source.json'
        with self.assertRaises(ReleaseError):
            self.publish()
        self.assertTrue(self.publisher.release['draft'])
        assets_before = copy.deepcopy(self.publisher.assets)
        self.publisher.fail_upload = None
        self.publisher.calls = []
        self.publish()
        uploads = [call[1][4] for call in self.publisher.mutations() if call[0] == 'command']
        for name, value in assets_before.items():
            self.assertEqual(value, self.publisher.assets[name])
            self.assertFalse(any(Path(path).name == name for path in uploads))
        self.assertFalse(self.publisher.release['draft'])

    def test_existing_draft_asset_mismatch_is_never_overwritten(self):
        self.prepare()
        self.publisher.tag = SHA
        self.publisher.release = {'id': 42, 'tag_name': 'v1.0.0', 'draft': True, 'prerelease': False}
        self.publisher.assets[self.publisher.archive] = b'changed'
        with self.assertRaises(ReleaseError):
            self.publish()
        self.assertEqual([], self.publisher.mutations())
        self.assertEqual(b'changed', self.publisher.assets[self.publisher.archive])

    def test_unexpected_asset_prevents_final_publication(self):
        self.prepare()
        self.publisher.extra_before_finalize = True
        with self.assertRaises(ReleaseError):
            self.publish()
        self.assertTrue(self.publisher.release['draft'])
        self.assertFalse(any(call[0:2] == ('api', 'PATCH') for call in self.publisher.calls))

    def test_default_branch_advance_prevents_final_publication(self):
        self.prepare()
        self.publisher.advance_before_finalize = True
        with self.assertRaises(ReleaseError):
            self.publish()
        self.assertTrue(self.publisher.release['draft'])

    def test_published_release_recheck_performs_no_mutations(self):
        self.prepare()
        self.publish()
        self.publisher.calls = []
        self.publish()
        self.assertEqual([], self.publisher.mutations())

    def test_published_signature_or_source_corruption_cannot_be_repaired_in_place(self):
        self.prepare()
        self.publish()
        self.publisher.assets[self.publisher.signature] = b'unsigned'
        self.publisher.calls = []
        with self.assertRaises(ReleaseError):
            self.publish()
        self.assertEqual([], self.publisher.mutations())

    def test_published_unrelated_tag_refused(self):
        self.prepare()
        self.publish()
        self.publisher.ancestor = False
        self.publisher.calls = []
        with self.assertRaises(ReleaseError):
            self.publish()
        self.assertEqual([], self.publisher.mutations())

    def test_published_missing_asset_refused_without_recreation(self):
        self.prepare()
        self.publish()
        del self.publisher.assets['SHA256SUMS']
        self.publisher.calls = []
        with self.assertRaises(ReleaseError):
            self.publish()
        self.assertEqual([], self.publisher.mutations())

    def test_draft_identity_state_cannot_change(self):
        for field, value in [('id', 0), ('id', True), ('id', 99), ('tag_name', 'v2.0.0'),
                             ('draft', False), ('prerelease', True)]:
            with self.subTest(field=field, value=value), self.assertRaises(ReleaseError):
                self.publisher.require_draft(dict(id=42, tag_name='v1.0.0', draft=True, prerelease=False,
                                                 **{}) | {field: value}, 'v1.0.0', 42)

    def test_binding_uses_its_own_archive_and_repo_identity(self):
        self.publisher = Fixture(self.root, dict(ENV, GITHUB_REPOSITORY='kumwe/kumwe-engine'))
        self.prepare()
        self.publish()
        self.assertIn('kumwe-engine-php-source.tar.gz', self.publisher.assets)
        self.assertNotIn('kumwe-engine-source.tar.gz', self.publisher.assets)


if __name__ == '__main__':
    unittest.main(verbosity=2)
