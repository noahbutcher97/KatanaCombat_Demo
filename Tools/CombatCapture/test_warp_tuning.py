import copy
import json
import unittest

from analyze_capture import CaptureError
from evaluate_capture import validate_runtime_experiment, validate_warp_tuning_settings


class WarpTuningTests(unittest.TestCase):
    def fixture(self):
        settings = dict(victim_window_s=[.2, .87], victim_offset_cm=[65, 0, 0])
        rows = []
        for role in ("Attacker", "Victim"):
            rows.append(dict(role=role, asset="/Game/" + role, notify_index=1,
                             notify_class="AnimNotifyState_PairedAnimationCollision",
                             property="bDisableMovement", before=True, after=False))
        rows.append(dict(role="Attacker", asset="/Game/Attacker", notify_index=0,
                         notify_class="AnimNotifyState_MotionWarping", property="RootMotionModifier.bWarpTranslation",
                         before=True, after=False))
        rows.append(dict(role="Victim", asset="/Game/Victim", notify_index=0,
                         notify_class="AnimNotifyState_MotionWarping", property="TriggerWindowSeconds",
                         before=[.0001, .873], after=[.2, .87]))
        rows.append(dict(role="Victim", asset="/Game/Pair", notify_index=-1,
                         notify_class="PairedAnimationData", property="VictimWarpConfig.RelativeOffset",
                         before=[50, 0, 0], after=[65, 0, 0]))
        scenario = dict(runtime_experiment="paired-warp-tuning", runtime_asset_overrides=rows)
        context = dict(runtime_experiment="paired-warp-tuning", warp_tuning=settings)
        metadata = dict(runtime_experiment="paired-warp-tuning", runtime_asset_overrides=json.dumps(rows))
        return scenario, context, metadata

    def test_complete_requested_tuning_is_accepted(self):
        scenario, context, metadata = self.fixture()
        self.assertEqual(validate_runtime_experiment(scenario, context, metadata)["name"], "paired-warp-tuning")

    def test_partial_duplicate_wrong_role_and_unrequested_overrides_are_rejected(self):
        mutations = [lambda rows: rows.pop(), lambda rows: rows.append(copy.deepcopy(rows[0])),
                     lambda rows: rows[3].update(role="Attacker"),
                     lambda rows: rows[3].update(after=[.3, .87]),
                     lambda rows: rows[4].update(after=[70, 0, 0]),
                     lambda rows: rows[2].update(property="RootMotionModifier.bWarpRotation"),
                     lambda rows: rows[3].update(asset="/Game/Unrelated"),
                     lambda rows: rows[3].update(notify_index=1)]
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                scenario, context, _ = self.fixture()
                mutate(scenario["runtime_asset_overrides"])
                with self.assertRaises(CaptureError):
                    validate_runtime_experiment(scenario, context)

    def test_captured_provenance_must_match_requested_and_reported_tuning(self):
        scenario, context, metadata = self.fixture()
        metadata["runtime_asset_overrides"] = "[]"
        with self.assertRaises(CaptureError):
            validate_runtime_experiment(scenario, context, metadata)

    def test_invalid_window_placement_and_types_are_rejected(self):
        for window, offset in [([.9, .2], [50, 0, 0]), ([-.1, .8], [50, 0, 0]),
                               ([0, 6], [50, 0, 0]), ([.2, .21], [50, 0, 0]),
                               ([.2, .8], [201, 0, 0]), ([.2, .8], [50, 0, 1]),
                               ([False, .8], [50, 0, 0]), ([.2, float("nan")], [50, 0, 0])]:
            with self.subTest(window=window, offset=offset), self.assertRaises(CaptureError):
                validate_warp_tuning_settings(dict(victim_window_s=window, victim_offset_cm=offset))


if __name__ == "__main__":
    unittest.main()
