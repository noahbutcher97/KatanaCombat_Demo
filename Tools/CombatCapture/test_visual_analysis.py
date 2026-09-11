import base64
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from review_visual import publish
from visual_analysis import load_evidence, render_review, validate_review


class VisualAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.pixels = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+jRZkAAAAASUVORK5CYII=")
        self.evidence = dict(frames=[dict(file="frames/observation.png", simulation_time_s=2.5,
                                         image_sha256=hashlib.sha256(self.pixels).hexdigest(),
                                         image="data:image/png;base64," + base64.b64encode(self.pixels).decode())])
        self.page = '<script id="visual-evidence" type="application/json">' + json.dumps(self.evidence) + '</script>'
        self.digest = hashlib.sha256(self.page.encode()).hexdigest()
        # A locomotion/camera review has no pair, weapon, montage or body proxy.
        self.review = dict(schema_version=1, title="Locomotion camera review", evidence_sha256=self.digest,
                           reviewer=dict(kind="human", identity="Test reviewer"), summary="Framing inspection only",
                           reviewed_frames=["frames/observation.png"], findings=[dict(id="camera_framing", category="Framing",
                           assessment="concern", basis="pixels", observation="Character exits the view",
                           interpretation="Recovery motion cannot be reviewed", limits="Single observed frame",
                           next_action="Capture a wider camera view", evidence=["frames/observation.png"])])

    def test_non_contact_review_uses_shared_schema_and_simulation_clock(self):
        summary = validate_review(self.review, self.evidence, self.digest)
        self.assertEqual(summary["status"], "review_recorded")
        html = render_review(self.review, self.evidence, summary, "pixels.html")
        self.assertIn("simulation 2.500s", html)
        self.assertIn("pixels.html#frame=0", html)

    def test_changed_evidence_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "different evidence"):
            validate_review(self.review, self.evidence, "changed")

    def test_unreviewed_frame_cannot_support_a_finding(self):
        self.review["reviewed_frames"] = []
        with self.assertRaisesRegex(ValueError, "unreviewed"):
            validate_review(self.review, self.evidence, self.digest)

    def test_capture_pass_cannot_be_used_as_a_visual_verdict(self):
        self.review["findings"][0]["assessment"] = "pass"
        with self.assertRaisesRegex(ValueError, "visual approval"):
            validate_review(self.review, self.evidence, self.digest)

    def test_detector_requires_version_and_calibration(self):
        self.review["reviewer"]["kind"] = "automated_detector"
        with self.assertRaisesRegex(ValueError, "detector version"):
            validate_review(self.review, self.evidence, self.digest)

    def test_unreviewed_disposition_cannot_claim_pixels(self):
        self.review["findings"][0]["assessment"] = "not_reviewed"
        with self.assertRaisesRegex(ValueError, "cannot claim"):
            validate_review(self.review, self.evidence, self.digest)

    def test_swapped_embedded_image_and_duplicate_frames_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            page = Path(directory) / "pixels.html"
            page.write_text(self.page, encoding="utf-8")
            self.assertEqual(load_evidence(page)[1], self.digest)
            corrupted = self.page.replace(self.evidence["frames"][0]["image_sha256"], "0" * 64)
            page.write_text(corrupted, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                load_evidence(page)
            duplicated = copy.deepcopy(self.evidence)
            duplicated["frames"] *= 2
            page.write_text('<script id="visual-evidence" type="application/json">' + json.dumps(duplicated) + '</script>', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Duplicate"):
                load_evidence(page)

    def test_rejected_republish_replaces_previous_success(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            evidence, findings, output = (directory / name for name in ("pixels.html", "findings.json", "review.html"))
            evidence.write_text(self.page, encoding="utf-8")
            findings.write_text(json.dumps(self.review), encoding="utf-8")
            self.assertEqual(publish(evidence, findings, output)["status"], "review_recorded")
            self.review["evidence_sha256"] = "stale"
            findings.write_text(json.dumps(self.review), encoding="utf-8")
            with self.assertRaises(ValueError):
                publish(evidence, findings, output)
            self.assertEqual(json.loads(output.with_suffix(".json").read_text())["status"], "inconclusive")

    def test_finding_text_is_escaped(self):
        self.review["findings"][0]["observation"] = "<script>alert(1)</script>"
        summary = validate_review(self.review, self.evidence, self.digest)
        html = render_review(self.review, self.evidence, summary, "pixels.html")
        self.assertNotIn("<script>", html)
        self.assertIn("&lt;script&gt;", html)

    def test_malformed_review_is_rejected_with_a_schema_error(self):
        for value in ([], None, "review"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate_review(value, self.evidence, self.digest)
        self.review["findings"] = [None]
        with self.assertRaisesRegex(ValueError, "must be objects"):
            validate_review(self.review, self.evidence, self.digest)


if __name__ == "__main__":
    unittest.main()
