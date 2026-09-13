"""Evaluate Katana's explicit live region profile without repairing observation clocks.

Geometry and interval operations belong to AnimationAnalysis. This adapter owns the
Katana participant mapping, declared interval, evidence validation and disposition.
"""
from __future__ import annotations

import argparse
from dataclasses import replace
import hashlib
import json
import math
from pathlib import Path
import time

from measure_finisher_regions import (ANALYSIS_LIMITS, RECORD_LIMITS, EXCLUSIONS,
    MeshAnalysisLimits, MeshPairSample, MeshRegion, MeshSelection, TimeInterval,
    measure_mesh_pair, read_mesh_observation, requirement, summarize_mesh_interval)

ROLES = ('victim', 'katana')
REGION_NAMES = ('victim-head-neck-box', 'equipped-katana-complete-surface')
DEFAULT_PROFILE = Path(__file__).parent / 'regions/finisher-head-katana.json'


def montage_coverage(attempts, start, end, max_gap, expected):
    """Assess actual attempted phases; guard poses do not create endpoint samples."""
    def finite(value):
        return type(value) in (int, float) and math.isfinite(value)

    if (not all(finite(v) for v in (start, end, max_gap)) or end <= start or start < 0
            or max_gap <= 0 or type(expected) is not int or expected < 2):
        raise ValueError('invalid declared montage interval, cadence or attempt count')
    if any(not finite(r['actual_montage_s']) or r['actual_montage_s'] < 0
           or type(r['acquired']) is not bool or type(r['output_count']) is not int
           or r['output_count'] < 0 for r in attempts):
        raise ValueError('invalid actual montage phase or acquisition status')
    positions = [r['actual_montage_s'] for r in attempts]
    reasons = []
    if len(attempts) != expected:
        reasons.append('missing_attempts')
    if any(not r['acquired'] or r['output_count'] != 2 for r in attempts):
        reasons.append('acquisition_loss')
    if not positions or positions[0] > start or positions[-1] < end:
        reasons.append('requested_interval_not_bracketed')
    gaps = [b-a for a, b in zip(positions, positions[1:])]
    if any(g <= 0 for g in gaps):
        reasons.append('non_increasing_montage_positions')
    if any(g > max_gap for g in gaps):
        reasons.append('montage_gap_exceeds_limit')
    return dict(status='insufficient' if reasons else 'bracketed_samples', reasons=reasons,
        actual_montage_s=positions, gaps_s=gaps,
        exact_start_observed=start in positions, exact_end_observed=end in positions)


def validate_participant(completion, row, role):
    """Export names may differ, but each record must belong to this declared batch."""
    meta, inventory = row[role], row['inventory'][role]
    obs = completion.observation
    if (completion.status != 'completed' or obs is None or not meta['exported']
            or meta['export_error'] or completion.request.request_id != row['batch_request_id']
            or meta['request_id'] != row['batch_request_id']):
        raise ValueError('incomplete export or batch request mismatch')
    if (obs.component_id != role or obs.pose.subject_id != role
            or obs.pose.stream_id != 'live-finisher-batch'
            or obs.topology.asset_id != inventory['asset'] or obs.topology.lod != 0
            or obs.topology.identity != meta['topology_id']
            or obs.component_generation != meta['component_generation']
            or obs.configuration_id != meta['configuration_id']
            or obs.pose.frame_id != row['engine_frame']
            or obs.pose.frame_id != meta['engine_frame']
            or obs.pose.revision != meta['pose_revision'] or row['world_in_tick'] is not False):
        raise ValueError('participant, asset, LOD, generation, pose or tick identity mismatch')
    # The native replay is the full-precision authority. The ancillary UE JSON
    # inventory uses fewer decimal digits; retain its numerical delta in the report.
    return obs


def native_controls_passed(controls):
    expected = {'exhausted_snapshot_budget': 'admission exhausted',
        'duplicate_component': 'non-null and unique',
        'missing_subsequent_finalization': 'no current finalized pose witness'}
    return (all(controls.get(name, {}).get('rejected_without_partial_pair') is True
                and reason in controls[name].get('error', '') for name, reason in expected.items())
        and controls.get('unwitnessed_control_enrolled') is True
        and controls.get('budget_control_retained_snapshots') == 0
        and controls.get('budget_control_retained_bytes') == 0)


def visible_requirement(obs):
    needed = ('material_displacement', 'raster_visibility')
    base = requirement(obs)
    return replace(base, required_features=(*base.required_features, *needed),
                   allowed_exclusions=tuple(f for f in EXCLUSIONS if f not in needed))


def evaluate(reference, output, profile=DEFAULT_PROFILE, limits=ANALYSIS_LIMITS):
    reference, output, profile = map(lambda p: Path(p).resolve(), (reference, output, profile))
    output.mkdir(parents=True, exist_ok=False)
    manifest = json.loads((reference / 'live-pairs.json').read_text(encoding='utf-8-sig'))
    mapping = json.loads(profile.read_text())
    regions = {role: MeshRegion(name, mapping[role]['topology'], mapping[role]['triangle_ids'],
                               ANALYSIS_LIMITS) for role, name in zip(ROLES, REGION_NAMES)}
    if any(regions[r].identity != mapping[r]['identity'] for r in ROLES):
        raise ValueError('retained region identity does not match explicit selection')
    report = dict(format='katana_live_finisher_mesh_qualification', schema_version=1,
        reference=str(reference), profile_sha256=hashlib.sha256(profile.read_bytes()).hexdigest(),
        region_triangles={r: len(regions[r].triangle_ids) for r in ROLES}, tolerance_cm=.5,
        search_budget=dict(max_triangles=limits.max_triangles, max_pair_tests=limits.max_pair_tests,
                           max_node_visits=limits.max_node_visits, max_coordinate_bits=limits.max_coordinate_bits),
        montage_coverage=montage_coverage(manifest['attempts'],
            manifest['required_interval_start_montage_s'], manifest['required_interval_end_montage_s'],
            manifest['required_max_gap_s'], manifest['expected_attempts']),
        attempts=[], native_controls=manifest.get('native_controls', {}),
        native_controls_passed=native_controls_passed(manifest.get('native_controls', {})),
        stop_reason=manifest['stop_reason'], containment='not_evaluated')
    results, visible_results, selections_by_attempt, acquired = [], [], [], []

    def save():
        (output / 'measurements.json').write_text(json.dumps(report, indent=2) + '\n')

    # Write after each attempt so a bounded analysis interruption retains progress.
    save()
    for row in manifest['attempts']:
        entry = {k: row[k] for k in ('batch_request_id', 'requested_montage_s', 'actual_montage_s',
            'engine_frame', 'capture_wall_ms', 'inventory_wall_ms', 'acquired', 'capture_error')}
        report['attempts'].append(entry)
        if not row['acquired']:
            entry['status'] = 'insufficient'
            save()
            continue
        pair = [read_mesh_observation(reference, row[r]['bundle'], limits=RECORD_LIMITS) for r in ROLES]
        observations = [validate_participant(c, row, r) for c, r in zip(pair, ROLES)]
        selections = [MeshSelection(o, regions[r], requirement(o)) for r, o in zip(ROLES, observations)]
        sample = MeshPairSample(row['batch_request_id'], observations[0].acquired, *selections)
        started = time.perf_counter()
        result = measure_mesh_pair(sample, tolerance=.5, limits=limits)
        entry.update(status=result.status, geometry=result.to_mapping(),
            offline_pair_wall_ms=(time.perf_counter()-started)*1000,
            acquisition_skew_ms=(observations[1].acquired.seconds-observations[0].acquired.seconds)*1000,
            completion_latency_ms=[(c.completed.seconds-o.acquired.seconds)*1000 for c,o in zip(pair,observations)],
            inventory_acquisition_rounding_delta_s=[row[r]['acquired_s']-o.acquired.seconds
                                                    for r,o in zip(ROLES,observations)])
        visible = [replace(s, requirement=visible_requirement(s.observation)) for s in selections]
        visible_result = measure_mesh_pair(replace(sample, first=visible[0],
            second=visible[1]), tolerance=.5, limits=limits)
        entry['required_visible_surface'] = visible_result.to_mapping()
        visible_results.append(visible_result)
        results.append(result); selections_by_attempt.append(selections); acquired.append(result.acquired.seconds)
        save()
        print(json.dumps(dict(request=sample.sample_id, status=result.status,
            distance_cm=result.minimum_distance, wall_ms=entry['offline_pair_wall_ms'])), flush=True)

    if len(results) >= 2:
        interval = TimeInterval(results[0].acquired, results[-1].acquired)
        report['observed_acquisition_interval'] = summarize_mesh_interval(results, interval,
            max_gap_seconds=manifest['required_max_gap_s'], max_samples=20).to_mapping()
        report['required_visible_interval'] = summarize_mesh_interval(visible_results, interval,
            max_gap_seconds=manifest['required_max_gap_s'], max_samples=20).to_mapping()
        report['actual_acquisition_gaps_s'] = [b-a for a,b in zip(acquired, acquired[1:])]
        report['achieved_mean_hz'] = (len(acquired)-1)/(acquired[-1]-acquired[0])
        # Actual endpoint observations, with a deliberately smaller allowed gap.
        measured = [r for r in results if r.status == 'measured']
        endpoints = [measured[0], measured[-1]] if len(measured) >= 2 else [results[0], results[-1]]
        control_interval = TimeInterval(endpoints[0].acquired, endpoints[-1].acquired)
        report['gap_control'] = summarize_mesh_interval(endpoints, control_interval,
            max_gap_seconds=(endpoints[-1].acquired.seconds-endpoints[0].acquired.seconds)/2,
            max_samples=20).to_mapping()

    if selections_by_attempt:
        first, second = selections_by_attempt[0]
        sample = MeshPairSample('live-negative-controls', first.observation.acquired, first, second)
        mismatches = {
            'wrong_topology': replace(first, region=replace(first.region, topology_id='0'*64)),
            'wrong_generation': replace(first, requirement=replace(first.requirement,
                component_generation=first.requirement.component_generation+1)),
            'wrong_pose': replace(first, requirement=replace(first.requirement,
                pose=replace(first.requirement.pose, revision=first.requirement.pose.revision+1))),
            'required_cloth': replace(first, requirement=replace(first.requirement,
                required_features=(*first.requirement.required_features, 'cloth'),
                allowed_exclusions=tuple(f for f in EXCLUSIONS if f != 'cloth')))}
        controls = {name: measure_mesh_pair(replace(sample, first=value), tolerance=.5,
                                           limits=ANALYSIS_LIMITS) for name,value in mismatches.items()}
        controls['exhausted_analysis_budget'] = measure_mesh_pair(sample, tolerance=.5,
            limits=MeshAnalysisLimits(1, 1, 1, 256))
        # Use another real frame as the wrong partner; never manufacture a timestamp.
        if len(selections_by_attempt) >= 2:
            controls['wrong_acquisition'] = measure_mesh_pair(replace(sample,
                second=selections_by_attempt[1][1]), tolerance=.5, limits=ANALYSIS_LIMITS)
        expected = {'wrong_topology': 'first:region_topology_mismatch',
            'wrong_generation': 'first:coverage:component_generation:mismatch',
            'wrong_pose': 'first:coverage:pose:mismatch', 'required_cloth': 'first:coverage:cloth:excluded',
            'exhausted_analysis_budget': 'first:triangle_limit', 'wrong_acquisition': 'second:acquisition_mismatch'}
        report['controls'] = {k: v.to_mapping() for k,v in controls.items()}
        report['analysis_controls_passed'] = (all(k in controls and controls[k].status == 'insufficient'
            and reason in controls[k].reasons for k,reason in expected.items())
            and report.get('gap_control', {}).get('status') == 'insufficient'
            and 'gap:0:exceeds_max_gap' in report['gap_control']['reasons'])
    report['replay_hashes'] = {str(p.relative_to(reference)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in reference.rglob('*') if p.is_file()}
    report['assessment'] = ('Sampled pre-material reference geometry only. Required visible coverage, '
        'exact montage endpoints, continuous collision, physical contact and artistic acceptance are separate claims.')
    report['pre_material_qualification'] = ('sampled_reference' if
        report['montage_coverage']['status'] == 'bracketed_samples'
        and len(results) == manifest['expected_attempts']
        and report.get('observed_acquisition_interval', {}).get('status') == 'complete'
        else 'insufficient')
    report['required_visible_qualification'] = ('sampled_reference' if
        report['montage_coverage']['status'] == 'bracketed_samples'
        and len(visible_results) == manifest['expected_attempts']
        and report.get('required_visible_interval', {}).get('status') == 'complete' else 'insufficient')
    save()
    if not report['native_controls_passed'] or not report.get('analysis_controls_passed', False):
        raise ValueError('qualification controls failed or lacked eligible inputs; inspect retained report')
    return dict(output=str(output), pairs=len(results), measured=sum(r.status=='measured' for r in results),
                montage_coverage=report['montage_coverage']['status'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile', type=Path, default=DEFAULT_PROFILE)
    parser.add_argument('--max-pair-tests', type=int, default=ANALYSIS_LIMITS.max_pair_tests)
    parser.add_argument('--max-node-visits', type=int, default=ANALYSIS_LIMITS.max_node_visits)
    args = parser.parse_args()
    limits = replace(ANALYSIS_LIMITS, max_pair_tests=args.max_pair_tests, max_node_visits=args.max_node_visits)
    print(json.dumps(evaluate(args.reference, args.output, args.profile, limits), indent=2))
