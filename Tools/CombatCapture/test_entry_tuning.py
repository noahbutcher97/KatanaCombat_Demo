import copy
import unittest

from analyze_capture import CaptureError
from evaluate_capture import validate_entry_settings, validate_runtime_experiment
import test_warp_tuning


def entry_settings():
    return dict(enabled=True, victim_offset_cm=[80, 0, 0], victim_yaw_deg=0,
                duration_s=.5, translation_speed_cm_s=300, travel_budget_cm=150,
                turn_rate_deg_s=540, turn_budget_deg=180, position_tolerance_cm=2, yaw_tolerance_deg=3)


class EntryTuningTests(unittest.TestCase):
    def fixture(self):
        scenario, context, _ = test_warp_tuning.WarpTuningTests().fixture()
        entry = entry_settings()
        context["warp_tuning"]["entry"] = entry
        scenario["runtime_asset_overrides"].append(dict(role="Victim", asset="/Game/Pair", notify_index=-1,
            notify_class="PairedAnimationData", property="Entry", before=dict(entry, enabled=False), after=copy.deepcopy(entry)))
        return scenario, context

    def test_complete_entry_provenance_is_required(self):
        scenario, context = self.fixture()
        validate_runtime_experiment(scenario, context)
        for mutate in (lambda rows: rows.pop(), lambda rows: rows[-1].update(asset="/Game/Unrelated"),
                       lambda rows: rows[-1]["after"].update(travel_budget_cm=300),
                       lambda rows: rows[-1]["before"].update(duration_s=False),
                       lambda rows: rows[-1].update(role="Attacker")):
            scenario, context = self.fixture()
            mutate(scenario["runtime_asset_overrides"])
            with self.assertRaises(CaptureError):
                validate_runtime_experiment(scenario, context)

    def test_invalid_limits_and_incomplete_entry_fail_closed(self):
        for key, value in (("duration_s", 0), ("translation_speed_cm_s", float("nan")),
                           ("turn_rate_deg_s", 1e100), ("victim_offset_cm", [False, 0, 0]),
                           ("victim_yaw_deg", 181), ("enabled", 1)):
            with self.subTest(key=key), self.assertRaises(CaptureError):
                validate_entry_settings(dict(entry_settings(), **{key: value}))
        settings = entry_settings()
        settings.pop("travel_budget_cm")
        with self.assertRaises(CaptureError):
            validate_entry_settings(settings)

    def test_antipodal_yaw_representation_preserves_the_requested_pose(self):
        scenario, context = self.fixture()
        context["warp_tuning"]["entry"]["victim_yaw_deg"] = -180
        scenario["runtime_asset_overrides"][-1]["after"]["victim_yaw_deg"] = 180
        validate_runtime_experiment(scenario, context)
