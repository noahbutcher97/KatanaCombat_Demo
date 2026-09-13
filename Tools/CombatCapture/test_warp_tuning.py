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

    def test_optional_facing_requires_exact_recorded_override(self):
        for policy in ("face-partner", "face-away-from-partner", "match-partner-heading"):
            scenario, context, _ = self.fixture()
            context["warp_tuning"]["victim_facing_policy"] = policy
            rows = scenario["runtime_asset_overrides"]
            with self.assertRaises(CaptureError):
                validate_runtime_experiment(scenario, context)
            rows.append(dict(role="Victim", asset="/Game/Pair", notify_index=-1,
                             notify_class="PairedAnimationData", property="VictimWarpConfig.FacingPolicy",
                             before="face-partner", after=policy))
            validate_runtime_experiment(scenario, context)
            for field, value in (("before", "unknown"), ("after", "unknown"), ("asset", "/Game/Unrelated"),
                                 ("notify_index", 0), ("role", "Attacker")):
                corrupted = copy.deepcopy(scenario)
                corrupted["runtime_asset_overrides"][-1][field] = value
                with self.assertRaises(CaptureError):
                    validate_runtime_experiment(corrupted, context)
            del context["warp_tuning"]["victim_facing_policy"]
            with self.assertRaises(CaptureError):
                validate_runtime_experiment(scenario, context)

    def test_unknown_facing_is_not_silently_defaulted(self):
        for policy in (None, True, 1, [], "unknown"):
            with self.subTest(policy=policy), self.assertRaises(CaptureError):
                validate_warp_tuning_settings(dict(victim_window_s=[.2, .87], victim_offset_cm=[50, 0, 0],
                                                  victim_facing_policy=policy))

    def test_invalid_window_placement_and_types_are_rejected(self):
        for window, offset in [([.9, .2], [50, 0, 0]), ([-.1, .8], [50, 0, 0]),
                               ([0, 6], [50, 0, 0]), ([.2, .21], [50, 0, 0]),
                               ([.2, .8], [201, 0, 0]), ([.2, .8], [50, 0, 1]),
                               ([False, .8], [50, 0, 0]), ([.2, float("nan")], [50, 0, 0])]:
            with self.subTest(window=window, offset=offset), self.assertRaises(CaptureError):
                validate_warp_tuning_settings(dict(victim_window_s=window, victim_offset_cm=offset))

    def test_primary_sync_independent_fields_require_both_roles(self):
        for settings in ({"time_s": .6}, {"nudge_enabled": False}, {"time_s": .6, "nudge_enabled": False}):
            scenario, context, _ = self.fixture()
            context["warp_tuning"]["primary_sync"] = settings
            rows = scenario["runtime_asset_overrides"]
            for role in ("Attacker", "Victim"):
                with self.assertRaises(CaptureError):
                    validate_runtime_experiment(scenario, context)
                start = settings.get("time_s", .0001)
                rows.append(dict(role=role, asset="/Game/" + role, notify_index=2,
                                 notify_class="AnimNotifyState_PairedAnimationSync", property="PrimarySyncSettings",
                                 before=dict(time_s=.0001, end_s=.0801, nudge_enabled=True),
                                 after=dict(time_s=start, end_s=start+.08, nudge_enabled=settings.get("nudge_enabled", True))))
            validate_runtime_experiment(scenario, context)
            for change in (lambda r: r.update(notify_index=1), lambda r: r.update(asset="/Game/Unrelated"),
                           lambda r: r["after"].update(time_s=.4), lambda r: r["after"].update(end_s=4),
                           lambda r: r["after"].update(nudge_enabled=not settings.get("nudge_enabled", True))):
                corrupt = copy.deepcopy(scenario)
                change(corrupt["runtime_asset_overrides"][-1])
                with self.assertRaises(CaptureError):
                    validate_runtime_experiment(corrupt, context)

    def test_primary_sync_invalid_settings_fail_closed(self):
        for setting in ({}, None, [], {"unknown": True}, {"time_s": True}, {"time_s": -1},
                        {"time_s": 6}, {"time_s": float("nan")}, {"nudge_enabled": 1}):
            _, context, _ = self.fixture()
            context["warp_tuning"]["primary_sync"] = setting
            with self.subTest(setting=setting), self.assertRaises(CaptureError):
                validate_warp_tuning_settings(context["warp_tuning"])


if __name__ == "__main__":
    unittest.main()
