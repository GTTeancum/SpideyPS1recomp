"""Conservative CPU envelopes must not use reads overlapping a stall boundary."""
import unittest

from performance_stalls import cpu_bracket, sample_coverage, host_counter_windows


class HostCounterTests(unittest.TestCase):
    def test_retains_coarse_window_and_uncapped_value(self):
        samples = [dict(previous_collection_start=1, collection_start=2,
                        collection_end=2.01, values={'performance': 115.9}, errors={})]
        result = host_counter_windows(1.2, 1.3, samples)
        self.assertEqual(result[0]['collection_envelope_ms'], 1010)
        self.assertEqual(result[0]['values']['performance'], 115.9)
        self.assertEqual(result[0]['last_collection_ms'], 10)

    def test_does_not_extrapolate_outside_collections(self):
        samples = [dict(previous_collection_start=1, collection_start=2,
                        collection_end=2.01, values={}, errors={})]
        self.assertEqual(host_counter_windows(0, 1, samples), [])
        self.assertEqual(host_counter_windows(2.01, 3, samples), [])

    def test_skips_unprimed_rates_and_preserves_unavailable(self):
        base = dict(collection_start=2, collection_end=2.01,
                    values={'frequency': None}, errors={'frequency': 'unavailable'})
        self.assertEqual(host_counter_windows(1, 3, [dict(base, previous_collection_start=None)]), [])
        result = host_counter_windows(1, 3, [dict(base, previous_collection_start=1)])
        self.assertIsNone(result[0]['values']['frequency'])
        self.assertEqual(result[0]['errors']['frequency'], 'unavailable')


class CpuBracketTests(unittest.TestCase):
    def setUp(self):
        self.samples = [
            dict(qpc_seconds_start=0, qpc_seconds_end=.01, cpu=1),
            dict(qpc_seconds_start=.9, qpc_seconds_end=1.1, cpu=1.05),
            dict(qpc_seconds_start=2.1, qpc_seconds_end=2.11, cpu=1.125),
        ]

    def test_skips_collection_overlapping_boundary(self):
        bracket = cpu_bracket(1, 2, self.samples)
        self.assertEqual((bracket['start'], bracket['end']), (0, 2.11))
        self.assertEqual(bracket['whole_process_cpu_ms'], 125)
        self.assertEqual(bracket['non_cpu_wall_lower_estimate_ms'], 843.75)

    def test_requires_both_coverage_bounds(self):
        self.assertIsNone(cpu_bracket(0, 2, self.samples))
        self.assertIsNone(cpu_bracket(1, 3, self.samples))
        self.assertIsNone(cpu_bracket(1, 2, []))

    def test_busy_multithreaded_process_does_not_imply_negative_wait(self):
        self.samples[-1]['cpu'] = 3
        self.assertEqual(cpu_bracket(1, 2, self.samples)['non_cpu_wall_lower_estimate_ms'], 0)

    def test_counter_reset_is_unusable(self):
        self.samples[-1]['cpu'] = .5
        self.assertIsNone(cpu_bracket(1, 2, self.samples))

    def test_collection_at_exact_boundary_is_usable(self):
        self.samples[1]['qpc_seconds_end'] = 1
        self.assertEqual(cpu_bracket(1, 2, self.samples)['start'], .9)


class SampleCoverageTests(unittest.TestCase):
    def test_empty_interval_has_measured_gap_not_a_continuous_stack(self):
        result = sample_coverage(1, 2, [.9, 2.1])
        self.assertTrue(result['bounded_by_samples'])
        self.assertEqual(result['samples_in_interval'], 0)
        self.assertEqual(result['largest_sample_gap_ms'], 1200)

    def test_missing_end_coverage_is_explicit(self):
        result = sample_coverage(1, 2, [.9, 1.2])
        self.assertFalse(result['bounded_by_samples'])
        self.assertEqual(result['samples_in_interval'], 1)
        self.assertIsNone(sample_coverage(1, 2, []))

    def test_counts_exact_boundaries(self):
        result = sample_coverage(1, 2, [1, 1.5, 2])
        self.assertTrue(result['bounded_by_samples'])
        self.assertEqual(result['samples_in_interval'], 3)
        self.assertEqual(result['largest_sample_gap_ms'], 500)


if __name__ == '__main__':
    unittest.main()
