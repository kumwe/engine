#!/usr/bin/env python3
"""Matched unchanged-App / actual PHP extension benchmark; Linux, Python stdlib only."""
import argparse
import concurrent.futures
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import random
import selectors
import statistics
import subprocess
import sys
import threading
import time

PROFILES = ('decimal', 'formula', 'document', 'preparation', 'report', 'canonical')
ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, value):
    target = Path(path)
    temporary = target.with_suffix(target.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')
    temporary.replace(target)


def command(args):
    result = subprocess.run(args, text=True, capture_output=True, check=False)
    return {'exit_code': result.returncode, 'stdout': result.stdout.strip(), 'stderr': result.stderr.strip()}


def revision(path):
    return {'path': str(path), 'head': command(['git', '-C', str(path), 'rev-parse', 'HEAD']),
            'status': command(['git', '-C', str(path), 'status', '--porcelain'])}


def distribution(values):
    if not values:
        return None
    ordered = sorted(values)
    return {'samples': len(values), 'mean': statistics.fmean(values),
            **{f'p{n}': ordered[max(0, math.ceil(n * len(ordered) / 100) - 1)] for n in (50, 95, 99)}}


def require_parity(result, expected, label):
    if not result.get('ok'):
        raise RuntimeError(f'{label}: worker failed: {result}')
    if expected is not None and result['digest'] != expected:
        raise RuntimeError(f'{label}: CORRECTNESS MISMATCH {result["digest"]} != {expected}')


def regression(previous, current, threshold):
    """Independent bootstrap median-ratio CI; admission needs >=20 samples each."""
    if min(len(previous), len(current)) < 20:
        return {'evaluated': False, 'reason': 'fewer than 20 samples'}
    rng = random.Random(1729)
    ratios = sorted(statistics.median(rng.choices(current, k=len(current))) /
                    statistics.median(rng.choices(previous, k=len(previous))) for _ in range(2000))
    return {'evaluated': True, 'median_ratio': statistics.median(current) / statistics.median(previous),
            'bootstrap_95_interval': [ratios[49], ratios[1949]],
            'regression': ratios[49] > 1 + threshold}


class Worker:
    def __init__(self, args, backend, label, extra_env=None):
        self.label = label
        self.lock = threading.Lock()
        config = {key: str(getattr(args, key)) for key in ('app', 'sdk', 'autoload', 'engine')}
        config['backend'] = backend
        config_path = args.output / (label + '.config.json')
        write(config_path, config)
        invocation = [str(args.php)]
        if backend == 'native':
            invocation += ['-d', 'extension=' + str(args.extension)]
        invocation += [str(args.engine / 'benchmarks/e2e/worker.php'), str(config_path)]
        self.stderr = (args.output / (label + '.stderr.log')).open('w')
        env = os.environ.copy()
        env.pop('USE_ZEND_ALLOC', None)
        if extra_env:
            env.update(extra_env)
        self.process = subprocess.Popen(invocation, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=self.stderr, text=True, bufsize=1, env=env)
        try:
            self.ready = self.read()
            if not self.ready.get('ready'):
                raise RuntimeError(f'{label}: failed startup: {self.ready}')
        except BaseException:
            self.process.kill(); self.process.wait(); self.stderr.close()
            raise

    def read(self):
        with selectors.DefaultSelector() as selector:
            selector.register(self.process.stdout, selectors.EVENT_READ)
            if not selector.select(180):
                self.process.kill()
                raise RuntimeError(self.label + ': worker timed out')
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError(self.label + ': worker exited; inspect stderr log')
        return json.loads(line)

    def request(self, job):
        with self.lock:
            self.process.stdin.write(json.dumps(job) + '\n')
            self.process.stdin.flush()
            return self.read()

    def close(self):
        if self.process.poll() is None:
            self.process.stdin.write('{"command":"stop"}\n')
            self.process.stdin.flush()
            try:
                self.process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
                raise RuntimeError(self.label + ': worker failed to stop')
        self.stderr.close()
        if self.process.returncode:
            raise RuntimeError(f'{self.label}: exit {self.process.returncode}')


def job(profile, size, shape='valid', phase='warm', iterations=1):
    return dict(profile=profile, size=size, shape=shape, phase=phase, iterations=iterations)


def matrix(args, workers, results):
    golden = {}
    for profile in args.profiles:
        phases = ('encode', 'digest') if profile == 'canonical' else (('warm',) if profile == 'decimal' else ('warm', 'cold'))
        for size in args.sizes:
            for shape in ('valid', 'hostile'):
                for phase in phases:
                    key = f'{profile}/{size}/{shape}/{phase}'
                    for backend, worker in workers.items():
                        warm = worker.request(job(profile, size, shape, phase, args.warmup))
                        require_parity(warm, golden.get(key), key + '/warmup/' + backend)
                        result = worker.request(job(profile, size, shape, phase, args.samples))
                        require_parity(result, golden.get(key, warm['digest']), key + '/' + backend)
                        golden[key] = result['digest']
                        result.update(key=key, backend=backend, profile=profile, size=size, shape=shape, phase=phase,
                                      latency_ns=distribution(result['durations_ns']),
                                      compile_ns=distribution(result['compile_durations_ns']),
                                      work_units_per_second=size * len(result['durations_ns']) * 1e9 / sum(result['durations_ns']))
                        results['matrix'].append(result)
                        write(args.output / 'results.json', results)
                        print(f'{key} {backend}: p50={result["latency_ns"]["p50"]/1e6:.3f}ms parity=ok', flush=True)
    return golden


def capacity(args, results):
    # Fixed backlogged burst + closed-loop saturation. Arrival/queue time is separate from worker service time.
    for profile in args.profiles:
        request = job(profile, args.capacity_size, phase='encode' if profile == 'canonical' else 'warm')
        expected = None
        for backend in ('php', 'native'):
            for count in args.workers:
                workers = []
                try:
                    for i in range(count):
                        workers.append(Worker(args, backend, f'capacity-{profile}-{backend}-{count}-{i}'))
                    for worker in workers:
                        warm = worker.request({**request, 'iterations': args.warmup})
                        require_parity(warm, expected, 'capacity warmup')
                        expected = warm['digest']
                    started = time.perf_counter_ns()
                    def burst_task(index):
                        entered = time.perf_counter_ns()
                        # Executor assigns at most one active task per worker via explicit queue ownership.
                        worker = workers[index % count]
                        with worker.lock:
                            service_start = time.perf_counter_ns()
                            worker.process.stdin.write(json.dumps(request) + '\n'); worker.process.stdin.flush()
                            response = worker.read()
                        done = time.perf_counter_ns()
                        require_parity(response, expected, 'burst')
                        return {'queue_ns': service_start - started, 'dispatch_wait_ns': service_start - entered,
                                'completion_ns': done - started, 'boundary_ns': done - service_start,
                                'service_ns': response['durations_ns'][0], 'peak_rss_bytes': response['peak_rss_bytes']}
                    with concurrent.futures.ThreadPoolExecutor(max_workers=count) as pool:
                        burst = list(pool.map(burst_task, range(args.burst_jobs)))
                    burst_wall = time.perf_counter_ns() - started
                    barrier = threading.Barrier(count)
                    def saturate(worker):
                        barrier.wait()
                        begin = time.perf_counter_ns(); deadline = begin + int(args.capacity_seconds * 1e9)
                        observations = []
                        while time.perf_counter_ns() < deadline:
                            before = time.perf_counter_ns(); response = worker.request(request); after = time.perf_counter_ns()
                            require_parity(response, expected, 'saturation')
                            observations.append({'boundary_ns': after - before, 'service_ns': response['durations_ns'][0],
                                                 'peak_rss_bytes': response['peak_rss_bytes']})
                        return {'wall_ns': time.perf_counter_ns() - begin, 'observations': observations}
                    with concurrent.futures.ThreadPoolExecutor(max_workers=count) as pool:
                        saturated = list(pool.map(saturate, workers))
                    samples = [o for part in saturated for o in part['observations']]
                    wall = max(part['wall_ns'] for part in saturated)
                    record = {'profile': profile, 'backend': backend, 'workers': count, 'size': args.capacity_size,
                              'digest': expected, 'burst': {'jobs': args.burst_jobs, 'wall_ns': burst_wall,
                              'work_units_per_second': args.burst_jobs * args.capacity_size * 1e9 / burst_wall,
                              'queue_ns': distribution([r['queue_ns'] for r in burst]),
                              'completion_ns': distribution([r['completion_ns'] for r in burst]), 'raw': burst},
                              'saturation': {'jobs': len(samples), 'wall_ns': wall,
                              'work_units_per_second': len(samples) * args.capacity_size * 1e9 / wall,
                              'boundary_ns': distribution([r['boundary_ns'] for r in samples]),
                              'service_ns': distribution([r['service_ns'] for r in samples]), 'raw': samples},
                              'worker_identity': [w.ready for w in workers]}
                    results['capacity'].append(record)
                    write(args.output / 'results.json', results)
                    print(f'capacity {profile} {backend} workers={count}: {record["saturation"]["work_units_per_second"]:.1f} units/s', flush=True)
                finally:
                    for worker in workers:
                        worker.close()


def allocations(args, results):
    probe = args.output / 'allocation_probe.so'
    compiled = command([args.cc, '-std=c11', '-O2', '-fPIC', '-shared', '-Wall', '-Wextra', '-Werror',
                        str(args.engine / 'benchmarks/e2e/allocation_probe.c'), '-o', str(probe)])
    if compiled['exit_code']:
        raise RuntimeError(f'Allocation probe build failed: {compiled}')
    deepbind = args.output / 'deepbind_probe.so'
    deepbind_build = command([args.cc, '-std=c11', '-O2', '-fPIC', '-shared', '-Wall', '-Wextra', '-Werror', '-pthread',
                             str(args.engine / 'benchmarks/e2e/deepbind_probe.c'), '-ldl', '-o', str(deepbind)])
    if deepbind_build['exit_code']:
        raise RuntimeError(f'Deepbind diagnostic build failed: {deepbind_build}')
    results['allocation_probe'] = {'build': compiled, 'sha256': sha(probe),
        'deepbind_build': deepbind_build, 'deepbind_sha256': sha(deepbind),
        'scope': 'Fresh whole process, successful glibc-interposed requested bytes/calls, USE_ZEND_ALLOC=0 and diagnostic RTLD_DEEPBIND removal; includes startup, compilation, warmup and measured jobs. Not live heap and not primary timing.'}
    for profile in args.profiles:
        expected = None
        for backend in ('php', 'native'):
            output = args.output / f'alloc-{profile}-{backend}.json'
            deepbind_output = args.output / f'deepbind-{profile}-{backend}.json'
            worker = Worker(args, backend, f'alloc-{profile}-{backend}',
                            {'USE_ZEND_ALLOC': '0', 'LD_PRELOAD': str(deepbind) + ':' + str(probe),
                             'KUMWE_BENCH_DISABLE_DEEPBIND': '1', 'KUMWE_BENCH_DEEPBIND_FILE': str(deepbind_output),
                             'KUMWE_BENCH_ALLOCATION_FILE': str(output)})
            try:
                request = job(profile, args.capacity_size, phase='encode' if profile == 'canonical' else 'warm')
                warm = worker.request({**request, 'iterations': args.warmup})
                require_parity(warm, expected, 'allocation warmup')
                measured = worker.request({**request, 'iterations': args.samples})
                require_parity(measured, expected or warm['digest'], 'allocation measurement')
                expected = measured['digest']
            finally:
                worker.close()
            counters = json.loads(output.read_text())
            deepbind_counts = json.loads(deepbind_output.read_text())
            if not deepbind_counts['disable_deepbind_enabled'] or deepbind_counts['counters_overflowed']:
                raise RuntimeError('Deepbind diagnostic missing or overflowed')
            if deepbind_counts['requested_deepbind_calls'] != deepbind_counts['stripped_deepbind_calls']:
                raise RuntimeError('Deepbind bypassed allocation instrumentation')
            if counters['counters_overflowed']:
                raise RuntimeError('Allocation diagnostic counter overflow')
            results['allocations'].append({'profile': profile, 'backend': backend, 'size': args.capacity_size,
                'warmup': args.warmup, 'repetitions': args.samples, 'counters': counters, 'deepbind': deepbind_counts, 'worker_identity': worker.ready,
                'measurement': measured})
            write(args.output / 'results.json', results)


def compare(args, results):
    if not args.baseline:
        return []
    old = json.loads(args.baseline.read_text())
    for key in ('machine', 'php_sha256', 'corpus_hashes'):
        if old['metadata'][key] != results['metadata'][key]:
            raise RuntimeError(f'Regression comparison requires matching {key}')
    for key in ('php', 'php_binary_sha256', 'icu', 'sources', 'conversion_reference'):
        if old['metadata']['worker_identity']['php'][key] != results['metadata']['worker_identity']['php'][key]:
            raise RuntimeError(f'Regression comparison requires matching PHP semantic baseline {key}')
    previous = {(r['key'], r['backend']): r for r in old['matrix']}
    checks = []
    for row in results['matrix']:
        key = (row['key'], row['backend'])
        if key not in previous:
            raise RuntimeError(f'Missing baseline workload {key}')
        require_parity(row, previous[key]['digest'], 'regression baseline')
        checks.append({'key': key, **regression(previous[key]['durations_ns'], row['durations_ns'], args.regression_threshold)})
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--php', type=Path, required=True)
    parser.add_argument('--extension', type=Path, required=True)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--autoload', type=Path, required=True)
    parser.add_argument('--engine', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--sizes', nargs='+', type=int, default=[1, 32, 256, 4096])
    parser.add_argument('--samples', type=int, default=30)
    parser.add_argument('--warmup', type=int, default=10)
    parser.add_argument('--workers', nargs='+', type=int, default=[1, 2, 4, 8])
    parser.add_argument('--capacity-seconds', type=float, default=3)
    parser.add_argument('--capacity-size', type=int, default=32)
    parser.add_argument('--burst-jobs', type=int, default=64)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--demand', action='append', default=[], metavar='PROFILE=UNITS_PER_SECOND',
                        help='Explicit observed deployment demand for measured headroom; no assumed arrival rate')
    parser.add_argument('--regression-threshold', type=float, default=0.10)
    parser.add_argument('--smoke', action='store_true', help='Functional gate only; insufficient samples for performance claims')
    args = parser.parse_args()
    if args.smoke:
        args.sizes = [1, 32]; args.samples = 3; args.warmup = 1; args.workers = [1, 2]
        args.capacity_seconds = .15; args.burst_jobs = 4
    if (platform.system() != 'Linux' or min(args.sizes + [args.capacity_size]) < 1 or max(args.sizes + [args.capacity_size]) > 4096
            or min(args.workers) < 1 or max(args.workers) > 128 or not 1 <= args.samples <= 10000
            or not 1 <= args.warmup <= 10000 or not 0 < args.capacity_seconds <= 3600 or not 1 <= args.burst_jobs <= 100000):
        parser.error('Invalid bounds or unsupported non-Linux host')
    for name in ('php', 'extension', 'app', 'sdk', 'autoload', 'engine'):
        path = getattr(args, name).resolve()
        if not path.exists():
            parser.error(f'{name} does not exist: {path}')
        setattr(args, name, path)
    demands = {}
    for demand in args.demand:
        try:
            name, raw = demand.split('=', 1); rate = float(raw)
            if name not in args.profiles or not math.isfinite(rate) or rate <= 0:
                raise ValueError()
            demands[name] = rate
        except ValueError:
            parser.error('Each demand must be a selected PROFILE=positive-units-per-second')
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    lscpu = command(['lscpu'])['stdout']
    stable_cpu = '\n'.join(line for line in lscpu.splitlines() if not any(word in line for word in ('MHz', 'BogoMIPS', 'scaling')))
    machine = {'platform': platform.platform(), 'cpu': stable_cpu, 'cpu_count': os.cpu_count()}
    metadata = {'format': 'kumwe-app-native-performance/1', 'started_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        'machine': machine, 'lscpu': lscpu, 'php_sha256': sha(args.php), 'extension_sha256': sha(args.extension),
        'compiler': command([args.cc, '--version']), 'arguments': {k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()},
        'sources': {name: revision(getattr(args, name)) for name in ('engine', 'app', 'sdk')},
        'harness_hashes': {p.name: sha(p) for p in (args.engine / 'benchmarks/e2e').iterdir() if p.is_file()},
        'corpus_hashes': {str(p.relative_to(args.engine)): sha(p) for p in (args.engine / 'corpus').rglob('*') if p.is_file()},
        'load_average_at_start': os.getloadavg(),
        'scope': 'Whole unchanged App pure semantic methods versus actual Zend extension boundary, including host preparation codec, JSON/KED serialization, copies, decoding and final output encoding. SQL, HTTP, authorization, persistence and allocation services are outside both paths.',
        'timing_scope': 'hrtime inside worker excludes correctness digest; capacity boundary includes process IPC and correctness verification; cold includes host plan construction, native compile and destruction.',
        'capacity_scope': 'Backlogged burst and closed-loop saturation, synthetic deterministic owner-derived workload. No production arrival-rate or HTTP capacity claim.',
        'smoke': args.smoke}
    if args.build_dir:
        cache = args.build_dir / 'CMakeCache.txt'
        metadata['cmake_cache'] = cache.read_text() if cache.exists() else None
    results = {'metadata': metadata, 'matrix': [], 'capacity': [], 'allocations': [], 'complete': False}
    workers = {}
    try:
        for backend in ('php', 'native'):
            workers[backend] = Worker(args, backend, 'matrix-' + backend)
        metadata['worker_identity'] = {key: w.ready for key, w in workers.items()}
        if workers['php'].ready['sources'] != workers['native'].ready['sources']:
            raise RuntimeError('App source mismatch between backends')
        matrix(args, workers, results)
        for worker in workers.values():
            worker.close()
        workers = {}
        capacity(args, results)
        allocations(args, results)
        results['capacity_summary'] = []
        for profile in args.profiles:
            for backend in ('php', 'native'):
                points = [r for r in results['capacity'] if r['profile'] == profile and r['backend'] == backend]
                peak = max(points, key=lambda r: r['saturation']['work_units_per_second'])
                achieved = peak['saturation']['work_units_per_second']
                results['capacity_summary'].append({'profile': profile, 'backend': backend,
                    'maximum_observed_units_per_second': achieved, 'workers_at_maximum': peak['workers'],
                    'declared_demand_units_per_second': demands.get(profile),
                    'observed_headroom_fraction': achieved / demands[profile] - 1 if profile in demands else None})
        results['regression_checks'] = compare(args, results)
        results['complete'] = True
        results['correctness_passed'] = True
        results['statistical_sample_floor_met'] = not args.smoke and args.samples >= 20
        results['regression_gate'] = ('failed' if any(c.get('regression') for c in results['regression_checks']) else
                                      'passed' if results['regression_checks'] and all(c['evaluated'] for c in results['regression_checks']) else 'not_evaluated')
        by_key = {}
        for row in results['matrix']:
            by_key.setdefault(row['key'], {})[row['backend']] = row
        results['comparisons'] = [{'key': key,
            'php_over_native_p50': pair['php']['latency_ns']['p50'] / pair['native']['latency_ns']['p50'],
            'native_over_php_median_bootstrap': regression(pair['php']['durations_ns'], pair['native']['durations_ns'], args.regression_threshold)}
            for key, pair in by_key.items()] 
        results['metadata']['load_average_at_end'] = os.getloadavg()
        write(args.output / 'results.json', results)
        return 1 if any(c.get('regression') for c in results['regression_checks']) else 0
    except BaseException as failure:
        results['failure'] = str(failure)
        write(args.output / 'results.json', results)
        raise
    finally:
        for worker in workers.values():
            worker.close()


if __name__ == '__main__':
    sys.exit(main())
