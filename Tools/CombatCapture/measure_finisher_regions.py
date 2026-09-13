"""Qualify retained Katana reference regions using the pinned shared mesh APIs.

No observation is retimed, interpolated or repaired. The separate same-observation
controls exercise analysis without claiming that two native streams are pairable.
"""
from __future__ import annotations

import argparse
from dataclasses import replace
import hashlib
import json
from pathlib import Path
import statistics
import time

import visual_analysis  # Resolves and verifies the consumer's exact dependency.
from animation_analysis import (MeshAnalysisLimits, MeshPairSample, MeshRegion,
                                MeshSelection, measure_mesh_pair, summarize_mesh_interval)
from animation_analysis.mesh_records import MeshRecordLimits, MeshRequirement, assess_mesh_coverage
from animation_analysis.mesh_replay import read_mesh_observation
from animation_analysis.temporal import TimeInterval

RECORD_LIMITS = MeshRecordLimits(250_000, 1_500_000, 64, 32*1024**2, 65_536, 33*1024**2)
ANALYSIS_LIMITS = MeshAnalysisLimits(50_000, 100_000, 200_000, 256)
EXCLUSIONS = ('morph', 'cloth', 'mesh_deformer', 'material_displacement', 'raster_visibility')


def requirement(observation):
    feature = 'rigid' if observation.producer_id == 'unreal-rigid-reference-v1' else 'bone'
    return MeshRequirement(observation.component_id, observation.component_generation,
        observation.topology.identity, observation.configuration_id, observation.pose,
        'centimetres', 'unreal-left-handed-z-up', (feature, 'pose_ordering'),
        (feature, 'pose_ordering', *EXCLUSIONS), EXCLUSIONS, (observation.producer_id,))


def world_positions(observation):
    if observation.vector_convention != 'row':
        raise ValueError('Katana reference selection requires the native row-vector convention')
    m = observation.component_to_world
    return [tuple(sum(point[k]*m[k*4+j] for k in range(3))+m[12+j] for j in range(3))
            for point in observation.positions()]


def select_region(observation, center, half_extents):
    """Explicit spatial authoring, independent of section labels and skin weights.

    Retain global triangle IDs whose centroids lie in the declared first-pose box.
    This is a geometric neighborhood, not certified anatomical segmentation.
    """
    points, indices = world_positions(observation), list(observation.topology.indices())
    selected = []
    for offset in range(0, len(indices), 3):
        triangle = [points[index] for index in indices[offset:offset+3]]
        centroid = [sum(p[axis] for p in triangle)/3 for axis in range(3)]
        if all(abs(centroid[axis]-center[axis]) <= half_extents[axis] for axis in range(3)):
            selected.append(offset//3)
    return selected


def evaluate(reference, output):
    reference, output = Path(reference).resolve(), Path(output).resolve()
    if output.exists():
        raise ValueError('Use a new output directory; qualification evidence is exclusive')
    output.mkdir(parents=True)
    manifest = json.loads((reference/'reference.json').read_text(encoding='utf-8-sig'))
    completions = []
    for row in manifest['samples']:
        if not all(row[role]['acquired'] and not row[role]['export_error'] for role in ('victim','katana')):
            raise ValueError('Required reference acquisition/export missing; retain native rejection')
        pair = tuple(read_mesh_observation(reference, row[role]['bundle'], limits=RECORD_LIMITS)
                     for role in ('victim','katana'))
        for role, completion in zip(('victim', 'katana'), pair):
            observation = completion.observation
            inventory = manifest['victim_inventory' if role == 'victim' else 'weapon_inventory']
            if (completion.status != 'completed' or observation is None
                    or completion.request.request_id != row[role]['bundle']
                    or observation.component_id != role or observation.pose.subject_id != role
                    or observation.topology.asset_id != inventory['asset']
                    or observation.topology.lod != 0):
                raise ValueError('Reference record differs from its declared participant, asset, LOD or request')
        completions.append(pair)
    if len(completions) != 5:
        raise ValueError('Five explicitly requested reference pairs required')
    first_body, first_weapon = (c.observation for c in completions[0])
    # Fixed before numerical analysis; follows the predeclared geometric selection.
    center = manifest['samples'][0]['head_world_xyz_cm']
    extents = [12, 12, 15]
    body_ids = select_region(first_body, center, extents)
    weapon_ids = list(range(len(first_weapon.topology.index_data)//12))
    body_region = MeshRegion('victim-head-neck-box', first_body.topology.identity, body_ids, ANALYSIS_LIMITS)
    weapon_region = MeshRegion('equipped-katana-complete-surface', first_weapon.topology.identity, weapon_ids, ANALYSIS_LIMITS)
    regions = dict(selection='first-pose triangle centroid inside world-axis box', center_cm=center,
        half_extents_cm=extents, anatomy='geometric neighborhood; no anatomical segmentation claim',
        victim=dict(topology=body_region.topology_id, identity=body_region.identity, triangle_ids=body_ids),
        katana=dict(topology=weapon_region.topology_id, identity=weapon_region.identity, triangle_ids=weapon_ids))
    (output/'regions.json').write_text(json.dumps(regions, indent=2)+'\n')
    results, eligibility, timings = [], [], []
    for index, pair in enumerate(completions):
        body, weapon = (c.observation for c in pair)
        if body is None or weapon is None:
            raise ValueError('Required mesh observation unavailable')
        selections = [MeshSelection(body, body_region, requirement(body)),
                      MeshSelection(weapon, weapon_region, requirement(weapon))]
        eligibility.append([dict(eligible=assess_mesh_coverage(s.observation,s.requirement).eligible,
                                 reasons=assess_mesh_coverage(s.observation,s.requirement).reasons) for s in selections])
        start = time.perf_counter()
        result = measure_mesh_pair(MeshPairSample(f'reference-{index}', body.acquired, *selections),
                                   tolerance=.5, limits=ANALYSIS_LIMITS)
        timings.append((time.perf_counter()-start)*1000)
        results.append(result)
    interval = TimeInterval(results[0].acquired, results[-1].acquired)
    summary = summarize_mesh_interval(results, interval, max_gap_seconds=.05, max_samples=20)

    # These retained controls use native geometry with its original acquisition.
    # A same-region self intersection is an analytic identity, not finisher contact.
    triangle = MeshRegion('katana-triangle-control', first_weapon.topology.identity, [0], ANALYSIS_LIMITS)
    selected = MeshSelection(first_weapon, triangle, requirement(first_weapon))
    sample = MeshPairSample('self-surface-control', first_weapon.acquired, selected, selected)
    control_results = {'self_surface': measure_mesh_pair(sample, tolerance=.5, limits=ANALYSIS_LIMITS)}
    wrong_region = replace(triangle, topology_id='0'*64)
    control_results['wrong_topology'] = measure_mesh_pair(replace(sample, first=replace(selected, region=wrong_region)), tolerance=.5, limits=ANALYSIS_LIMITS)
    wrong_generation = replace(selected.requirement, component_generation=selected.requirement.component_generation+1)
    control_results['wrong_generation'] = measure_mesh_pair(replace(sample, first=replace(selected, requirement=wrong_generation)), tolerance=.5, limits=ANALYSIS_LIMITS)
    cloth = replace(selected.requirement, required_features=(*selected.requirement.required_features, 'cloth'),
                    allowed_exclusions=tuple(f for f in EXCLUSIONS if f != 'cloth'))
    control_results['required_cloth'] = measure_mesh_pair(replace(sample, first=replace(selected, requirement=cloth)), tolerance=.5, limits=ANALYSIS_LIMITS)
    # Complete weapon region exceeds this control's one-triangle analysis budget.
    whole = MeshSelection(first_weapon, weapon_region, requirement(first_weapon))
    tiny = MeshAnalysisLimits(1, 1, 1, 256)
    control_results['exhausted_analysis_budget'] = measure_mesh_pair(replace(sample, first=whole, second=whole), tolerance=.5, limits=tiny)

    self_series = []
    for index, pair in enumerate(completions):
        obs = pair[1].observation
        selection = MeshSelection(obs, triangle, requirement(obs))
        self_series.append(measure_mesh_pair(MeshPairSample(f'self-{index}',obs.acquired,selection,selection), tolerance=.5,limits=ANALYSIS_LIMITS))
    actual_interval = TimeInterval(self_series[0].acquired, self_series[-1].acquired)
    # Use the actual positive gap with a deliberately smaller bound; no retiming.
    actual_gap = self_series[-1].acquired.seconds-self_series[0].acquired.seconds
    gap_summary = summarize_mesh_interval([self_series[0], self_series[-1]], actual_interval,
                                          max_gap_seconds=actual_gap/2, max_samples=20)
    controls_ok = (control_results['self_surface'].status == 'measured'
        and control_results['self_surface'].surface_intersection is True
        and control_results['self_surface'].minimum_distance == 0
        and all(r.status == 'insufficient' for k,r in control_results.items() if k != 'self_surface')
        and 'first:region_topology_mismatch' in control_results['wrong_topology'].reasons
        and 'first:coverage:component_generation:mismatch' in control_results['wrong_generation'].reasons
        and 'first:coverage:cloth:excluded' in control_results['required_cloth'].reasons
        and 'first:triangle_limit' in control_results['exhausted_analysis_budget'].reasons
        and gap_summary.status == 'insufficient')
    report = dict(format='katana_finisher_mesh_qualification', schema_version=1,
        reference=str(reference), requested_pairs=5, native_observations=10,
        individual_eligibility=eligibility, results=[r.to_mapping() for r in results],
        interval=summary.to_mapping(), controls={k:r.to_mapping() for k,r in control_results.items()},
        gap_control=gap_summary.to_mapping(), controls_passed=controls_ok,
        source_times_s=[r['source_time_s'] for r in manifest['samples']],
        acquisition_skew_ms=[(p[1].observation.acquired.seconds-p[0].observation.acquired.seconds)*1000 for p in completions],
        acquisition_gaps_ms={role:[(completions[i][side].observation.acquired.seconds-completions[i-1][side].observation.acquired.seconds)*1000
                                  for i in range(1,len(completions))] for side,role in enumerate(('victim','katana'))},
        completion_latency_ms={role:[(p[side].completed.seconds-p[side].observation.acquired.seconds)*1000 for p in completions]
                               for side,role in enumerate(('victim','katana'))},
        offline_pair_wall_ms=timings, offline_pair_median_ms=statistics.median(timings),
        replay_hashes={str(p.relative_to(reference)):hashlib.sha256(p.read_bytes()).hexdigest() for p in reference.rglob('*') if p.is_file()},
        assessment='Live coverage is evaluated separately. Native two-stream timestamps are preserved; no retiming or live pass is inferred.')
    (output/'measurements.json').write_text(json.dumps(report, indent=2)+'\n')
    if not controls_ok:
        raise ValueError('One or more retained failure controls did not meet their expected contract')
    return dict(output=str(output), observed_pairs=sum(r.status=='measured' for r in results),
                interval=summary.status, controls_passed=controls_ok, region_triangles=[len(body_ids),len(weapon_ids)])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(evaluate(args.reference, args.output), indent=2))
