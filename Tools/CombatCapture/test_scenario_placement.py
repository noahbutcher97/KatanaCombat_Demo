import copy
import unittest

from analyze_capture import CaptureError
from scenario_placement import resolve_placement, validate_placement
from evaluate_capture import compatibility


class ScenarioPlacementTests(unittest.TestCase):
    def fixture(self):
        definition = dict(placements={"rear": {"Attacker": dict(offset_cm=[0, 0, 0], yaw_deg=0),
                                              "Victim": dict(offset_cm=[100, 0, 0], yaw_deg=0)}})
        scenario = dict(definition=definition, placement="rear", placement_observation={"origin_cm": [50, 30, 90],
                        "roles": {"Attacker": dict(location_cm=[50, 30, 90], yaw_deg=0),
                                  "Victim": dict(location_cm=[150, 30, 90], yaw_deg=0)}})
        return scenario, dict(placement="rear"), dict(placement="rear")

    def test_observed_pose_must_match_requested_origin_and_roles(self):
        scenario, context, metadata = self.fixture()
        self.assertEqual(validate_placement(scenario, context, metadata)["name"], "rear")
        for mutate in (lambda s: s["placement_observation"]["origin_cm"].__setitem__(0, 0),
                       lambda s: s["placement_observation"]["roles"]["Victim"].update(yaw_deg=180),
                       lambda s: s["placement_observation"]["roles"].pop("Victim"),
                       lambda s: s.pop("placement_observation")):
            changed = copy.deepcopy(scenario); mutate(changed)
            with self.assertRaises(CaptureError): validate_placement(changed, context, metadata)
        with self.assertRaises(CaptureError): validate_placement(scenario, {}, metadata)
        with self.assertRaises(CaptureError): validate_placement(scenario, context, {})

    def test_rejects_unknown_roles_extra_fields_and_nonfinite_or_coerced_values(self):
        scenario, _, _ = self.fixture()
        for field, value in [("offset_cm", [False, 0, 0]), ("offset_cm", [3001, 0, 0]),
                             ("offset_cm", [0, 0]), ("yaw_deg", "0"), ("yaw_deg", float("nan")),
                             ("yaw_deg", float("inf")), ("yaw_deg", 181), ("yaw_deg", True),
                             ("yaw_deg", 10**400)]:
            definition = copy.deepcopy(scenario["definition"])
            definition["placements"]["rear"]["Victim"][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(CaptureError):
                resolve_placement(definition, "rear")
        for change in (lambda p: p.update(Bystander=p["Victim"]), lambda p: p["Victim"].update(extra=1)):
            definition = copy.deepcopy(scenario["definition"]); change(definition["placements"]["rear"])
            with self.assertRaises(CaptureError): resolve_placement(definition, "rear")
        with self.assertRaises(CaptureError): resolve_placement(scenario["definition"], "unknown")

    def test_default_and_equivalent_yaw_preserve_compatibility(self):
        self.assertEqual(resolve_placement({}), {})
        validate_placement(dict(definition={}), {})
        scenario, context, metadata = self.fixture()
        scenario["definition"]["placements"]["rear"]["Victim"]["yaw_deg"] = -180
        scenario["placement_observation"]["roles"]["Victim"]["yaw_deg"] = 180
        validate_placement(scenario, context, metadata)
        scenario.update(scenario="FinisherRecovery", map_key="ThirdPerson", variant="Completed")
        alternate = dict(scenario, placement="default")
        self.assertNotEqual(compatibility(scenario), compatibility(alternate))

    def test_grounding_must_explain_achieved_pose_with_bounded_floor_evidence(self):
        scenario, context, metadata = self.fixture()
        scenario["definition"]["placement_support"] = dict(mode="walking_floor", max_adjustment_cm=20)
        for role in scenario["placement_observation"]["roles"].values():
            requested = role["location_cm"][:]
            role["location_cm"][2] -= 5.85
            role["support"] = dict(status="grounded", requested_location_cm=requested,
                capsule_radius_cm=30, capsule_half_height_cm=88, floor_distance_before_cm=8,
                floor_distance_after_cm=2.15, vertical_adjustment_cm=-5.85,
                floor_component="/World/Floor.Collision", floor_impact_cm=[requested[0], requested[1], -6])
        validate_placement(scenario, context, metadata)
        for field, value in (("status", "unsupported_floor"), ("vertical_adjustment_cm", -21),
                             ("floor_distance_after_cm", 8), ("capsule_radius_cm", False),
                             ("requested_location_cm", [0,0,0]), ("floor_component", "")):
            altered = copy.deepcopy(scenario)
            altered["placement_observation"]["roles"]["Attacker"]["support"][field] = value
            with self.subTest(field=field), self.assertRaises(CaptureError):
                validate_placement(altered, context, metadata)
        altered = copy.deepcopy(scenario)
        altered["placement_observation"]["roles"]["Attacker"]["location_cm"][0] += 1
        with self.assertRaises(CaptureError): validate_placement(altered, context, metadata)
        scenario["placement_observation"]["roles"]["Victim"].pop("support")
        with self.assertRaises(CaptureError): validate_placement(scenario, context, metadata)
