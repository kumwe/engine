#!/usr/bin/env python3
"""Compare every retained baseline case with a complete, compatible current run."""
import argparse
import hashlib
import json
from pathlib import Path
import run as runner


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--current', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--regression-threshold', type=float, default=.10)
    args = parser.parse_args()
    old = json.loads(args.baseline.read_text())
    current = json.loads(args.current.read_text())
    if not current.get('complete') or not current.get('correctness_passed'):
        raise ValueError('Current run must be complete and parity verified')
    previous = {(row['key'], row['backend']) for row in old['matrix']}
    if len(previous) != len(old['matrix']) or not previous:
        raise ValueError('Baseline must contain unique nonempty cases')
    selected = [row for row in current['matrix'] if (row['key'], row['backend']) in previous]
    if len(selected) != len(previous):
        raise ValueError('Current run does not cover the retained baseline')
    checks = runner.compare(args, {**current, 'matrix': selected})
    if not all(check['evaluated'] for check in checks):
        raise ValueError('All retained comparisons require at least twenty samples')
    failed = any(check['regression'] for check in checks)
    evidence = {
        'format': 'kumwe-retained-baseline-comparison/1',
        'scope': 'Every retained workload/backend case; this is not a full historical matrix or capacity regression gate.',
        'baseline_complete': old.get('complete', False),
        'baseline_sha256': hashlib.sha256(args.baseline.read_bytes()).hexdigest(),
        'current_sha256': hashlib.sha256(args.current.read_bytes()).hexdigest(),
        'metadata_admission_passed': True,
        'compared_cases': len(checks),
        'current_cases_without_baseline': len(current['matrix']) - len(checks),
        'regression_threshold': args.regression_threshold,
        'regression_gate': 'failed' if failed else 'passed',
        'checks': checks,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    runner.write(args.output, evidence)
    print(json.dumps({'compared': len(checks), 'statistical_regressions': sum(bool(row['regression']) for row in checks)}))
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
