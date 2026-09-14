"""Read-only UE Python inventory. KATANA_ENTRY_INVENTORY names an input/output folder.

inventory-input.json supplies reference_mesh and explicit animations. Run through
UnrealEditor-Cmd -run=pythonscript; no packages are saved. Raw pose evidence does
not establish the effective AnimBlueprint presentation.
"""
import json
import math
import os
from pathlib import Path
import traceback
import unreal

root = Path(os.environ["KATANA_ENTRY_INVENTORY"])
report = {"status": "incomplete", "animations": [], "evidence": "authored_raw_pose"}
try:
    request = json.loads((root / "inventory-input.json").read_text())
    mesh = unreal.load_asset(request["reference_mesh"])
    skeleton = mesh.get_editor_property("skeleton")
    options = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.RAW,
        extract_root_motion=False, incorporate_root_motion_into_pose=True)
    for path in request["animations"]:
        sequence = unreal.load_asset(path)
        assert isinstance(sequence, unreal.AnimSequence), path
        duration = sequence.get_play_length()
        row = {"asset": sequence.get_path_name(), "duration_s": duration,
               "same_skeleton": sequence.get_editor_property("skeleton") == skeleton,
               "rate_scale": sequence.get_editor_property("rate_scale"),
               "root_motion_enabled": sequence.get_editor_property("enable_root_motion"),
               "force_root_lock": sequence.get_editor_property("force_root_lock"),
               "samples": []}
        try:
            row["notifies"] = [str(n) for n in sequence.get_editor_property("notifies")]
        except Exception as error:
            row["notifies_unavailable"] = str(error)
        count = math.ceil(duration * 60)
        for i in range(count + 1):
            time = min(i / 60, duration)
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
            assert unreal.AnimPoseExtensions.is_valid(pose), path
            points = {}
            for bone in ("root", "pelvis", "foot_l", "foot_r"):
                transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
                points[bone] = {"position_cm": [getattr(transform.translation, axis) for axis in "xyz"],
                                "rotation_xyzw": [getattr(transform.rotation, axis) for axis in "xyzw"]}
            row["samples"].append({"time_s": time, "points": points})
        report["animations"].append(row)
    report["status"] = "complete"
except Exception:
    report["error"] = traceback.format_exc()
(root / "animation-inventory.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
if report["status"] != "complete":
    raise RuntimeError(report["error"])
