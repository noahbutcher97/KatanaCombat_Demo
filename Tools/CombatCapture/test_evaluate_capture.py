import copy
import json
from pathlib import Path
import unittest
from unittest.mock import patch

import analyze_capture
import evaluate_capture as evaluator
from analyze_capture import CaptureError
from run_scenario import scenario_outputs, automation_succeeded
import test_analyze_capture


class ScenarioEvaluationTests(unittest.TestCase):

    def test_transient_experiment_requires_matching_effective_provenance(self):
        overrides = [dict(role=role, asset="/Game/Montage", notify_index=0,
                          notify_class="AnimNotifyState_PairedAnimationCollision",
                          property="bDisableMovement", before=True, after=False)
                     for role in ("Attacker", "Victim")]
        scenario = dict(runtime_experiment="permit-root-motion", runtime_asset_overrides=overrides)
        context = dict(runtime_experiment="permit-root-motion")
        metadata = dict(runtime_experiment="permit-root-motion", runtime_asset_overrides=json.dumps(overrides))
        actual = evaluator.validate_runtime_experiment(scenario, context, metadata)
        self.assertEqual(actual["asset_overrides"], overrides)
        with self.assertRaisesRegex(CaptureError, "metadata disagrees"):
            evaluator.validate_runtime_experiment(scenario, context, {})
        with self.assertRaisesRegex(CaptureError, "identity is inconsistent"):
            evaluator.validate_runtime_experiment(scenario, {}, metadata)
        scenario["runtime_asset_overrides"][0]["property"] = "bApplyDamage"
        with self.assertRaisesRegex(CaptureError, "Unsupported runtime notify override"):
            evaluator.validate_runtime_experiment(scenario, context, metadata)

    def test_experiment_cannot_reuse_unmodified_reference_compatibility(self):
        original = evaluator.compatibility(self.scenario)
        self.scenario.update(runtime_experiment="permit-root-motion", runtime_asset_overrides=[{"role": "Attacker"}])
        self.assertNotEqual(original, evaluator.compatibility(self.scenario))
        with self.assertRaisesRegex(CaptureError, "both participants"):
            evaluator.validate_runtime_experiment(self.scenario, dict(runtime_experiment="permit-root-motion"))
        self.scenario["runtime_asset_overrides"] = [None]
        with self.assertRaises(CaptureError):
            evaluator.validate_runtime_experiment(self.scenario, dict(runtime_experiment="permit-root-motion"))

    def test_warp_controls_require_exact_role_and_property(self):
        movement = [dict(role=role, asset="/Game/" + role, notify_index=0,
                         notify_class="AnimNotifyState_PairedAnimationCollision",
                         property="bDisableMovement", before=True, after=False) for role in ("Attacker", "Victim")]
        for role, experiment, property_name in (("Attacker", "attacker-source-translation", "bWarpTranslation"),
                                                ("Victim", "victim-source-translation", "bWarpTranslation"),
                                                ("Victim", "victim-source-rotation", "bWarpRotation")):
            warp = dict(role=role, asset="/Game/" + role, notify_index=1, notify_class="AnimNotifyState_MotionWarping",
                        property="RootMotionModifier." + property_name, before=True, after=False)
            scenario = dict(runtime_experiment=experiment, runtime_asset_overrides=movement + [warp])
            context = dict(runtime_experiment=experiment)
            self.assertEqual(evaluator.validate_runtime_experiment(scenario, context)["name"], experiment)
            for invalid in (movement, movement + [warp, warp], movement[1:] + [warp],
                            movement + [dict(warp, role="Victim" if role == "Attacker" else "Attacker")],
                            movement + [dict(warp, property="RootMotionModifier." + ("bWarpRotation" if property_name == "bWarpTranslation" else "bWarpTranslation"))],
                            movement + [dict(warp, notify_index=-1)], movement + [dict(warp, notify_index=True)]):
                with self.subTest(role=role, overrides=invalid), self.assertRaises(CaptureError):
                    evaluator.validate_runtime_experiment(dict(scenario, runtime_asset_overrides=invalid), context)
            with self.assertRaises(CaptureError):
                evaluator.validate_runtime_experiment(dict(scenario, runtime_experiment="permit-root-motion"),
                                                     dict(runtime_experiment="permit-root-motion"))

    def test_exact_test_result_and_exit_are_required_without_shutdown_banner(self):
        scope = "KatanaCombat.Capture.Scenarios.HoldReleaseRecovery.ThirdPerson.Forward"
        line = "Test Completed. Result={Success} Name={Forward} Path={" + scope + "}"
        self.assertTrue(automation_succeeded(line, 0, scope))
        self.assertFalse(automation_succeeded(line, 255, scope))
        self.assertFalse(automation_succeeded(line, 0, scope + "Extra"))
        self.assertFalse(automation_succeeded(line.replace("Success", "Fail"), 0, scope))
        self.assertFalse(automation_succeeded(line + "\n" + line, 0, scope))
    def setUp(self):
        self.fixture = test_analyze_capture.CaptureAnalysisTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root = self.fixture.root
        self.manifest = self.fixture.manifest
        self.manifest.update(schema_version=2, sample_hz=50, frame_hz=50, sample_count=101,
                             frame_count=101, metadata=dict(run_id="test"), stop_reason="scenario_finished")
        self.manifest["participants"][0]["role"] = "Attacker"
        for kind in ("actions", "defense"):
            (self.root / f"Player.{kind}.csv").rename(self.root / f"Attacker.{kind}.csv")
        template = copy.deepcopy(self.fixture.samples[0]["actors"][0])
        self.samples, self.frames = [], []
        for i in range(101):
            time = i * .02
            actor = copy.deepcopy(template)
            actor.update(role="Attacker", custom_time_dilation=1, mesh_component="CharacterMesh0", mesh_asset="/Game/Mesh",
                         pose_evaluation_serial=i+1, pose_engine_frame=i+10, pose_simulation_time_s=time,
                         pose_finalized_this_frame=True, montages=[])
            actor["points"]["pelvis"].update(actor_cm=[i*.1, 0, 100], component_rotation_xyzw=[0, 0, 0, 1])
            self.samples.append(dict(index=i+1, simulation_time_s=time, wall_elapsed_s=time, engine_frame=i+10,
                                     world_time_dilation=1, world_paused=False, actors=[actor]))
            frame = copy.deepcopy(self.fixture.frames[0])
            frame.update(index=i+1, simulation_time_s=time, wall_elapsed_s=time, sample_index=i+1, pie_draw_index=i+1,
                         engine_frame=i+10, sample_time_lag_s=0, render_resources_ready=True,
                         pose_links=[dict(role="Attacker", pose_evaluation_serial=i+1, pose_engine_frame=i+10)])
            self.frames.append(frame)
        criteria = dict(version=1, required_role="Attacker", required_point="pelvis", max_sample_gap_s=.075,
                        max_pose_age_s=.04, minimum_samples=3, max_frame_gap_s=.12, max_frame_sample_lag_s=.075)
        self.scenario = dict(schema_version=1, run_id="test", scenario="FinisherRecovery", map_key="ThirdPerson",
                             variant="Completed", capture_mode="rendered", control_offset_cm=0, definition=dict(criteria=criteria),
                             checks=[dict(name=name, status="pass", reason="fixture", simulation_time_s=1.8) for name in sorted(evaluator.EXPECTED_CHECKS)],
                             events=[dict(marker=name, simulation_time_s=time) for name, time in
                                     (("finisher_requested", .2), ("finisher_started", .24), ("ownership_released", 1.5), ("recovery_repress", 1.7))])
        context = dict(run_id="test", source=dict(files={"example.cpp": "abc"}),
                       source_identity=evaluator.identity({"example.cpp": "abc"}), editor_binaries={"example.dll": "def"},
                       evaluator_identity=evaluator.implementation_identity(evaluator.__file__), declared_changes=[])
        evaluator.atomic_json(self.root / "run-context.json", context)
        evaluator.atomic_json(self.root / "asset-identity.json", dict(files={"mesh.uasset": "abc"}, identity=evaluator.identity({"mesh.uasset": "abc"})))
        self.write()

    def write(self):
        evaluator.atomic_json(self.root / "session.json", self.manifest)
        evaluator.atomic_json(self.root / "scenario.json", self.scenario)
        for name, rows in (("samples", self.samples), ("frames", self.frames)):
            (self.root / f"{name}.jsonl").write_text("".join(json.dumps(row)+"\n" for row in rows))

    def reference(self):
        return dict(compatibility=evaluator.compatibility(self.scenario), evaluator_identity=evaluator.implementation_identity(evaluator.__file__),
                    max_actor_relative_step_cm=2, acceptance_basis="Known numerical control")

    def check(self, result, name):
        return next(c for c in result["cases"] if c["name"] == name)

    def test_numerical_control_and_real_defect_removal(self):
        reference = self.reference()
        good = evaluator.evaluate(self.root, reference)
        self.assertEqual(good["status"], "pass")
        self.assertAlmostEqual(good["measurements"]["transition"]["max_actor_relative_step_cm"], .1)
        self.samples[30]["actors"][0]["points"]["pelvis"]["actor_cm"][2] += 200
        self.write()
        bad = evaluator.evaluate(self.root, reference)
        self.assertEqual(self.check(bad, "pose.transition_envelope")["status"], "fail")
        self.samples[30]["actors"][0]["points"]["pelvis"]["actor_cm"][2] -= 200
        self.write()
        self.assertEqual(evaluator.evaluate(self.root, reference)["status"], "pass")

    def test_hold_scenario_requires_its_own_assertions_and_event_window(self):
        self.scenario.update(scenario="HoldReleaseRecovery", variant="Forward")
        self.scenario["events"] = [dict(marker=name, simulation_time_s=time) for name, time in
                                   (("hold_requested", .2), ("hold_started", .3), ("hold_released", .7),
                                    ("follow_up_observed", .9), ("ownership_released", 1.5), ("recovery_repress", 1.7))]
        names = ("authored_hold_available", "real_hold_started", "hold_movement_suppressed",
                 "competing_input_preserves_hold", "authored_follow_up_starts", "hold_cleanup",
                 "movement_recovers", "repress_executes_attack", "scenario_completes")
        self.scenario["checks"] = [dict(name=name, status="pass", reason="fixture", simulation_time_s=1.8) for name in names]
        self.write()
        result = evaluator.evaluate(self.root)
        self.assertEqual(result["status"], "pass")
        self.assertAlmostEqual(result["timing"]["release_to_follow_up_observation_s"]["value"], .2)
        self.scenario["checks"] = [c for c in self.scenario["checks"] if c["name"] != "authored_follow_up_starts"]
        self.write()
        result = evaluator.evaluate(self.root)
        self.assertEqual(result["status"], "inconclusive")
        self.assertEqual(self.check(result, "gameplay.authored_follow_up_starts")["status"], "inconclusive")

    def test_each_missing_stale_or_changed_pose_is_inconclusive(self):
        for field, value in (("valid", False), ("pose_evaluation_serial", 0), ("pose_simulation_time_s", 0),
                             ("pose_finalized_this_frame", False), ("mesh_asset", "/Game/Replacement"),
                             ("pose_engine_frame", 0), ("custom_time_dilation", .1)):
            with self.subTest(field=field):
                actor = self.samples[30]["actors"][0]
                original = actor[field]
                actor[field] = value
                self.write()
                result = evaluator.evaluate(self.root, self.reference())
                self.assertEqual(self.check(result, "pose.transition_envelope")["status"], "inconclusive")
                actor[field] = original

    def test_missing_point_and_nonfinite_quaternion_reject(self):
        actor = self.samples[30]["actors"][0]
        original = actor["points"]["pelvis"]
        actor["points"]["pelvis"] = None
        self.write()
        self.assertEqual(self.check(evaluator.evaluate(self.root), "pose.transition_envelope")["status"], "inconclusive")
        actor["points"]["pelvis"] = original
        original["component_rotation_xyzw"][0] = float("nan")
        self.write()
        with self.assertRaises(CaptureError):
            evaluator.evaluate(self.root)

    def test_missing_frames_does_not_upgrade_headless_to_visual_pass(self):
        self.frames = []
        self.manifest.update(frame_count=0, render_available=False)
        self.write()
        self.assertEqual(self.check(evaluator.evaluate(self.root), "visual.window_evidence")["status"], "not_run")

    def test_render_readiness_and_association_fail_eligibility(self):
        for field, value in (("render_resources_ready", False), ("sample_time_lag_s", 1), ("engine_frame", 0)):
            with self.subTest(field=field):
                original = self.frames[30][field]
                self.frames[30][field] = value
                self.write()
                self.assertEqual(self.check(evaluator.evaluate(self.root), "visual.window_evidence")["status"], "inconclusive")
                self.frames[30][field] = original

    def test_sparse_or_uncovered_event_interval_is_inconclusive(self):
        original = copy.deepcopy(self.samples)
        for rows in (original[20:], original[:40] + original[60:]):
            self.samples = rows
            for i, row in enumerate(self.samples):
                row["index"] = i+1
            self.frames = []
            self.manifest.update(sample_count=len(rows), frame_count=0, render_available=False)
            self.write()
            self.assertEqual(self.check(evaluator.evaluate(self.root), "pose.transition_envelope")["status"], "inconclusive")

    def test_legacy_schema_remains_readable_but_cannot_assert_fresh_pose(self):
        self.manifest["schema_version"] = 1
        self.write()
        result = evaluator.evaluate(self.root)
        self.assertEqual(result["recording_integrity"], "valid")
        self.assertEqual(self.check(result, "pose.transition_envelope")["status"], "inconclusive")

    def test_reference_rejects_incompatible_criteria_not_candidate_asset_hash(self):
        reference = self.reference()
        self.scenario["definition"]["criteria"]["max_sample_gap_s"] = .08
        self.write()
        self.assertEqual(self.check(evaluator.evaluate(self.root, reference), "pose.transition_envelope")["status"], "inconclusive")
        result = evaluator.evaluate(self.root, self.reference())
        self.assertEqual(result["status"], "pass")

    def test_incomplete_scenario_does_not_pass_on_only_recorded_checks(self):
        self.scenario["checks"].pop()
        self.write()
        self.assertEqual(evaluator.evaluate(self.root)["status"], "inconclusive")

    def test_rejected_rerun_replaces_previous_success(self):
        evaluator.evaluate_and_write(self.root, self.reference())
        self.assertIn("pass", (self.root / "evaluation.html").read_text())
        (self.root / "scenario.json").write_text("truncated")
        with self.assertRaises(CaptureError):
            evaluator.evaluate_and_write(self.root)
        self.assertEqual(json.loads((self.root / "evaluation.json").read_text())["status"], "inconclusive")
        self.assertIn("Evaluation rejected", (self.root / "evaluation.html").read_text())

    def test_analysis_rejection_invalidates_previous_report(self):
        with patch("sys.argv", ["analyze_capture.py", str(self.root)]):
            self.assertEqual(analyze_capture.main(), 0)
            (self.root / "session.json").write_text("truncated")
            self.assertEqual(analyze_capture.main(), 2)
        self.assertEqual(json.loads((self.root / "analysis.json").read_text())["analysis_state"], "inconclusive")
        self.assertIn("No current analysis", (self.root / "report.html").read_text())

    def test_failed_automation_log_replay_is_one_artifact(self):
        log = "LogTemp: COMBAT_SCENARIO_OUTPUT=D:/Saved/Run\nLogAutomationController: COMBAT_SCENARIO_OUTPUT=D:/Saved/Run [log] \n"
        self.assertEqual(scenario_outputs(log), {"D:/Saved/Run"})

    def test_viewport_resolution_is_a_measurement_requirement(self):
        self.scenario["definition"].update(viewport_width=960, viewport_height=540)
        self.write()
        result = evaluator.evaluate(self.root)
        self.assertEqual(self.check(result, "visual.window_evidence")["status"], "inconclusive")

    def test_changed_source_requires_an_explicit_candidate_declaration(self):
        result = evaluator.evaluate(self.root)
        reference = self.reference()
        reference["provenance"] = dict(result["provenance"], execution_source_identity="previous source")
        self.assertEqual(self.check(evaluator.evaluate(self.root, reference), "pose.transition_envelope")["status"], "inconclusive")
        context = evaluator.read_json(self.root / "run-context.json")
        context["declared_changes"] = ["Correct input recovery"]
        evaluator.atomic_json(self.root / "run-context.json", context)
        self.assertEqual(self.check(evaluator.evaluate(self.root, reference), "pose.transition_envelope")["status"], "pass")

    def test_reference_cannot_use_duplicate_runs_as_repeatability(self):
        from summarize_runs import create_reference
        evaluator.evaluate_and_write(self.root)
        with self.assertRaisesRegex(CaptureError, "Duplicate recordings"):
            create_reference([self.root]*3, 10, "Known control")

    def test_reference_rejects_modified_bundle(self):
        from summarize_runs import create_reference
        evaluator.evaluate_and_write(self.root)
        self.samples[30]["actors"][0]["points"]["pelvis"]["actor_cm"][2] = 300
        self.write()
        with self.assertRaisesRegex(CaptureError, "changed after evaluation"):
            create_reference([self.root]*3, 10, "Known control")

    def test_reference_rejects_changed_portable_implementation(self):
        reference = self.reference()
        self.assertEqual(evaluator.evaluate(self.root, reference)["status"], "pass")
        with patch("capture_format.implementation_manifest", return_value={"metrics.py": "changed"}):
            result = evaluator.evaluate(self.root, reference)
        outcome = self.check(result, "pose.transition_envelope")
        self.assertEqual(outcome["status"], "inconclusive")
        self.assertIn("evaluator identity is incompatible", outcome["reason"])

    def test_rejected_reference_read_invalidates_prior_success(self):
        evaluator.evaluate_and_write(self.root, self.reference())
        with self.assertRaises(CaptureError):
            evaluator.evaluate_and_write(self.root, self.root / "absent-reference.json")
        self.assertEqual(evaluator.read_json(self.root / "evaluation.json")["status"], "inconclusive")

    def test_capture_disabled_keeps_gameplay_timing(self):
        self.scenario["capture_mode"] = "disabled"
        self.write()
        result = evaluator.evaluate(self.root)
        self.assertEqual(result["status"], "pass")
        self.assertAlmostEqual(result["timing"]["paired_observation_duration_s"]["value"], 1.26)
        evaluator.evaluate_and_write(self.root)
        page = (self.root / "evaluation.html").read_text()
        self.assertIn('href="scenario.json"', page)
        self.assertNotIn('href="report.html"', page)


if __name__ == "__main__":
    unittest.main()
