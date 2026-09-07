#!/usr/bin/env python3
"""Produce a compact, verified index of a complete actual-artifact acceptance run."""
import argparse
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path

spec = importlib.util.spec_from_file_location('benchmark_runner', Path(__file__).with_name('run.py'))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def read(path):
    return json.loads(path.read_text())


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1048576), b''):
            digest.update(chunk)
    return digest.hexdigest()


def artifact(path):
    return {'file': path.name, 'sha256': sha(path), 'bytes': path.stat().st_size}


def verify_rows(results):
    expected = set()
    for profile in runner.PROFILES:
        phases = ('encode', 'digest') if profile == 'canonical' else ('warm',) if profile == 'decimal' else ('warm', 'cold')
        for size in (1, 32, 256, 4096):
            for shape in ('valid', 'hostile'):
                for phase in phases:
                    for backend in ('php', 'native'):
                        expected.add((profile, size, shape, phase, backend))
    seen = set()
    pairs = {}
    for row in results['matrix']:
        key = (row['profile'], row['size'], row['shape'], row['phase'], row['backend'])
        if key in seen or key not in expected or not row['ok'] or len(row['durations_ns']) < 20:
            raise ValueError(f'Invalid, duplicate or under-sampled matrix row: {key}')
        if any(not isinstance(n, int) or n <= 0 for n in row['durations_ns']):
            raise ValueError(f'Invalid duration: {key}')
        seen.add(key)
        pairs.setdefault(key[:-1], {})[row['backend']] = row
    if seen != expected:
        raise ValueError(f'Missing matrix cases: {expected - seen}')
    for key, pair in pairs.items():
        if pair['php']['digest'] != pair['native']['digest'] or pair['php']['dataset_descriptor_sha256'] != pair['native']['dataset_descriptor_sha256']:
            raise ValueError(f'Unmatched semantics or dataset: {key}')
    return pairs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--results', type=Path, required=True)
    parser.add_argument('--artifact-dir', type=Path, required=True)
    parser.add_argument('--old-new-comparison', type=Path)
    parser.add_argument('--container-context', type=Path)
    parser.add_argument('--source-context', type=Path)
    parser.add_argument('--ci-url', action='append', default=[])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    results = read(args.results)
    if not results.get('complete') or not results.get('correctness_passed') or not results.get('statistical_sample_floor_met'):
        raise ValueError('Only complete, parity-verified >=20-sample results may enter the acceptance index')
    pairs = verify_rows(results)
    actual = results['metadata']['worker_identity']['native']['capabilities']
    expected_path = args.artifact_dir / 'candidate-compatibility.json'
    observed_path = args.artifact_dir / 'candidate-tuple.json'
    build_path = args.artifact_dir / 'build-identity.json'
    cli_path = args.artifact_dir / 'engine-cli-capabilities.json'
    module_path = args.artifact_dir / 'modules/kumwe_engine.so'
    expected, observed, build, cli = (read(p) for p in (expected_path, observed_path, build_path, cli_path))
    if sha(module_path) != results['metadata']['extension_sha256'] or observed['tuple'] != actual:
        raise ValueError('Actual module or observed tuple differs from measured artifact')
    if expected['capabilities'] != actual['computation'] or build != actual['binding_build']:
        raise ValueError('Independent semantic/build tuple differs from measured artifact')
    for key in ('extension_version', 'embedded_engine_commit', 'embedded_source_sha256', 'binding_build_digest', 'binding_build'):
        if expected[key] != actual[key]:
            raise ValueError(f'Independent tuple mismatch: {key}')
    if cli['computation'] != expected['capabilities'] or cli['corpora'] != actual['corpora']:
        raise ValueError('Independent source CLI differs from measured semantic profile/corpus tuple')
    capacity_keys = [(r['profile'], r['backend'], r['workers']) for r in results['capacity']]
    expected_capacity = {(p, b, n) for p in runner.PROFILES for b in ('php', 'native') for n in (1, 2, 4, 8)}
    if len(capacity_keys) != len(set(capacity_keys)) or set(capacity_keys) != expected_capacity:
        raise ValueError('Incomplete capacity matrix')
    allocation_keys = [(r['profile'], r['backend']) for r in results['allocations']]
    if len(allocation_keys) != len(set(allocation_keys)) or set(allocation_keys) != {(p, b) for p in runner.PROFILES for b in ('php', 'native')}:
        raise ValueError('Incomplete allocation matrix')
    matrix = []
    for key, pair in sorted(pairs.items()):
        row = dict(zip(('profile', 'size', 'shape', 'phase'), key))
        row.update(digest=pair['php']['digest'], dataset_descriptor_sha256=pair['php']['dataset_descriptor_sha256'])
        if key[0] in ('document', 'preparation'):
            row['variable_text_fields'] = pair['php']['definition_fields']
            row['total_definition_fields'] = pair['php']['definition_fields'] + 2
        for backend in ('php', 'native'):
            source = pair[backend]
            row[backend] = {name: source[name] for name in ('latency_ns', 'compile_ns', 'work_units_per_second',
                'output_bytes', 'zend_used_bytes', 'zend_peak_bytes', 'rss_bytes', 'peak_rss_bytes')}
        row['native_over_php'] = runner.regression(pair['php']['durations_ns'], pair['native']['durations_ns'], .10)
        row['php_over_native_p50'] = pair['php']['latency_ns']['p50'] / pair['native']['latency_ns']['p50']
        matrix.append(row)
    capacity = []
    for source in results['capacity']:
        entry = {k: source[k] for k in ('profile', 'backend', 'workers', 'size', 'digest')}
        for name in ('burst', 'saturation'):
            if source[name]['jobs'] <= 0:
                raise ValueError('Empty capacity measurement')
            entry[name] = {k: v for k, v in source[name].items() if k != 'raw'}
            entry[name]['largest_worker_peak_rss_bytes'] = max(r['peak_rss_bytes'] for r in source[name]['raw'])
        capacity.append(entry)
    allocations = []
    for source in results['allocations']:
        if source['counters']['counters_overflowed'] or source['deepbind']['counters_overflowed']:
            raise ValueError('Overflowed instrumentation')
        entry = {k: source[k] for k in ('profile', 'backend', 'size', 'warmup', 'repetitions', 'counters', 'deepbind')}
        entry['digest'] = source['measurement']['digest']
        entry['peak_rss_bytes'] = source['measurement']['peak_rss_bytes']
        allocations.append(entry)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    raw_path = args.output.parent / 'results.json.gz'
    with args.results.open('rb') as source, raw_path.open('wb') as sink:
        with gzip.GzipFile(filename='', mode='wb', fileobj=sink, mtime=0) as encoded:
            for chunk in iter(lambda: source.read(1048576), b''):
                encoded.write(chunk)
    index = {'format': 'kumwe-app-native-evidence-index/1', 'complete': True,
        'scope': results['metadata']['scope'], 'timing_scope': results['metadata']['timing_scope'],
        'capacity_scope': results['metadata']['capacity_scope'],
        'canonical_comparison_scope': 'Shared App Definition/native generic subset: no floats, arrays <=512 members, depth <32; generated maps have <=256 members. No claim of a PHP baseline for the entire generic profile.',
        'raw_results': {'uncompressed': artifact(args.results), 'compressed': artifact(raw_path)},
        'artifact': artifact(module_path), 'independent_tuple': {'evidence': artifact(expected_path), 'value': expected},
        'independent_cli': {'evidence': artifact(cli_path), 'value': cli},
        'observed_tuple': artifact(observed_path), 'build_identity': artifact(build_path), 'ci': args.ci_url,
        'metadata': {key: results['metadata'][key] for key in ('started_utc', 'machine', 'arguments', 'sources',
            'harness_hashes', 'corpus_hashes', 'php_sha256', 'load_average_at_start', 'load_average_at_end')},
        'php_identity': results['metadata']['worker_identity']['php'],
        'matrix': matrix, 'capacity': capacity, 'capacity_summary': results['capacity_summary'],
        'allocation_scope': results['allocation_probe']['scope'], 'allocations': allocations,
        'release_or_cutover_authorized': False}
    if args.old_new_comparison:
        index['old_new_comparison'] = {'evidence': artifact(args.old_new_comparison), 'value': read(args.old_new_comparison)}
    if args.container_context:
        index['container_context'] = read(args.container_context)
    if args.source_context:
        index['source_context'] = {'evidence': artifact(args.source_context), 'value': read(args.source_context)}
    runner.write(args.output, index)
    print(json.dumps({'index': str(args.output), 'matrix_cases': len(matrix), 'capacity_cases': len(capacity),
                      'allocation_cases': len(allocations), 'raw_sha256': index['raw_results']['uncompressed']['sha256']}))


if __name__ == '__main__':
    main()
