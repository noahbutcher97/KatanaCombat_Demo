"""Named actor placement and observed setup checks for registered Katana scenarios."""
from __future__ import annotations

import math
from analyze_capture import CaptureError


def _numbers(values, count, bound):
    if (not isinstance(values, list) or len(values) != count
            or any(type(v) not in (int, float) or abs(v) > bound or not math.isfinite(v) for v in values)):
        raise CaptureError("Placement requires finite, bounded numeric coordinates")
    return list(values)


def resolve_placement(definition, name="default"):
    if name == "default":
        return {}
    placements = definition.get("placements", {})
    selected = placements.get(name) if isinstance(placements, dict) else None
    if not isinstance(selected, dict) or set(selected) != {"Attacker", "Victim"}:
        raise CaptureError("Named placement requires exactly the registered Attacker and Victim roles")
    support_budget(definition)
    result = {}
    for role, pose in selected.items():
        if not isinstance(pose, dict) or set(pose) != {"offset_cm", "yaw_deg"}:
            raise CaptureError("Placement pose requires offset_cm and yaw_deg")
        offset = _numbers(pose["offset_cm"], 3, 3000)
        yaw = _numbers([pose["yaw_deg"]], 1, 180)[0]
        result[role] = dict(offset_cm=offset, yaw_deg=yaw)
    return result


def support_budget(definition):
    support = definition.get("placement_support")
    if support is None:
        return 0
    if (not isinstance(support, dict) or set(support) != {"mode", "max_adjustment_cm"}
            or support["mode"] != "walking_floor"):
        raise CaptureError("Invalid floor preparation contract")
    budget, = _numbers([support["max_adjustment_cm"]], 1, 50)
    if budget <= 0:
        raise CaptureError("Floor preparation needs a positive adjustment budget")
    return budget


def validate_support(evidence, requested, achieved, budget):
    fields = {"status", "requested_location_cm", "capsule_radius_cm", "capsule_half_height_cm",
              "floor_distance_before_cm", "floor_distance_after_cm", "vertical_adjustment_cm",
              "floor_component", "floor_impact_cm"}
    if not isinstance(evidence, dict) or set(evidence) != fields or evidence["status"] != "grounded":
        raise CaptureError("Incomplete or rejected floor preparation")
    before = _numbers(evidence["requested_location_cm"], 3, 1e9)
    impact = _numbers(evidence["floor_impact_cm"], 3, 1e9)
    radius, height, floor_before, floor_after, adjustment = _numbers([evidence[k] for k in (
        "capsule_radius_cm", "capsule_half_height_cm", "floor_distance_before_cm",
        "floor_distance_after_cm", "vertical_adjustment_cm")], 5, 1e6)
    if (math.dist(before, requested) > .01 or radius <= 0 or height < radius or floor_before < 0
            or not 1.89 <= floor_after <= 2.41 or abs(adjustment) > budget
            or not isinstance(evidence["floor_component"], str) or not evidence["floor_component"]
            or abs((achieved[2]-requested[2])-adjustment) > .01
            or abs(floor_before+adjustment-floor_after) > .02):
        raise CaptureError("Floor observation contradicts the requested placement or bounds")
    return [requested[0], requested[1], requested[2]+adjustment]


def validate_placement(scenario, context, metadata=None):
    name = scenario.get("placement", "default")
    if not isinstance(name, str) or context.get("placement", "default") != name:
        raise CaptureError("Requested and recorded placement differ")
    if metadata is not None and metadata.get("placement", "default") != name:
        raise CaptureError("Capture metadata placement differs from the scenario")
    selected = resolve_placement(scenario["definition"], name)
    if not selected:
        if scenario.get("placement_observation"):
            raise CaptureError("Unrequested placement observation")
        return dict(name=name, meaning="Existing scenario setup; no named pose override")
    observation = scenario.get("placement_observation")
    if not isinstance(observation, dict) or set(observation) != {"origin_cm", "roles"}:
        raise CaptureError("Named placement has no complete request-boundary observation")
    origin = _numbers(observation["origin_cm"], 3, 1e9)
    roles = observation["roles"]
    if not isinstance(roles, dict) or set(roles) != set(selected):
        raise CaptureError("Observed placement roles differ from the requested roles")
    budget = support_budget(scenario["definition"])
    for role, expected in selected.items():
        actual = roles[role]
        if not isinstance(actual, dict) or set(actual) != ({"location_cm", "yaw_deg", "support"} if budget else {"location_cm", "yaw_deg"}):
            raise CaptureError("Observed placement pose is incomplete")
        location = _numbers(actual["location_cm"], 3, 1e9)
        yaw = _numbers([actual["yaw_deg"]], 1, 180)[0]
        requested = [a+b for a, b in zip(origin, expected["offset_cm"])]
        target = validate_support(actual["support"], requested, location, budget) if budget else requested
        error = math.dist(location, target)
        angle = abs((yaw-expected["yaw_deg"]+180) % 360-180)
        if error > .01 or angle > .01:
            raise CaptureError("Observed request-boundary transform differs from the named placement")
    return dict(name=name, requested=selected, observed=observation,
                meaning="Named fixture poses observed before public input; this is setup, not gameplay alignment")
