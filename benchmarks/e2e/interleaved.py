#!/usr/bin/env python3
"""Compare complete calls through unchanged PHP, previous, and current native workers."""
import argparse
import os
from pathlib import Path
import random
import time
import run as runner


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('app', 'sdk', 'autoload', 'php', 'old-extension', 'new-extension', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--engine', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--sizes', nargs='+', type=int, default=[32, 256])
    parser.add_argument('--profiles', nargs='+', choices=runner.PROFILES, default=list(runner.PROFILES))
    parser.add_argument('--rounds', type=int, default=60)
    parser.add_argument('--warmup', type=int, default=10)
    args = parser.parse_args()
    if args.rounds < 20 or args.warmup < 1 or any(size < 1 or size > 4096 for size in args.sizes):
        parser.error('At least 20 rounds, positive warmup and sizes 1..4096 are required.')
    args.output.mkdir(parents=True, exist_ok=True)
    results = {
        'format': 'kumwe-interleaved-complete-call/1', 'complete': False,
        'started_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        'method': 'Deterministic randomized PHP/old/new order each round; unchanged complete-call worker; digest parity checked on every call. Shared development host; no deployment acceptance claim.',
        'arguments': {key: str(value) if isinstance(value, Path) else value for key, value in vars(args).items()},
        'load_start': os.getloadavg(), 'engine': runner.revision(args.engine),
        'worker_source_sha256': runner.sha(args.engine / 'benchmarks/e2e/worker.php'),
        'module_sha256': {'old': runner.sha(args.old_extension), 'new': runner.sha(args.new_extension)},
        'matrix': [], 'workers': {},
    }
    workers = {}
    try:
        workers['php'] = runner.Worker(args, 'php', 'php')
        args.extension = args.old_extension
        workers['old'] = runner.Worker(args, 'native', 'old')
        args.extension = args.new_extension
        workers['new'] = runner.Worker(args, 'native', 'new')
        results['workers'] = {label: worker.ready for label, worker in workers.items()}
        rng = random.Random(7001)
        for size in args.sizes:
            for profile in args.profiles:
                for phase in ('encode', 'digest') if profile == 'canonical' else ('warm',):
                    request = runner.job(profile, size, phase=phase)
                    expected = None
                    raw = {label: [] for label in workers}
                    for label, worker in workers.items():
                        response = worker.request({**request, 'iterations': args.warmup})
                        runner.require_parity(response, expected, 'warmup/' + label)
                        expected = response['digest']
                    for _ in range(args.rounds):
                        order = list(workers)
                        rng.shuffle(order)
                        for label in order:
                            response = workers[label].request(request)
                            runner.require_parity(response, expected, label)
                            raw[label].append(response['durations_ns'][0])
                    record = {
                        'profile': profile, 'size': size, 'phase': phase, 'shape': 'valid', 'digest': expected,
                        'dataset_descriptor_sha256': response['dataset_descriptor_sha256'],
                        'raw_ns': raw, 'latency_ns': {label: runner.distribution(values) for label, values in raw.items()},
                        'new_over_old': runner.regression(raw['old'], raw['new'], 0.1),
                        'new_over_php': runner.regression(raw['php'], raw['new'], 0.1),
                    }
                    results['matrix'].append(record)
                    runner.write(args.output / 'results.json', results)
                    medians = {label: round(values['p50'] / 1e6, 4) for label, values in record['latency_ns'].items()}
                    print(profile, size, phase, medians, flush=True)
        results.update(complete=True, correctness_passed=True, load_finish=os.getloadavg())
        runner.write(args.output / 'results.json', results)
    finally:
        for worker in workers.values():
            worker.close()


if __name__ == '__main__':
    main()
