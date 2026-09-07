#!/usr/bin/env python3
"""Prepare and independently verify committed native source bundles; never publish or attest."""
import argparse
import datetime
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tarfile
import tempfile


class ReleaseError(ValueError):
    """A source identity, archive, or eligibility check failed."""


def run(root, *command):
    try:
        return subprocess.check_output(command, cwd=root, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as error:
        raise ReleaseError(error.stderr.decode('utf-8', errors='replace').strip()) from error


def digest(data, algorithm='sha256'):
    return hashlib.new(algorithm, data).hexdigest()


def encode(value):
    return (json.dumps(value, indent=2, ensure_ascii=False) + '\n').encode()


def clean_source(root):
    commit = run(root, 'git', 'rev-parse', 'HEAD').decode().strip()
    run(root, 'git', 'ls-files', '--error-unmatch', 'tools/release-source.py')
    if run(root, 'git', 'status', '--porcelain', '--untracked-files=no').strip():
        raise ReleaseError('Commit tracked source changes before preparing or verifying a bundle.')
    return commit


def kind(root):
    return 'binding' if (root / 'resources/engine-lock.json').is_file() else 'engine'


def archive_name(root):
    return 'kumwe-engine-php-source.tar.gz' if kind(root) == 'binding' else 'kumwe-engine-source.tar.gz'


def archive_bytes(root):
    if kind(root) == 'engine':
        # Reuse the established Engine archive recipe and its export-ignore policy.
        run(root, 'bash', 'tools/source-archive.sh')
        return (root / 'artifacts/kumwe-engine-source.tar.gz').read_bytes()
    tar = run(root, 'git', 'archive', '--format=tar', '--prefix=kumwe-engine-php/', 'HEAD')
    # Match the explicit, reproducible gzip -n recipe used by the standalone Engine.
    return subprocess.run(['gzip', '-n'], input=tar, stdout=subprocess.PIPE, check=True).stdout


def archive_files(data, package_kind):
    """Read without extraction: refuse duplicate paths, traversal, links and build residue."""
    prefix = 'kumwe-engine-php/' if package_kind == 'binding' else 'kumwe-engine/'
    files = {}
    names = set()
    forbidden = {'.git', 'node_modules', 'artifacts', 'build', 'engine-build', 'modules', '.libs', 'autom4te.cache'}
    try:
        with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
            for member in archive.getmembers():
                name = member.name.rstrip('/')
                parts = PurePosixPath(name).parts
                if (not name or name.startswith('/') or '..' in parts or '\\' in name
                        or name != PurePosixPath(name).as_posix()):
                    raise ReleaseError('Unsafe archive path: ' + name)
                if name in names:
                    raise ReleaseError('Duplicate archive path: ' + name)
                names.add(name)
                if name == prefix.rstrip('/') and member.isdir():
                    continue
                if not name.startswith(prefix):
                    raise ReleaseError('Archive does not have its declared package root: ' + name)
                relative = name[len(prefix):]
                if any(part in forbidden for part in PurePosixPath(relative).parts):
                    raise ReleaseError('Build/cache content is not source distribution material: ' + relative)
                if PurePosixPath(relative).suffix.lower() in {'.pem', '.key'}:
                    raise ReleaseError('Credential-like file is not source distribution material: ' + relative)
                if member.isdir():
                    continue
                if not member.isfile():
                    raise ReleaseError('Source archives may contain only regular files and directories: ' + relative)
                if package_kind == 'engine' and PurePosixPath(relative).suffix.lower() in {'.php', '.phar'}:
                    raise ReleaseError('PHP oracle/runtime content must not ship in Engine: ' + relative)
                handle = archive.extractfile(member)
                if handle is None:
                    raise ReleaseError('Unreadable archive member: ' + relative)
                files[relative] = handle.read()
    except (tarfile.TarError, OSError, EOFError) as error:
        raise ReleaseError('Invalid compressed source archive.') from error
    if not files:
        raise ReleaseError('An empty archive is not a source package.')
    return dict(sorted(files.items()))


def read_json(files, path):
    try:
        document = json.loads(files[path])
    except (KeyError, ValueError, UnicodeError) as error:
        raise ReleaseError('Missing or invalid source JSON: ' + path) from error
    if not isinstance(document, dict):
        raise ReleaseError('Source JSON must be an object: ' + path)
    return document


def exact_hash(files, path, expected):
    if not isinstance(expected, str) or re.fullmatch('[a-f0-9]{64}', expected) is None:
        raise ReleaseError('Malformed source digest: ' + path)
    if path not in files or digest(files[path]) != expected:
        raise ReleaseError('Source material digest mismatch: ' + path)


def engine_materials(files, prefix=''):
    caps = read_json(files, prefix + 'resources/capabilities.json')
    abi = read_json(files, prefix + 'resources/abi-manifest.json')
    contracts = read_json(files, prefix + 'resources/contracts.json')
    for path, expected in abi['files'].items():
        exact_hash(files, prefix + path, expected)
    for corpus in caps['corpora']:
        exact_hash(files, prefix + corpus['path'], corpus['sha256'])
    for module in contracts['modules']:
        for key in ('corpus', 'preparation_corpus'):
            if key in module:
                exact_hash(files, prefix + module[key], module[key + '_sha256'])
        release = module.get('semantic_release')
        if not isinstance(release, dict) or re.fullmatch('[a-f0-9]{40}', release.get('commit', '')) is None:
            raise ReleaseError('Semantic owner material requires an exact source commit.')
        if module.get('corpus_sha256') != release.get('corpus_sha256'):
            raise ReleaseError('Semantic owner corpus disagrees with embedded corpus identity.')
    blockers = []
    if re.fullmatch(r'[1-9][0-9]*\.[0-9]+\.[0-9]+', caps.get('version', '')) is None:
        blockers.append('Engine version is a development candidate, not an App-eligible stable release.')
    if (contracts.get('abi_frozen') is not True or abi.get('status') not in ('stable', 'frozen')
            or caps.get('abi_status') not in ('stable', 'frozen')):
        blockers.append('The Engine ABI remains unfrozen.')
    if re.search('draft|candidate|development', contracts.get('state', '')):
        blockers.append('The semantic contract matrix still declares candidate inputs.')
    if caps.get('semantic_release_verified') is not True or any(
        module.get('release_verified') is not True for module in contracts['modules']
    ):
        blockers.append('Independent semantic-owner release verification is incomplete.')
    return caps, abi, contracts, blockers


def source_facts(files, package_kind):
    manifest_paths = ['LICENSE', 'CHARTER.md', 'MIGRATION-HANDOFF.md']
    if package_kind == 'engine':
        caps, abi, contracts, blockers = engine_materials(files)
        manifest_paths += [
            'resources/capabilities.json', 'resources/abi-manifest.json', 'resources/contracts.json',
            'resources/pcre2-source.json', 'resources/unicode-source.json',
        ]
        identity = {'version': caps['version'], 'abi_major': abi['abi_major'], 'abi_minor': abi['abi_minor']}
        dependencies = [module['semantic_release'] for module in contracts['modules']]
    else:
        compatibility = read_json(files, 'resources/compatibility/v1.json')
        lock = read_json(files, 'resources/engine-lock.json')
        if re.fullmatch('[a-f0-9]{40}', lock.get('commit', '')) is None:
            raise ReleaseError('Embedded Engine requires an exact source commit.')
        if re.fullmatch('[a-f0-9]{64}', lock.get('archive_sha256', '')) is None:
            raise ReleaseError('Embedded Engine requires an exact source archive digest.')
        actual = {path[len('vendor/engine/'):]: digest(data) for path, data in files.items()
                  if path.startswith('vendor/engine/')}
        if not actual or actual != lock.get('files'):
            raise ReleaseError('Embedded Engine source closure differs from the exact lock.')
        _, _, _, blockers = engine_materials(files, 'vendor/engine/')
        if compatibility.get('state') == 'candidate' or lock.get('state') == 'candidate':
            blockers.append('The extension or its embedded Engine is a candidate.')
        if compatibility.get('publication_allowed') is not True:
            blockers.append('Extension compatibility metadata does not permit publication.')
        if lock.get('release_verified') is not True or not lock.get('release'):
            blockers.append('The embedded immutable Engine release has not been independently verified.')
        if (re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', compatibility.get('version', '')) is None
                or compatibility.get('version') == '0.0.0'):
            blockers.append('The extension version is not stable.')
        manifest_paths += ['composer.json', 'resources/api/v1.json', 'resources/compatibility/v1.json',
                           'resources/engine-lock.json', 'php_kumwe_engine_build.h']
        identity = {'version': compatibility['version'], 'abi_major': compatibility['abi_major'],
                    'abi_minor': compatibility['abi_minor'], 'engine_commit': lock['commit'],
                    'engine_archive_sha256': lock['archive_sha256']}
        dependencies = [{key: value for key, value in lock.items() if key != 'files'}]
    materials = []
    for path in manifest_paths:
        if path not in files:
            raise ReleaseError('Required source distribution material is absent: ' + path)
        materials.append({'path': path, 'sha256': digest(files[path])})
    return {'identity': identity, 'materials': materials, 'dependencies': dependencies,
            'stable_source_blockers': blockers}


def binding_sbom(files, commit, timestamp):
    entries = []
    for index, (path, data) in enumerate(files.items()):
        entries.append({'SPDXID': 'SPDXRef-File-' + str(index), 'fileName': './' + path,
                        'checksums': [{'algorithm': 'SHA1', 'checksumValue': digest(data, 'sha1')},
                                      {'algorithm': 'SHA256', 'checksumValue': digest(data)}],
                        'licenseConcluded': 'NOASSERTION', 'licenseInfoInFiles': ['NOASSERTION'],
                        'copyrightText': 'NOASSERTION'})
    lock = read_json(files, 'resources/engine-lock.json')
    verification = digest(''.join(sorted(item['checksums'][0]['checksumValue'] for item in entries)).encode(), 'sha1')
    package_id = 'SPDXRef-Binding'
    return encode({
        'spdxVersion': 'SPDX-2.3', 'dataLicense': 'CC0-1.0', 'SPDXID': 'SPDXRef-DOCUMENT',
        'name': 'Kumwe PHP binding committed source inventory',
        'documentNamespace': 'https://github.com/kumwe/kumwe-engine/sbom/' + commit,
        'creationInfo': {'created': timestamp, 'creators': ['Tool: kumwe-native-release-source']},
        'packages': [
            {'SPDXID': package_id, 'name': 'kumwe/kumwe-engine', 'versionInfo': commit,
             'downloadLocation': 'git+https://github.com/kumwe/kumwe-engine.git@' + commit,
             'filesAnalyzed': True, 'packageVerificationCode': {'packageVerificationCodeValue': verification},
             'licenseConcluded': 'NOASSERTION', 'licenseDeclared': 'Apache-2.0',
             'copyrightText': 'NOASSERTION'},
            {'SPDXID': 'SPDXRef-EmbeddedEngine', 'name': 'kumwe/engine', 'versionInfo': lock['commit'],
             'downloadLocation': 'git+' + lock['repository'] + '.git@' + lock['commit'],
             'filesAnalyzed': False, 'licenseConcluded': 'NOASSERTION',
             'licenseDeclared': 'Apache-2.0 AND BSD-3-Clause AND BSD-2-Clause AND PHP-3.01 AND Unicode-3.0',
             'copyrightText': 'NOASSERTION'},
        ],
        'files': entries,
        'relationships': [
            {'spdxElementId': 'SPDXRef-DOCUMENT', 'relatedSpdxElement': package_id, 'relationshipType': 'DESCRIBES'},
            {'spdxElementId': package_id, 'relatedSpdxElement': 'SPDXRef-EmbeddedEngine', 'relationshipType': 'DEPENDS_ON'},
        ] + [{'spdxElementId': package_id, 'relatedSpdxElement': entry['SPDXID'], 'relationshipType': 'CONTAINS'}
             for entry in entries],
    })


def source_record(root, commit, files, archive, sbom, tag):
    tree = run(root, 'git', 'rev-parse', 'HEAD^{tree}').decode().strip()
    timestamp = datetime.datetime.fromtimestamp(int(run(root, 'git', 'show', '-s', '--format=%ct', commit)),
                                                datetime.timezone.utc).isoformat().replace('+00:00', 'Z')
    facts = source_facts(files, kind(root))
    if tag and (not re.fullmatch(r'v?[0-9]+\.[0-9]+\.[0-9]+', tag)
                or run(root, 'git', 'rev-parse', 'refs/tags/' + tag + '^{commit}').decode().strip() != commit):
        raise ReleaseError('The supplied source tag does not identify the exact selected commit.')
    if tag and tag.removeprefix('v') != facts['identity']['version']:
        raise ReleaseError('The supplied source tag version differs from the declared source version.')
    package = 'kumwe/kumwe-engine' if kind(root) == 'binding' else 'kumwe/engine'
    return {
        'schema': 'kumwe-native-source-bundle/v1', 'package': package,
        'source': {'repository': 'https://github.com/' + package, 'commit': commit, 'tree': tree,
                   'committed_at': timestamp, 'tag': tag},
        'archive': {'name': archive_name(root), 'sha256': digest(archive), 'bytes': len(archive)},
        'sbom': {'name': 'source.spdx.json', 'sha256': digest(sbom)},
        **facts,
        'release_attestation': False, 'publication_performed': False,
    }


def provenance(record):
    # An unsigned source assembly statement, deliberately not a signed build/release attestation.
    return encode({
        '_type': 'https://in-toto.io/Statement/v1',
        'subject': [{'name': record['archive']['name'], 'digest': {'sha256': record['archive']['sha256']}},
                    {'name': record['sbom']['name'], 'digest': {'sha256': record['sbom']['sha256']}}],
        'predicateType': 'https://kumwe.dev/provenance/native-source-assembly/v1',
        'predicate': {'source': record['source'], 'materials': record['materials'],
                      'dependencies': record['dependencies'],
                      'recipe': 'committed git archive; gzip -n; committed-source SPDX inventory',
                      'signed': False, 'release_attestation': False, 'compiled_artifact': False},
    })


def bundle(root, commit, tag=None):
    first = archive_bytes(root)
    second = archive_bytes(root)
    if first != second:
        raise ReleaseError('Repeated committed source archives are not byte-reproducible.')
    files = archive_files(first, kind(root))
    timestamp = datetime.datetime.fromtimestamp(int(run(root, 'git', 'show', '-s', '--format=%ct', commit)),
                                                datetime.timezone.utc).isoformat().replace('+00:00', 'Z')
    sbom = run(root, 'node', 'tools/source-sbom.mjs') if kind(root) == 'engine' else binding_sbom(files, commit, timestamp)
    inventory = json.loads(sbom)
    listed = {entry['fileName'].removeprefix('./'): next(check['checksumValue'] for check in entry['checksums']
              if check['algorithm'] == 'SHA256') for entry in inventory['files']}
    if listed != {path: digest(data) for path, data in files.items()}:
        raise ReleaseError('SPDX inventory does not describe exactly the exported source archive.')
    record = source_record(root, commit, files, first, sbom, tag)
    result = {archive_name(root): first, 'source.spdx.json': sbom,
              'source.json': encode(record), 'source.provenance.json': provenance(record)}
    result['SHA256SUMS'] = ''.join(digest(data) + '  ' + path + '\n' for path, data in sorted(result.items())).encode()
    if clean_source(root) != commit:
        raise ReleaseError('Source changed while preparing its bundle; repeat against one committed head.')
    return result, record


def require_stable(record):
    if record['stable_source_blockers']:
        raise ReleaseError('Stable source refused: ' + ' '.join(record['stable_source_blockers']))


def outside_source(root, path):
    resolved = path.resolve()
    if resolved == root or root in resolved.parents:
        raise ReleaseError('Source evidence must be stored outside the tested repository.')
    return resolved


def verify_checksums(directory, source_name):
    names = {source_name, 'source.json', 'source.spdx.json', 'source.provenance.json'}
    if {path.name for path in directory.iterdir()} != names | {'SHA256SUMS'}:
        raise ReleaseError('Source bundle has missing or unrecorded files.')
    checksum_file = directory / 'SHA256SUMS'
    if checksum_file.is_symlink() or not checksum_file.is_file() or checksum_file.stat().st_size > 2048:
        raise ReleaseError('Invalid checksum inventory.')
    lines = checksum_file.read_text().splitlines()
    recorded = {}
    for line in lines:
        match = re.fullmatch(r'([a-f0-9]{64})  ([a-zA-Z0-9._-]+)', line)
        if match is None or match[2] in recorded:
            raise ReleaseError('Invalid or duplicate checksum entry.')
        recorded[match[2]] = match[1]
    if set(recorded) != names:
        raise ReleaseError('Checksum inventory does not name exactly the source bundle.')
    for name, expected in recorded.items():
        path = directory / name
        if path.is_symlink() or not path.is_file():
            raise ReleaseError('Evidence must be a regular file: ' + name)
        hasher = hashlib.sha256()
        with path.open('rb') as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b''):
                hasher.update(chunk)
        if hasher.hexdigest() != expected:
            raise ReleaseError('Evidence checksum mismatch: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['prepare', 'verify'])
    parser.add_argument('directory', type=Path, help='external source bundle directory')
    parser.add_argument('--expected-commit', help='full independently supplied commit required by verify')
    parser.add_argument('--expected-sha256', help='optional independently supplied source archive digest')
    parser.add_argument('--tag', help='existing exact source tag; this tool never creates tags')
    parser.add_argument('--require-stable', action='store_true', help='also reject declared candidate/unverified source')
    arguments = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    destination = outside_source(root, arguments.directory)
    commit = clean_source(root)
    if arguments.action == 'verify' and not arguments.expected_commit:
        raise ReleaseError('Verification requires an independently supplied --expected-commit.')
    if arguments.expected_commit and arguments.expected_commit != commit:
        raise ReleaseError('Expected commit differs from the clean checked-out source.')
    tag = arguments.tag
    if arguments.action == 'verify':
        if not destination.is_dir() or destination.is_symlink():
            raise ReleaseError('Verification requires a regular source bundle directory.')
        verify_checksums(destination, archive_name(root))
        try:
            source_file = destination / 'source.json'
            if source_file.is_symlink() or source_file.stat().st_size > 4 * 1024 * 1024:
                raise ReleaseError('Invalid or oversized bundle source record.')
            supplied = json.loads(source_file.read_bytes())
        except (OSError, ValueError) as error:
            raise ReleaseError('Missing or invalid bundle source record.') from error
        if tag is None:
            tag = supplied['source']['tag']
    if arguments.require_stable:
        facts = source_facts(archive_files(archive_bytes(root), kind(root)), kind(root))
        require_stable(facts)
    prepared, record = bundle(root, commit, tag)
    if arguments.expected_sha256 and arguments.expected_sha256 != record['archive']['sha256']:
        raise ReleaseError('Expected archive digest differs from the exact committed source.')
    if arguments.require_stable:
        require_stable(record)
    if arguments.action == 'prepare':
        if destination.exists():
            raise ReleaseError('Refusing to overwrite an existing evidence directory.')
        destination.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.kumwe-source-', dir=destination.parent) as temporary:
            for name, data in prepared.items():
                (Path(temporary) / name).write_bytes(data)
            os.replace(temporary, destination)
    else:
        if {path.name for path in destination.iterdir()} != set(prepared):
            raise ReleaseError('Source bundle has missing or unrecorded files.')
        for name, expected in prepared.items():
            path = destination / name
            if (path.is_symlink() or not path.is_file() or path.stat().st_size != len(expected)
                    or path.read_bytes() != expected):
                raise ReleaseError('Source bundle differs from reproducible committed input: ' + name)
    print(('Prepared' if arguments.action == 'prepare' else 'Verified') + ' exact ' + record['package'] + ' source bundle for ' + commit)
    print('Unsigned source evidence only; independent release attestation and publication remain separate.')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, TypeError, OSError, subprocess.CalledProcessError) as error:
        print('Source release check failed: ' + str(error), file=sys.stderr)
        raise SystemExit(1) from error
