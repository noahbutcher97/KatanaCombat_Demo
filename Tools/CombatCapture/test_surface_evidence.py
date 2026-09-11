import json
from pathlib import Path
import struct
import tempfile
import unittest

from review_surfaces import publish
from visual_analysis import load_evidence
from visual_analysis.surface_evidence import load_surface_bundle


class SurfaceEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.capture=self.root/'capture';self.capture.mkdir()
        self.data=struct.pack('<8sQII',b'SURFACE1',123,4,1)+bytes([32,32,32,255]*4)+bytes([3,0,0,101])+struct.pack('<8f',*([25]*8))
        (self.capture/'sample.surface').write_bytes(self.data)
        self.manifest=dict(schema_version=1,depth_convention='camera_axis_cm_clear_infinity',label_semantics='frontmost_custom_depth',
                           subjects={'LeftObject':3,'RightObject':101},frames=[dict(file='sample.surface',width=4,height=1,engine_frame=123,
                           simulation_time_s=2.5,world_to_clip_row_major=[1.0]*16)])
        self.save()

    def save(self):
        (self.capture/'surfaces.json').write_text(json.dumps(self.manifest))

    def test_portable_review_uses_shared_evidence_reader(self):
        output=self.root/'review.html'
        result=publish(self.capture,'LeftObject','RightObject',.1,output)
        self.assertEqual(result['status'],'measurement_recorded')
        evidence,digest=load_evidence(output)
        self.assertEqual(digest,result['evidence_sha256'])
        self.assertEqual(evidence['frames'][0]['simulation_time_s'],2.5)
        self.assertEqual(result['measurements'][0]['minimum_visible_pixel_center_distance_px'],3)
        self.assertFalse(list(self.root.rglob('*.png')))

    def test_stale_frame_replaces_previous_success(self):
        output=self.root/'review.html';publish(self.capture,'LeftObject','RightObject',.1,output)
        self.manifest['frames'][0]['engine_frame']=122;self.save()
        with self.assertRaisesRegex(ValueError,'frame identity'):
            publish(self.capture,'LeftObject','RightObject',.1,output)
        self.assertEqual(json.loads(output.with_suffix('.json').read_text())['status'],'inconclusive')

    def test_truncated_and_extra_channels_reject(self):
        for data in (self.data[:20],self.data[:-1],self.data+b'\0'):
            (self.capture/'sample.surface').write_bytes(data)
            with self.assertRaises(ValueError):load_surface_bundle(self.capture)

    def test_unknown_labels_and_escape_paths_reject(self):
        self.manifest['subjects'].pop('LeftObject');self.save()
        with self.assertRaisesRegex(ValueError,'Unregistered'):load_surface_bundle(self.capture)
        self.manifest['frames'][0]['file']='../sample.surface';self.save()
        with self.assertRaisesRegex(ValueError,'direct bundle'):load_surface_bundle(self.capture)

    def test_output_cannot_overwrite_input(self):
        original=(self.capture/'surfaces.json').read_bytes()
        with self.assertRaises(ValueError):publish(self.capture,'LeftObject','RightObject',.1,self.capture/'surfaces.html')
        self.assertEqual((self.capture/'surfaces.json').read_bytes(),original)

    def test_invalid_clocks_and_duplicate_observations_reject(self):
        self.manifest['frames'][0]['simulation_time_s']=float('nan');self.save()
        with self.assertRaisesRegex(ValueError,'finite ordered clocks'):load_surface_bundle(self.capture)
        self.manifest['frames'][0]['simulation_time_s']=2.5
        duplicate=dict(self.manifest['frames'][0],file='duplicate.surface')
        (self.capture/'duplicate.surface').write_bytes(self.data)
        self.manifest['frames'].append(duplicate);self.save()
        with self.assertRaisesRegex(ValueError,'increasing render frames'):load_surface_bundle(self.capture)


if __name__=='__main__':unittest.main()
