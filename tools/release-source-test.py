#!/usr/bin/env python3
"""Exercise the real committed-source recipe and hostile evidence without a native rebuild."""
import gzip
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest

TOOL = Path(__file__).with_name('release-source.py')
SPEC = importlib.util.spec_from_file_location('release_source', TOOL)
RELEASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RELEASE)


class SourceReleaseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory(prefix='kumwe-source-release-tests-')
        cls.base = Path(cls.work.name)
        cls.repo = cls.base / 'source'
        cls.repo.mkdir()
        source = TOOL.parent.parent
        # Use precisely committed source; the fixture then commits the tool under test.
        exported = subprocess.check_output(['git', 'archive', '--format=tar', 'HEAD'], cwd=source)
        with tarfile.open(fileobj=io.BytesIO(exported)) as archive:
            archive.extractall(cls.repo, filter='data')
        # Some source exports omit .gitattributes. Restore the actual committed export policy.
        attributes = subprocess.run(['git', 'show', 'HEAD:.gitattributes'], cwd=source,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if attributes.returncode == 0:
            (cls.repo / '.gitattributes').write_bytes(attributes.stdout)
        # Export-ignored inputs need not be regenerated: they were not source distribution material.
        (cls.repo / 'tools/release-source.py').write_bytes(TOOL.read_bytes())
        for command in [
            ['git', 'init', '-q'], ['git', 'config', 'user.email', 'source-fixture@example.invalid'],
            ['git', 'config', 'user.name', 'Source release fixture'], ['git', 'add', '.'],
            ['git', 'commit', '-qm', 'Committed native source fixture'],
        ]:
            subprocess.run(command, cwd=cls.repo, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        cls.commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=cls.repo).decode().strip()
        cls.output = cls.base / 'bundle'
        cls.invoke('prepare', cls.output)
        cls.record = json.loads((cls.output / 'source.json').read_text())

    @classmethod
    def tearDownClass(cls):
        cls.work.cleanup()

    @classmethod
    def invoke(cls, *arguments, success=True):
        process = subprocess.run([sys.executable, str(cls.repo / 'tools/release-source.py'), *map(str, arguments)],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if success and process.returncode != 0:
            raise AssertionError(process.stderr)
        if not success and process.returncode == 0:
            raise AssertionError('Invalid source/evidence unexpectedly accepted: ' + process.stdout)
        return process

    def test_reproducible_complete_bundle_and_verification(self):
        other = self.base / 'second-bundle'
        self.invoke('prepare', other)
        self.assertEqual({p.name: p.read_bytes() for p in self.output.iterdir()},
                         {p.name: p.read_bytes() for p in other.iterdir()})
        self.invoke('verify', self.output, '--expected-commit', self.commit,
                    '--expected-sha256', self.record['archive']['sha256'])
        inventory = json.loads((self.output / 'source.spdx.json').read_bytes())
        self.assertTrue(inventory['files'])
        statement = json.loads((self.output / 'source.provenance.json').read_bytes())
        self.assertFalse(statement['predicate']['signed'])
        self.assertFalse(statement['predicate']['release_attestation'])
        self.assertFalse(self.record['publication_performed'])

    def test_candidate_is_not_a_stable_release(self):
        self.assertTrue(self.record['stable_source_blockers'])
        failed = self.base / 'refused-stable'
        process = self.invoke('prepare', failed, '--require-stable', success=False)
        self.assertIn('Stable source refused', process.stderr)
        self.assertFalse(failed.exists())
        self.invoke('verify', self.output, '--expected-commit', self.commit, '--require-stable', success=False)

    def test_dirty_source_and_in_tree_evidence_are_refused(self):
        license_path = self.repo / 'LICENSE'
        original = license_path.read_bytes()
        try:
            license_path.write_bytes(original + b'changed\n')
            process = self.invoke('prepare', self.base / 'dirty-output', success=False)
            self.assertIn('Commit tracked source changes', process.stderr)
        finally:
            license_path.write_bytes(original)
        self.invoke('prepare', self.repo / 'evidence', success=False)

    def test_independent_source_coordinates_are_required(self):
        self.invoke('verify', self.output, success=False)
        self.invoke('verify', self.output, '--expected-commit', '0' * 40, success=False)
        self.invoke('verify', self.output, '--expected-commit', self.commit,
                    '--expected-sha256', '0' * 64, success=False)
        self.invoke('prepare', self.base / 'floating-tag', '--tag', 'main', success=False)
        self.invoke('prepare', self.output, success=False)

    def test_same_commit_tag_must_match_declared_source_version(self):
        package_kind = RELEASE.kind(self.repo)
        archive = (self.output / self.record['archive']['name']).read_bytes()
        files = RELEASE.archive_files(archive, package_kind)
        path = 'resources/compatibility/v1.json' if package_kind == 'binding' else 'resources/capabilities.json'
        declaration = json.loads(files[path])
        declaration['version'] = '1.0.1'
        files[path] = RELEASE.encode(declaration)
        sbom = (self.output / 'source.spdx.json').read_bytes()
        tags = ('v1.0.0', 'v1.0.1', '1.0.1')
        try:
            for tag in tags:
                subprocess.run(['git', 'tag', tag, self.commit], cwd=self.repo, check=True)
            # Every tag names exactly the same fixture commit. Its spelling alone is insufficient.
            with self.assertRaisesRegex(RELEASE.ReleaseError, 'tag version differs'):
                RELEASE.source_record(self.repo, self.commit, files, archive, sbom, 'v1.0.0')
            for tag in tags[1:]:
                record = RELEASE.source_record(self.repo, self.commit, files, archive, sbom, tag)
                self.assertEqual(record['identity']['version'], '1.0.1')
                self.assertEqual(record['source']['tag'], tag)
        finally:
            subprocess.run(['git', 'tag', '-d', *tags], cwd=self.repo,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)

    def test_each_evidence_artifact_is_bound_to_committed_input(self):
        for name in ('source.json', 'source.provenance.json', 'source.spdx.json', 'SHA256SUMS',
                     self.record['archive']['name']):
            with self.subTest(artifact=name):
                path = self.output / name
                original = path.read_bytes()
                try:
                    # Valid-looking edits and appended bytes cannot be authorized by changing a checksum file.
                    path.write_bytes(original + b' ')
                    self.invoke('verify', self.output, '--expected-commit', self.commit, success=False)
                finally:
                    path.write_bytes(original)
        extra = self.output / 'unrecorded.txt'
        try:
            extra.write_text('not in source provenance')
            self.invoke('verify', self.output, '--expected-commit', self.commit, success=False)
        finally:
            extra.unlink()

    def test_rehashed_false_attestation_claim_is_still_refused(self):
        record_path = self.output / 'source.json'
        sums_path = self.output / 'SHA256SUMS'
        original_record, original_sums = record_path.read_bytes(), sums_path.read_bytes()
        try:
            record = json.loads(original_record)
            record['release_attestation'] = True
            changed = RELEASE.encode(record)
            record_path.write_bytes(changed)
            lines = original_sums.decode().splitlines()
            lines = [hashlib.sha256(changed).hexdigest() + '  source.json'
                     if line.endswith('  source.json') else line for line in lines]
            sums_path.write_text('\n'.join(lines) + '\n')
            process = self.invoke('verify', self.output, '--expected-commit', self.commit, success=False)
            self.assertIn('differs from reproducible committed input: source.json', process.stderr)
        finally:
            record_path.write_bytes(original_record)
            sums_path.write_bytes(original_sums)

    def test_corpus_and_embedded_source_mismatches_are_refused(self):
        package_kind = RELEASE.kind(self.repo)
        files = RELEASE.archive_files((self.output / self.record['archive']['name']).read_bytes(), package_kind)
        prefix = 'vendor/engine/' if package_kind == 'binding' else ''
        path = prefix + 'resources/abi-manifest.json'
        damaged = dict(files)
        record = json.loads(damaged[path])
        record['files'][next(iter(record['files']))] = '0' * 64
        damaged[path] = json.dumps(record).encode()
        with self.assertRaises(RELEASE.ReleaseError):
            RELEASE.source_facts(damaged, package_kind)

    def test_unsafe_archive_paths_links_and_duplicate_members_are_refused(self):
        root = 'kumwe-engine/'
        for name, member_type in [('kumwe-engine/../escape', tarfile.REGTYPE),
                                  ('/absolute', tarfile.REGTYPE),
                                  (root + 'link', tarfile.SYMTYPE),
                                  (root + 'build/cache', tarfile.REGTYPE),
                                  (root + 'secret.key', tarfile.REGTYPE),
                                  (root + 'oracle.php', tarfile.REGTYPE)]:
            with self.subTest(path=name):
                stream = io.BytesIO()
                with tarfile.open(fileobj=stream, mode='w') as archive:
                    member = tarfile.TarInfo(name)
                    member.type = member_type
                    member.linkname = '/outside' if member_type == tarfile.SYMTYPE else ''
                    member.size = 1 if member_type == tarfile.REGTYPE else 0
                    archive.addfile(member, io.BytesIO(b'x') if member.size else None)
                with self.assertRaises(RELEASE.ReleaseError):
                    RELEASE.archive_files(gzip.compress(stream.getvalue(), mtime=0), 'engine')
        stream = io.BytesIO()
        with tarfile.open(fileobj=stream, mode='w') as archive:
            for _ in range(2):
                member = tarfile.TarInfo(root + 'duplicate')
                member.size = 1
                archive.addfile(member, io.BytesIO(b'x'))
        with self.assertRaises(RELEASE.ReleaseError):
            RELEASE.archive_files(gzip.compress(stream.getvalue(), mtime=0), 'engine')


if __name__ == '__main__':
    unittest.main()
