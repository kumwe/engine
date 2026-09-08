"""Deterministic harness admission tests; these are not performance measurements."""
import importlib.util
import copy
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('runner', Path(__file__).with_name('run.py'))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class AdmissionTest(unittest.TestCase):
    def test_corrupted_or_missing_semantics_fail_closed(self):
        for result in ({'ok': True, 'digest': 'changed'}, {'ok': False, 'error': 'refused'}):
            with self.assertRaises(RuntimeError):
                runner.require_parity(result, 'actual-php-oracle', 'deliberate fault')
        runner.require_parity({'ok': True, 'digest': 'actual-php-oracle'}, 'actual-php-oracle', 'same')

    def test_regression_requires_enough_evidence(self):
        self.assertFalse(runner.regression([100] * 19, [200] * 30, .1)['evaluated'])
        self.assertTrue(runner.regression([100] * 30, [200] * 30, .1)['regression'])
        self.assertFalse(runner.regression([100] * 30, [100] * 30, .1)['regression'])
        self.assertFalse(runner.regression([100] * 30, [50] * 30, .1)['regression'])
        # Wide overlapping samples must not trigger a one-run threshold.
        self.assertFalse(runner.regression(list(range(1, 101)), list(range(2, 102)), .1)['regression'])

    def test_changed_workload_or_build_cannot_enter_comparison(self):
        metadata = {'machine': {}, 'php_sha256': 'php', 'corpus_hashes': {'corpus': 'fixed'},
                    'harness_hashes': {'worker.php': 'workload'},
                    'worker_identity': {'php': {key: 'fixed' for key in
                        ('php', 'php_binary_sha256', 'icu', 'sources', 'conversion_reference', 'memory_limit')},
                    'native': {'capabilities': {'binding_build': {key: 'fixed' for key in
                        ('architecture', 'compiler', 'engine_flags', 'extension_flags', 'libc',
                         'sanitizers', 'thread_model', 'zend_module_api', 'debug')}}}}}
        runner.require_comparable_metadata(metadata, copy.deepcopy(metadata))
        altered = copy.deepcopy(metadata)
        altered['harness_hashes']['worker.php'] = 'changed-work'
        with self.assertRaises(RuntimeError):
            runner.require_comparable_metadata(metadata, altered)
        altered = copy.deepcopy(metadata)
        altered['worker_identity']['php']['memory_limit'] = 'different'
        with self.assertRaises(RuntimeError):
            runner.require_comparable_metadata(metadata, altered)
        for key in ('engine_flags', 'compiler', 'sanitizers'):
            altered = copy.deepcopy(metadata)
            altered['worker_identity']['native']['capabilities']['binding_build'][key] = 'different'
            with self.assertRaises(RuntimeError):
                runner.require_comparable_metadata(metadata, altered)

    def test_tail_percentiles_keep_slow_samples(self):
        observed = runner.distribution(list(range(1, 101)))
        self.assertEqual((50, 95, 99), (observed['p50'], observed['p95'], observed['p99']))


if __name__ == '__main__':
    unittest.main()
