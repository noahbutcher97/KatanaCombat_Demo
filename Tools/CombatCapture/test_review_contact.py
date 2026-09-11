import unittest

from review_contact import linked_actor, project, segment_gap


class ContactReviewTests(unittest.TestCase):
    def test_projection_uses_ue_row_vectors_and_constrained_rectangle(self):
        frame = dict(world_to_clip_row_major=[1, 0, 0, 0, 0, 1, 0, 0,
                                              0, 0, 1, 0, .25, -.5, 0, 1],
                     projection_view_rect=[100, 50, 900, 450])
        self.assertEqual(project([0, 0, 0], frame), [600, 350])
        self.assertEqual(project([.25, .5, 0], frame), [700, 250])

    def test_geometry_behind_camera_is_rejected(self):
        frame = dict(world_to_clip_row_major=[1, 0, 0, 0, 0, 1, 0, 0,
                                              0, 0, 1, 1, 0, 0, 0, 0],
                     projection_view_rect=[0, 0, 960, 540])
        with self.assertRaisesRegex(ValueError, "behind"):
            project([0, 0, -1], frame)

    def test_same_serial_on_different_engine_frame_is_rejected(self):
        actor = dict(role="Victim", valid=True, pose_evaluation_serial=5, pose_engine_frame=20)
        frame = dict(pose_links=[dict(actor, pose_engine_frame=21)])
        with self.assertRaisesRegex(ValueError, "pose mismatch"):
            linked_actor(dict(actors=[actor]), frame, "Victim")

    def test_gap_respects_finite_segment_and_sphere_radius(self):
        self.assertAlmostEqual(segment_gap([0, 0, 0], [10, 0, 0], [13, 4, 0], 2), 3)
        self.assertAlmostEqual(segment_gap([0, 0, 0], [10, 0, 0], [5, 1, 0], 2), -1)


if __name__ == "__main__":
    unittest.main()
