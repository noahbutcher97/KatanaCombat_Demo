import copy
import unittest

from evaluate_capture import CaptureError, validate_entry_override, validate_entry_settings


class EntryPresentationTests(unittest.TestCase):
    def fixture(self):
        return dict(enabled=True, victim_offset_cm=[100, 0, 0], victim_yaw_deg=0,
                    duration_s=.65, translation_speed_cm_s=122, travel_budget_cm=150,
                    turn_rate_deg_s=540, turn_budget_deg=180, position_tolerance_cm=2,
                    yaw_tolerance_deg=3)

    def test_original_settings_keep_victim_moving_without_animation(self):
        settings = validate_entry_settings(self.fixture())
        self.assertEqual(settings["moving_role"], "victim")
        self.assertEqual(settings["movement_animation"], "")
        validate_entry_override(self.fixture(), dict(notify_class="PairedAnimationData",
            before=self.fixture(), after=settings))

    def test_presentation_identity_and_timing_are_verified(self):
        settings = validate_entry_settings(self.fixture())
        settings.update(moving_role="initiator", movement_animation="/Game/Walk.Walk")
        row = dict(notify_class="PairedAnimationData", before=self.fixture(), after=settings)
        validate_entry_override(settings, row)
        for key, value in dict(moving_role="victim", movement_animation="/Game/Other.Other",
                               movement_slot="OtherSlot", movement_play_rate=2,
                               movement_blend_in_s=.2, movement_blend_out_s=.2).items():
            changed = copy.deepcopy(row); changed["after"][key] = value
            with self.subTest(field=key), self.assertRaises(CaptureError):
                validate_entry_override(settings, changed)

    def test_partial_unknown_and_invalid_fields_fail_closed(self):
        full = validate_entry_settings(self.fixture())
        cases = [self.fixture() | {"moving_role": "initiator"}, full | {"extra": True}]
        cases += [full | {key: value} for key, value in [
            ("moving_role", "unknown"), ("movement_animation", None), ("movement_animation", "local"),
            ("movement_slot", "None"), ("movement_play_rate", True), ("movement_play_rate", 0),
            ("movement_blend_in_s", float("nan")), ("movement_blend_out_s", -1)]]
        for settings in cases:
            with self.subTest(settings=settings), self.assertRaises(CaptureError):
                validate_entry_settings(settings)


if __name__ == "__main__":
    unittest.main()
