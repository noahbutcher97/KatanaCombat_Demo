"""Evaluate explicit scenario assertions and sampled pose evidence; no artistic feel score."""
from __future__ import annotations

import argparse
import hashlib
import html
import json
import math
from pathlib import Path

import visual_analysis  # Repository bootstrap.
from animation_analysis.errors import EvidenceError as CaptureError
from animation_analysis.integrity import number, read_json, read_lines, vector
from animation_analysis.artifacts import digest, identity, atomic_text, atomic_json
from animation_analysis.metrics import STATUSES, summarize_statuses
from analyze_capture import analyze
from capture_format import bundle_identity, implementation_identity, event_interval, window_rows
from scenario_placement import validate_placement


VERSION = 1
EXPECTED_CHECKS = {"paired_takeover", "input_during_finisher", "paired_cleanup", "victim_outcome",
                   "victim_token_cleanup", "repress_executes_attack", "movement_recovers",
                   "bystanders_remain_active", "scenario_completes"}


def case(name, status, reason, interval=None, **details):
    if status not in STATUSES:
        raise CaptureError("Unknown evaluation status")
    return dict(name=name, status=status, reason=reason, interval_simulation_s=interval, **details)


def summarize(cases):
    return summarize_statuses(c["status"] for c in cases)


def pose_measurement(samples, interval, criteria):
    rows = window_rows(samples, interval)
    if len(rows) < criteria["minimum_samples"]:
        raise CaptureError("Too few pose samples")
    role, point = criteria["required_role"], criteria["required_point"]
    gaps, steps, angular_steps, actors = [], [], [], []
    component = asset = None
    previous = None
    for row in rows:
        now = number(row["simulation_time_s"])
        if row.get("world_paused") or number(row["world_time_dilation"]) != 1:
            raise CaptureError("This transition criterion requires unpaused simulation at unit world dilation")
        matching = [a for a in row["actors"] if a["role"] == role and a.get("valid")]
        if len(matching) != 1:
            raise CaptureError(f"Required participant {role} is missing or duplicated")
        actor = matching[0]
        if number(actor["custom_time_dilation"]) != 1:
            raise CaptureError("This transition criterion requires unit actor dilation")
        if component is None:
            component, asset = actor["mesh_component"], actor["mesh_asset"]
        if (actor["mesh_component"], actor["mesh_asset"]) != (component, asset):
            raise CaptureError("Required mesh changed during the event window")
        age = now - number(actor["pose_simulation_time_s"])
        serial = number(actor["pose_evaluation_serial"])
        if serial <= 0 or age < -1e-6 or age > criteria["max_pose_age_s"]:
            raise CaptureError(f"Stale or unevaluated {role} pose at {now:.6f}")
        if not actor.get("pose_finalized_this_frame") or actor["pose_engine_frame"] != row["engine_frame"]:
            raise CaptureError("Pose finalization is not associated with the sampled engine frame")
        bone = actor.get("points", {}).get(point)
        if bone is None:
            raise CaptureError(f"Required point {role}:{point} is missing")
        position = vector(bone["actor_cm"])
        rotation = bone["component_rotation_xyzw"]
        if len(rotation) != 4:
            raise CaptureError("Expected quaternion")
        rotation = [number(v) for v in rotation]
        norm = math.sqrt(sum(v*v for v in rotation))
        if abs(norm - 1) > .001:
            raise CaptureError("Non-unit bone quaternion")
        if previous:
            old_time, old_serial, old_position, old_rotation = previous
            gap = now - old_time
            if not 0 < gap <= criteria["max_sample_gap_s"]:
                raise CaptureError(f"Pose sampling gap {gap:.6f}s exceeds the criterion's resolution")
            if serial <= old_serial:
                raise CaptureError("Pose evaluation serial did not advance")
            gaps.append(gap)
            steps.append(dict(value=math.dist(position, old_position), time_s=now))
            dot = abs(sum(a*b for a, b in zip(rotation, old_rotation)))
            angular_steps.append(math.degrees(2 * math.acos(min(1, dot))))
        previous = now, serial, position, rotation
        actors.append(actor)
    largest = max(steps, key=lambda item: item["value"])
    return dict(max_actor_relative_step_cm=largest["value"], largest_step_time_s=largest["time_s"],
                max_component_rotation_step_deg=max(angular_steps), max_sample_gap_s=max(gaps),
                sample_count=len(rows), role=role, point=point,
                meaning="Maximum sampled actor-relative point step; an envelope regression detector, not proof of a pose pop")


def visual_eligibility(frames, samples, interval, criteria, resolution=None):
    rows = window_rows(frames, interval)
    role = criteria["required_role"]
    for a, b in zip(rows, rows[1:]):
        if b["simulation_time_s"] - a["simulation_time_s"] > criteria["max_frame_gap_s"]:
            raise CaptureError("Frame cadence cannot resolve the complete event window")
    for frame in rows:
        if resolution and (frame["width"], frame["height"]) != resolution:
            raise CaptureError("Actual viewport resolution differs from the scenario definition")
        if not frame.get("render_resources_ready"):
            raise CaptureError("Render resources were still compiling in the event window")
        sample = samples[frame["sample_index"] - 1]
        lag = number(frame["simulation_time_s"]) - number(sample["simulation_time_s"])
        if not 0 <= lag <= criteria["max_frame_sample_lag_s"] or abs(lag - number(frame["sample_time_lag_s"])) > 1e-6:
            raise CaptureError("Frame/sample timing association is outside the allowed error")
        links = [p for p in frame["pose_links"] if p["role"] == role]
        actors = [a for a in sample["actors"] if a["role"] == role]
        if len(links) != 1 or len(actors) != 1 or links[0]["pose_evaluation_serial"] != actors[0]["pose_evaluation_serial"]:
            raise CaptureError("Rendered frame and sampled pose have different evaluation identities")
        if links[0]["pose_engine_frame"] != frame["engine_frame"]:
            raise CaptureError("Rendered frame did not use a pose finalized in that engine frame")
    return dict(frame_count=len(rows), max_frame_gap_s=max((b["simulation_time_s"] - a["simulation_time_s"] for a, b in zip(rows, rows[1:])), default=0))


def compatibility(scenario):
    # Source/assets are reported as candidate changes, not blindly required to match.
    value = dict(scenario=scenario["scenario"], map_key=scenario["map_key"], variant=scenario["variant"],
                 definition_hash=identity(scenario["definition"]), evaluator_version=VERSION)
    if scenario.get("placement", "default") != "default":
        value["placement"] = scenario["placement"]
    if scenario.get("runtime_experiment", "none") != "none":
        value["runtime_experiment"] = scenario["runtime_experiment"]
        value["runtime_overrides_identity"] = identity(scenario.get("runtime_asset_overrides", []))
    return value


def tuning_numbers(value, count):
    if not isinstance(value, list) or len(value) != count or any(type(v) not in (int, float) or not math.isfinite(v) for v in value):
        raise CaptureError("Expected finite numeric tuning values")
    return value


FACING_POLICIES = ("face-partner", "face-away-from-partner", "match-partner-heading")


def validate_primary_sync_settings(settings):
    if not isinstance(settings, dict) or not settings or set(settings) - {"time_s", "nudge_enabled"}:
        raise CaptureError("Primary sync requires a time and/or nudge setting")
    if "time_s" in settings:
        time = tuning_numbers([settings["time_s"]], 1)[0]
        if not 0 <= time <= 5:
            raise CaptureError("Primary sync time must be within 0..5 seconds")
    if "nudge_enabled" in settings and type(settings["nudge_enabled"]) is not bool:
        raise CaptureError("Primary sync nudge setting must be boolean")
    return settings


def validate_primary_sync_override(settings, row):
    before, after = row.get("before"), row.get("after")
    for state in (before, after):
        if not isinstance(state, dict) or set(state) != {"time_s", "end_s", "nudge_enabled"}:
            raise CaptureError("Incomplete primary sync override")
        start, end = tuning_numbers([state["time_s"], state["end_s"]], 2)
        if not -.001 <= start < end <= 5 or type(state["nudge_enabled"]) is not bool:
            raise CaptureError("Invalid primary sync override")
    if (abs(after["time_s"] - settings.get("time_s", before["time_s"])) > 1e-5
            or abs((after["end_s"]-after["time_s"]) - (before["end_s"]-before["time_s"])) > 1e-5
            or after["nudge_enabled"] != settings.get("nudge_enabled", before["nudge_enabled"])):
        raise CaptureError("Primary sync differs from the request or changes an unrequested field")


def validate_entry_settings(settings):
    presentation = dict(moving_role="victim", movement_animation="", movement_slot="DefaultSlot",
                        movement_play_rate=1., movement_blend_in_s=.1, movement_blend_out_s=.1)
    scalars = ("victim_yaw_deg", "duration_s", "translation_speed_cm_s", "travel_budget_cm",
               "turn_rate_deg_s", "turn_budget_deg", "position_tolerance_cm", "yaw_tolerance_deg")
    required = {"enabled", "victim_offset_cm", *scalars}
    if (not isinstance(settings, dict) or set(settings) not in (required, required | set(presentation),
            required | set(presentation) | {"movement_start_time_s"})
            or type(settings["enabled"]) is not bool):
        raise CaptureError("Entry configuration requires the complete typed pose and motion limits")
    tuning_numbers(settings["victim_offset_cm"], 3)
    values = tuning_numbers([settings[k] for k in scalars], len(scalars))
    if (not -180 <= values[0] <= 180 or values[1] <= 0 or any(v < 0 or v > 3.402823e38 for v in values[1:])
            or values[-1] > 180):
        raise CaptureError("Entry limits or upright yaw are outside their supported ranges")
    result = presentation | {"movement_start_time_s": 0.} | settings
    if (result["moving_role"] not in ("victim", "initiator")
            or not isinstance(result["movement_animation"], str)
            or (result["movement_animation"] and not result["movement_animation"].startswith("/Game/"))
            or not isinstance(result["movement_slot"], str) or not result["movement_slot"]
            or result["movement_slot"].lower() == "none"):
        raise CaptureError("Invalid entry movement role or animation identity")
    rate, blend_in, blend_out = tuning_numbers([result[k] for k in
        ("movement_play_rate", "movement_blend_in_s", "movement_blend_out_s")], 3)
    if not (.01 <= rate <= 10 and 0 <= blend_in <= 1 and 0 <= blend_out <= 1):
        raise CaptureError("Invalid entry presentation timing")
    start, = tuning_numbers([result["movement_start_time_s"]], 1)
    if not 0 <= start <= 3.402823e38 or (start and not result["movement_animation"]):
        raise CaptureError("Invalid entry source start time")
    return result


def validate_entry_override(settings, row):
    settings = validate_entry_settings(settings)
    before = validate_entry_settings(row.get("before"))
    after = validate_entry_settings(row.get("after"))
    if row.get("notify_class") != "PairedAnimationData" or after["enabled"] != settings["enabled"]:
        raise CaptureError("Entry override has inconsistent ownership or enable state")
    for key, expected in settings.items():
        if key == "enabled":
            continue
        actual = after[key]
        if isinstance(expected, str):
            if actual != expected:
                raise CaptureError("Entry override differs from the requested presentation")
            continue
        pairs = zip(actual, expected) if isinstance(expected, list) else [(actual, expected)]
        if any(abs((a-b+180) % 360-180 if key == "victim_yaw_deg" else a-b) > 1e-4 for a, b in pairs):
            raise CaptureError("Entry override differs from the requested pose or limits")


def validate_warp_tuning_settings(settings):
    required = {"victim_window_s", "victim_offset_cm"}
    if not isinstance(settings, dict) or not required <= set(settings) or set(settings) - required - {"victim_facing_policy", "primary_sync", "entry"}:
        raise CaptureError("Warp tuning requires a victim window and offset, with an optional facing policy")
    if "primary_sync" in settings:
        validate_primary_sync_settings(settings["primary_sync"])
    if "entry" in settings and not validate_entry_settings(settings["entry"])["enabled"]:
        raise CaptureError("Requested entry preparation must be enabled")
    if "victim_facing_policy" in settings and settings["victim_facing_policy"] not in FACING_POLICIES:
        raise CaptureError("Unsupported victim facing policy")
    start, end = tuning_numbers(settings["victim_window_s"], 2)
    x, y, z = tuning_numbers(settings["victim_offset_cm"], 3)
    if not (0 <= start and end <= 5 and end - start >= .02 and abs(x) <= 200 and abs(y) <= 200 and z == 0):
        raise CaptureError("Warp tuning is outside the supported window/placement bounds")
    return settings


def validate_warp_tuning_overrides(overrides, context):
    settings = validate_warp_tuning_settings(context.get("warp_tuning"))
    facing_requested = "victim_facing_policy" in settings
    sync_requested = "primary_sync" in settings
    entry_requested = "entry" in settings
    if len(overrides) != 5 + int(facing_requested) + 2 * int(sync_requested) + int(entry_requested):
        raise CaptureError("Warp tuning must describe every requested override")
    seen, assets, notify_ids = set(), {}, set()
    expected = {("Attacker", "bDisableMovement"), ("Victim", "bDisableMovement"),
                ("Attacker", "RootMotionModifier.bWarpTranslation"), ("Victim", "TriggerWindowSeconds"),
                ("Victim", "VictimWarpConfig.RelativeOffset")}
    if facing_requested:
        expected.add(("Victim", "VictimWarpConfig.FacingPolicy"))
    if sync_requested:
        expected.update((role, "PrimarySyncSettings") for role in ("Attacker", "Victim"))
    if entry_requested:
        expected.add(("Victim", "Entry"))
    for row in overrides:
        role, prop = row.get("role"), row.get("property")
        key = (role, prop)
        offset = prop == "VictimWarpConfig.RelativeOffset"
        facing = prop == "VictimWarpConfig.FacingPolicy"
        entry = prop == "Entry"
        pair_property = offset or facing or entry
        asset, index = row.get("asset"), row.get("notify_index")
        if (key not in expected or key in seen or not isinstance(asset, str) or not asset.startswith("/Game/")
                or type(index) is not int or (index != -1 if pair_property else index < 0)):
            raise CaptureError("Invalid or duplicate warp tuning override")
        notify_id = (role, asset, index)
        if not pair_property and notify_id in notify_ids:
            raise CaptureError("Duplicate tuning notify identity")
        if not pair_property:
            notify_ids.add(notify_id)
        seen.add(key)
        asset_key = "pair" if pair_property else role
        if asset_key in assets and assets[asset_key] != asset:
            raise CaptureError("Warp tuning role has inconsistent asset identity")
        assets[asset_key] = asset
        if entry:
            validate_entry_override(settings["entry"], row)
        elif prop == "PrimarySyncSettings":
            if row.get("notify_class") != "AnimNotifyState_PairedAnimationSync":
                raise CaptureError("Primary sync override must identify a paired sync notify")
            validate_primary_sync_override(settings["primary_sync"], row)
        elif facing:
            if (row.get("notify_class") != "PairedAnimationData" or row.get("before") not in FACING_POLICIES
                    or row.get("after") != settings["victim_facing_policy"]):
                raise CaptureError("Facing override differs from the requested policy")
        elif prop in ("TriggerWindowSeconds", "VictimWarpConfig.RelativeOffset"):
            values = settings["victim_offset_cm" if offset else "victim_window_s"]
            before = tuning_numbers(row.get("before"), len(values))
            after = tuning_numbers(row.get("after"), len(values))
            if (row.get("notify_class") != ("PairedAnimationData" if offset else "AnimNotifyState_MotionWarping")
                    or any(abs(a-b) > 1e-5 for a, b in zip(after, values))
                    or (not offset and not 0 <= before[0] < before[1] <= 5)):
                raise CaptureError("Warp tuning differs from the requested settings")
        elif (row.get("notify_class") != ("AnimNotifyState_PairedAnimationCollision" if prop == "bDisableMovement" else "AnimNotifyState_MotionWarping")
              or row.get("before") is not True or row.get("after") is not False):
            raise CaptureError("Unsupported warp tuning boolean override")
    if seen != expected:
        raise CaptureError("Missing required warp tuning override")


def validate_runtime_experiment(scenario, context, metadata=None, override_bytes=None):
    experiment = scenario.get("runtime_experiment", "none")
    overrides = scenario.get("runtime_asset_overrides", [])
    warp_role = {"attacker-source-translation": "Attacker", "victim-source-translation": "Victim", "victim-source-rotation": "Victim"}.get(experiment)
    warp_property = "RootMotionModifier.bWarpRotation" if experiment == "victim-source-rotation" else "RootMotionModifier.bWarpTranslation"
    if experiment not in ("none", "permit-root-motion", "attacker-source-translation", "victim-source-translation", "victim-source-rotation", "paired-warp-tuning") or context.get("runtime_experiment", "none") != experiment:
        raise CaptureError("Runtime experiment identity is inconsistent")
    if (not isinstance(overrides, list) or any(not isinstance(row, dict) for row in overrides)
            or (experiment == "none") != (len(overrides) == 0)):
        raise CaptureError("Runtime experiment lacks its explicit asset overrides")
    if experiment == "paired-warp-tuning":
        validate_warp_tuning_overrides(overrides, context)
    elif experiment != "none":
        if {row.get("role") for row in overrides} != {"Attacker", "Victim"}:
            raise CaptureError("Movement experiment must identify both participants")
        movement_roles, warp_roles, notify_ids = set(), [], set()
        for row in overrides:
            movement = row.get("property") == "bDisableMovement" and row.get("notify_class") == "AnimNotifyState_PairedAnimationCollision"
            warp_override = (row.get("property") == warp_property
                           and row.get("notify_class") == "AnimNotifyState_MotionWarping"
                           and warp_role is not None and row.get("role") == warp_role)
            if (not (movement or warp_override) or row.get("before") is not True or row.get("after") is not False
                    or not isinstance(row.get("asset"), str) or not row["asset"].startswith("/Game/")
                    or type(row.get("notify_index")) is not int or row["notify_index"] < 0):
                raise CaptureError("Unsupported runtime notify override")
            notify_id = (row["role"], row["asset"], row["notify_index"])
            if notify_id in notify_ids:
                raise CaptureError("Duplicate runtime notify override")
            notify_ids.add(notify_id)
            if movement:
                movement_roles.add(row["role"])
            else:
                warp_roles.append(row["role"])
        if movement_roles != {"Attacker", "Victim"} or warp_roles != ([warp_role] if warp_role else []):
            raise CaptureError("Runtime experiment does not describe its required movement and warp overrides")
    if metadata is not None:
        if "runtime_asset_overrides_sha1" in metadata:
            if ("runtime_asset_overrides" in metadata or override_bytes is None
                    or hashlib.sha1(override_bytes).hexdigest() != metadata["runtime_asset_overrides_sha1"]):
                raise CaptureError("Runtime override sidecar is missing, ambiguous or does not match capture metadata")
            try:
                recorded = json.loads(override_bytes)
            except (ValueError, UnicodeError) as error:
                raise CaptureError("Runtime override sidecar is not valid JSON") from error
        else:
            recorded = json.loads(metadata.get("runtime_asset_overrides", "[]"))
        if metadata.get("runtime_experiment", "none") != experiment or recorded != overrides:
            raise CaptureError("Capture metadata disagrees with the runtime experiment")
    return dict(name=experiment, asset_overrides=overrides, identity=identity(overrides))


def evaluate(root, reference=None):
    root = Path(root)
    scenario = read_json(root / "scenario.json")
    if scenario["schema_version"] != 1 or scenario["scenario"] not in ("FinisherRecovery", "HoldReleaseRecovery"):
        raise CaptureError("No evaluator registered for this scenario/schema")
    is_hold = scenario["scenario"] == "HoldReleaseRecovery"
    expected_checks = {"authored_hold_available", "real_hold_started", "hold_movement_suppressed",
                       "competing_input_preserves_hold", "authored_follow_up_starts", "hold_cleanup",
                       "movement_recovers", "repress_executes_attack", "scenario_completes"} if is_hold else EXPECTED_CHECKS
    if scenario.get("runtime_experiment", "none") != "none":
        expected_checks = expected_checks | {"experiment_ready", "experiment_restored"}
    request_event = "hold_requested" if is_hold else "finisher_requested"
    checks = scenario["checks"]
    if len({c["name"] for c in checks}) != len(checks) or any(c["status"] not in ("pass", "fail") for c in checks):
        raise CaptureError("Malformed or duplicate scenario checks")
    cases = [case("gameplay." + c["name"], c["status"], c["reason"], [c["simulation_time_s"]]*2) for c in checks]
    for missing in sorted(expected_checks - {c["name"] for c in checks}):
        cases.append(case("gameplay." + missing, "inconclusive", "Scenario did not reach this assertion"))
    result = dict(evaluation_schema_version=VERSION, run_id=scenario["run_id"],
                  compatibility=compatibility(scenario), evaluator_identity=implementation_identity(__file__),
                  capture_mode=scenario["capture_mode"], control_offset_cm=scenario["control_offset_cm"],
                  cases=cases, measurements={}, timing={},
                  unmeasured=["Perceived responsiveness", "contact-region correctness", "foot support/slip",
                              "occlusion/readability", "audio", "haptics", "artistic animation quality"])
    # These observations also exist when recording is disabled, allowing a fair
    # comparison of scenario behavior across the three capture modes.
    timing_events = (("request_to_hold_observation_s", "hold_requested", "hold_started"),
                     ("release_to_follow_up_observation_s", "hold_released", "follow_up_observed"),
                     ("release_to_control_restoration_s", "hold_released", "ownership_released")) if is_hold else (("request_to_paired_observation_s", "finisher_requested", "finisher_started"),
                               ("paired_observation_duration_s", "finisher_started", "ownership_released"),
                               ("release_to_repress_s", "ownership_released", "recovery_repress"))
    for label, first, last in timing_events:
        try:
            start, end = event_interval(scenario["events"], first, last)
            result["timing"][label] = dict(value=end-start, interval_simulation_s=[start, end], clock="simulation", source="scenario observation, bounded by fixture update cadence")
        except CaptureError:
            result["timing"][label] = None
    try:
        context, assets = read_json(root / "run-context.json"), read_json(root / "asset-identity.json")
        if context["run_id"] != scenario["run_id"] or context["source_identity"] != identity(context["source"]["files"]) or assets["identity"] != identity(assets["files"]):
            raise CaptureError("Run/source/asset identity is inconsistent")
        if not context["editor_binaries"] or not assets["files"]:
            raise CaptureError("No editor binary or asset identity")
        metadata = read_json(root / "session.json")["metadata"] if scenario["capture_mode"] != "disabled" else None
        override_path = root / "runtime-overrides.json"
        override_bytes = override_path.read_bytes() if override_path.is_file() else None
        result["runtime_experiment"] = validate_runtime_experiment(scenario, context, metadata, override_bytes)
        result["placement"] = validate_placement(scenario, context, metadata)
        execution_files = {name: sha for name, sha in context["source"]["files"].items()
                           if (not name.startswith(("Tools/CombatCapture/", "Tools/AnimationAnalysis/", "Dependencies/AnimationAnalysis/Python/"))
                               or name.startswith("Tools/CombatCapture/scenarios/"))}
        result["provenance"] = dict(source_identity=context["source_identity"], execution_source_identity=identity(execution_files), assets_identity=assets["identity"],
                                    capture_evaluator_identity=context["evaluator_identity"], declared_changes=context["declared_changes"])
        cases.append(case("run.identity", "pass", "Run, source, scenario, engine/binary and project asset identities recorded"))
    except (CaptureError, KeyError, TypeError) as error:
        cases.append(case("run.identity", "inconclusive", str(error)))
    if scenario["capture_mode"] == "disabled":
        cases.append(case("pose.transition_envelope", "not_run", "Capture disabled for overhead control"))
        cases.append(case("visual.window_evidence", "not_run", "Capture disabled for overhead control"))
        result["status"] = summarize(cases)
        return result
    manifest = read_json(root / "session.json")
    analysis, frames, markers = analyze(root)
    result["recording_integrity"] = analysis["data_integrity"]
    cases.append(case("recording.integrity", "pass" if analysis["data_integrity"] == "valid" else "fail",
                      "; ".join(analysis["issues"]) or "Bundle counts, clocks, telemetry and PNG validation passed"))
    samples = read_lines(root / "samples.jsonl")
    criteria = scenario["definition"]["criteria"]
    interval = None
    try:
        interval = event_interval(scenario["events"], request_event, "ownership_released", .05, .15)
        if manifest["schema_version"] != 2:
            raise CaptureError("Schema 1 has no explicit pose evaluation identity")
        if analysis["data_integrity"] != "valid" or manifest["stop_reason"] != "scenario_finished":
            raise CaptureError("Incomplete capture or early recording limit")
        if manifest["metadata"]["run_id"] != scenario["run_id"]:
            raise CaptureError("Scenario and capture run identities differ")
        measurement = pose_measurement(samples, interval, criteria)
        result["measurements"]["transition"] = measurement
        cases.append(case("pose.window_evidence", "pass", "Fresh evaluated poses bracket the complete window within declared sampling limits", interval, coverage=measurement))
        if reference is None:
            cases.append(case("pose.transition_envelope", "not_run", "No explicitly selected calibrated reference", interval))
        else:
            if reference["compatibility"] != result["compatibility"] or reference["evaluator_identity"] != result["evaluator_identity"]:
                raise CaptureError("Reference scenario/criteria/evaluator identity is incompatible")
            if "provenance" not in result:
                raise CaptureError("Reference comparison requires complete run provenance")
            if "provenance" in reference:
                changed = {name: reference["provenance"][name] != result["provenance"][name] for name in ("execution_source_identity", "assets_identity")}
                result["reference_changes"] = dict(changed, declared_changes=result["provenance"]["declared_changes"])
                if any(changed.values()) and not result["provenance"]["declared_changes"]:
                    raise CaptureError("Source/assets differ from the reference without a declared candidate change")
            limit = number(reference["max_actor_relative_step_cm"])
            value = measurement["max_actor_relative_step_cm"]
            nearest = min(frames, key=lambda f: abs(f["simulation_time_s"] - measurement["largest_step_time_s"]), default=None)
            cases.append(case("pose.transition_envelope", "pass" if value <= limit else "fail",
                              "Sampled displacement compared with the selected reference envelope", interval,
                              observed_cm=value, threshold_cm=limit, reference_basis=reference["acceptance_basis"],
                              reference_id=identity(reference), frame=nearest["file"] if nearest else None))
    except (CaptureError, KeyError, TypeError, ValueError) as error:
        cases.append(case("pose.transition_envelope", "inconclusive", str(error), interval))
    if not frames:
        cases.append(case("visual.window_evidence", "not_run", "No rendered frames; headless motion cannot establish visual evidence", interval))
    else:
        try:
            if interval is None or analysis["data_integrity"] != "valid":
                raise CaptureError("No intact event window")
            definition = scenario["definition"]
            resolution = (definition["viewport_width"], definition["viewport_height"]) if "viewport_width" in definition else None
            coverage = visual_eligibility(frames, samples, interval, criteria, resolution)
            cases.append(case("visual.window_evidence", "pass", "Frames cover the event window with matching evaluated poses; visibility still requires review", interval, coverage=coverage))
        except (CaptureError, KeyError, TypeError, ValueError) as error:
            cases.append(case("visual.window_evidence", "inconclusive", str(error), interval))
    result["input_to_action"] = analysis["input_timing"]
    result["sampled_event_observations"] = {}
    try:
        requested = event_interval(scenario["events"], request_event, "ownership_released")[0]
        previous = None
        for sample in samples:
            actors = {a["role"]: a for a in sample["actors"]}
            if sample["simulation_time_s"] < requested:
                previous = sample
                continue
            if not previous:
                continue
            old = {a["role"]: a for a in previous["actors"]}
            attacker, victim = actors.get("Attacker", {}), actors.get("Victim", {})
            old_attacker, old_victim = old.get("Attacker", {}), old.get("Victim", {})
            observations = {
                "first_evaluated_montage_contribution": attacker.get("pose_finalized_this_frame") and any(m["weight"] > 0 for m in attacker.get("montages", [])) and not any(m["weight"] > 0 for m in old_attacker.get("montages", [])),
                "victim_health_decrease": victim.get("health", math.inf) < old_victim.get("health", -math.inf),
                "movement_suppression_released": old_attacker.get("movement_input_suppressed") and attacker.get("movement_input_suppressed") is False}
            for name, observed in observations.items():
                if observed and name not in result["sampled_event_observations"]:
                    lower, upper = previous["simulation_time_s"], sample["simulation_time_s"]
                    result["sampled_event_observations"][name] = dict(interval_simulation_s=[lower, upper],
                        observation_time_s=upper, uncertainty_s=upper-lower,
                        status="pass" if upper-lower <= criteria["max_sample_gap_s"] else "inconclusive",
                        meaning="Sampled state transition; montage contribution is not a pixel response and health change is not contact-region validation")
            previous = sample
    except (CaptureError, KeyError, TypeError):
        pass
    result["status"] = summarize(cases)
    return result


def evaluate_and_write(root, reference=None):
    root = Path(root)
    # Replace prior success before opening inputs. A crash leaves an explicit pending state.
    atomic_json(root / "evaluation.json", dict(status="inconclusive", reason="Evaluation in progress", evaluator_identity=implementation_identity(__file__)))
    atomic_text(root / "evaluation.html", "<!doctype html><meta charset=utf-8><p>Evaluation in progress. No current result.</p>")
    try:
        if isinstance(reference, Path):
            reference = read_json(reference)
        before = bundle_identity(root)
        result = evaluate(root, reference)
        if bundle_identity(root) != before:
            raise CaptureError("Capture changed while evaluation was running")
        result["bundle_identity"] = before
        rows = ""
        for c in result["cases"]:
            detail = html.escape(c["reason"])
            if c.get("interval_simulation_s"):
                detail += "<br><small>Simulation interval: " + "â€“".join(f"{t:.3f}" for t in c["interval_simulation_s"]) + " s</small>"
            if "threshold_cm" in c:
                detail += f"<br>Observed {c['observed_cm']:.3f} cm; envelope {c['threshold_cm']:.3f} cm."
            if c.get("frame"):
                detail += '<br><a href="' + html.escape(c["frame"], quote=True) + '">Nearest captured frame</a>'
            rows += "<tr><td>" + html.escape(c["name"]) + "</td><td>" + c["status"] + "</td><td>" + detail + "</td></tr>"
        page = '<!doctype html><meta charset=utf-8><title>Combat scenario evaluation</title><style>body{font:16px system-ui;max-width:1100px;margin:40px auto;padding:20px}td{padding:10px;border-bottom:1px solid #ccc}pre{white-space:pre-wrap}</style>'
        primary_link = '<a href="scenario.json">Scenario assertions and timing</a>' if result["capture_mode"] == "disabled" else '<a href="report.html">Frame timeline and measurements</a>'
        page += '<h1>Combat scenario evaluation: ' + result["status"] + '</h1><p>A pass applies to executed cases. Not-run and inconclusive cases establish no quality verdict. Artistic quality, contact, foot support, audio and haptics remain unmeasured.</p><p>' + primary_link + ' Â· <a href="evaluation.json">Evaluation JSON</a></p><table>' + rows + '</table><pre>' + html.escape(json.dumps(result, indent=2)) + '</pre>'
        atomic_text(root / "evaluation.html", page)
        atomic_json(root / "evaluation.json", result)
        return result
    except (CaptureError, KeyError, TypeError, ValueError, OSError) as error:
        failure = dict(status="inconclusive", reason=str(error), evaluator_identity=implementation_identity(__file__))
        atomic_text(root / "evaluation.html", "<!doctype html><meta charset=utf-8><p>Evaluation rejected: " + html.escape(str(error)) + "</p>")
        atomic_json(root / "evaluation.json", failure)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--reference", type=Path)
    args = parser.parse_args()
    try:
        result = evaluate_and_write(args.capture, args.reference)
        print(f"{result['status']}: {args.capture / 'evaluation.html'}")
        return 0 if result["status"] == "pass" else 1
    except (CaptureError, OSError, ValueError, KeyError, TypeError) as error:
        print(f"Evaluation rejected: {error}")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
