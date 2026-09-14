"""Consumer evidence binding controls; geometric predicates are tested upstream."""
from copy import deepcopy
from types import SimpleNamespace as NS
import unittest

from measure_live_finisher_regions import montage_coverage, native_controls_passed, validate_participant


class LiveFinisherRegionsTests(unittest.TestCase):
    def attempts(self):
        return [dict(actual_montage_s=t, acquired=True, output_count=2)
                for t in (.38, .413, .447, .48, .513, .547, .58)]

    def test_guard_samples_bracket_without_inventing_endpoints(self):
        result = montage_coverage(self.attempts(), .4, .55, .05, 7)
        self.assertEqual(result['status'], 'bracketed_samples')
        self.assertFalse(result['exact_start_observed'])
        self.assertFalse(result['exact_end_observed'])

    def test_acquisition_failure_is_not_filtered_out(self):
        rows = self.attempts(); rows[3].update(acquired=False, output_count=0)
        result = montage_coverage(rows, .4, .55, .05, 7)
        self.assertEqual(result['status'], 'insufficient')
        self.assertIn('acquisition_loss', result['reasons'])
        self.assertEqual(len(result['actual_montage_s']), 7)

    def test_missing_or_late_attempt_does_not_shrink_interval(self):
        result = montage_coverage(self.attempts()[1:], .4, .55, .05, 7)
        self.assertIn('missing_attempts', result['reasons'])
        self.assertIn('requested_interval_not_bracketed', result['reasons'])

    def test_actual_gap_is_used_without_sorting(self):
        rows = self.attempts(); rows[3]['actual_montage_s'] = .54
        result = montage_coverage(rows, .4, .55, .05, 7)
        self.assertIn('montage_gap_exceeds_limit', result['reasons'])
        self.assertIn('non_increasing_montage_positions', result['reasons'])

    def test_nonfinite_phases_and_malformed_acquisition_reject(self):
        for field, value in [('actual_montage_s', float('nan')), ('actual_montage_s', float('inf')),
                             ('actual_montage_s', True), ('acquired', 1), ('output_count', True)]:
            with self.subTest(field=field, value=value):
                rows = self.attempts(); rows[3][field] = value
                with self.assertRaises(ValueError):
                    montage_coverage(rows, .4, .55, .05, 7)

    def test_invalid_requested_interval_rejects(self):
        for values in [(float('nan'), .55, .05, 7), (.55, .4, .05, 7), (.4, .55, 0, 7),
                       (.4, .55, .05, True)]:
            with self.subTest(values=values), self.assertRaises(ValueError):
                montage_coverage(self.attempts(), *values)

    def participant(self):
        observation = NS(component_id='victim', component_generation=2, configuration_id='config',
            pose=NS(subject_id='victim', stream_id='live-finisher-batch', frame_id=42, revision=9),
            topology=NS(asset_id='asset', lod=0, identity='topology'))
        completion = NS(status='completed', observation=observation, request=NS(request_id='pair-1'))
        row = dict(batch_request_id='pair-1', engine_frame=42, world_in_tick=False,
            inventory={'victim': {'asset': 'asset'}}, victim=dict(bundle='victim-export-1',
                exported=True, export_error='', request_id='pair-1', topology_id='topology',
                component_generation=2, configuration_id='config', engine_frame=42, pose_revision=9))
        return completion, row

    def test_bundle_name_is_distinct_from_shared_request(self):
        completion, row = self.participant()
        self.assertIs(validate_participant(completion, row, 'victim'), completion.observation)

    def test_wrong_request_generation_frame_and_pose_reject(self):
        for field, value in [('request_id','other'), ('component_generation',3),
                             ('engine_frame',43), ('pose_revision',10), ('exported',False)]:
            with self.subTest(field=field):
                completion, row = self.participant(); row['victim'][field] = value
                with self.assertRaises(ValueError):
                    validate_participant(completion, row, 'victim')

    def test_wrong_reason_is_not_a_passing_native_control(self):
        controls = dict(exhausted_snapshot_budget=dict(rejected_without_partial_pair=True,
                error='admission exhausted'), duplicate_component=dict(rejected_without_partial_pair=True,
                error='non-null and unique'), missing_subsequent_finalization=dict(
                rejected_without_partial_pair=True,error='no current finalized pose witness'),
            unwitnessed_control_enrolled=True, budget_control_retained_snapshots=0,
            budget_control_retained_bytes=0)
        self.assertTrue(native_controls_passed(controls))
        wrong = deepcopy(controls); wrong['exhausted_snapshot_budget']['error'] = 'null sampler'
        self.assertFalse(native_controls_passed(wrong))
        wrong = deepcopy(controls); wrong['budget_control_retained_snapshots'] = 1
        self.assertFalse(native_controls_passed(wrong))


if __name__ == '__main__':
    unittest.main()
