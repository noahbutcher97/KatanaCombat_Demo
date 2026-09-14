# Live finisher mesh evaluation

This consumer workflow observes the existing paired-finisher scenario. Katana owns
the participants, explicit triangle profile and interpretation. AnimationAnalysis
owns acquisition, mesh replay, surface queries and interval summaries.

The integration is isolated on `integrate/finalized-pose-trial`. Its exact tested
dependency revision, `ea890e38dced51e21503c5ff637b26ac0b6170b5`, is available on
AnimationAnalysis `main` (verified 2026-09-13). Install it from the public repository
with `python Tools/AnimationAnalysis/setup_dependency.py`; a local source override
is optional.
See the [trial report](../audits/FINALIZED_POSE_CONSUMER_TRIAL_2026-09-13.md) for
the exact revision, source location and tested environment. Rebuild the editor
after installing native changes; updating Python alone is insufficient.

## Capture

Use a new absolute output directory for each process. The observer enrolls once
in the first active paired montage, retains the original participants and waits
for a later natural finalization. It does not drive animation, force a LOD or save
assets. Set these variables only in the process running this diagnostic:

```powershell
$env:KATANA_MESH_OUTPUT = '<new absolute evidence directory>'
$env:KATANA_MESH_BATCH = '1'
python Tools/CombatCapture/run_scenario.py --mode rendered --skip-build --map ThirdPerson --variant Completed --placement rear-reference --camera-view contact_oblique --finisher-experiment paired-warp-tuning --victim-warp-window .15 .45 --victim-warp-offset 42 0 0 --victim-facing-policy match-partner-heading --primary-sync-time .466667 --sync-nudge disabled
```

`--skip-build` requires an already verified matching editor build. The experiment
uses the registered scenario's transient gameplay tuning; acquisition never changes
pose evaluation to meet producer eligibility. Use `--variant Interrupted` in a
separate process/output directory to retain incomplete-interval evidence.

The observer explicitly opts into `FinalizedAnimation` and calls mixed skeletal /
rigid `CaptureBatch` at `OnWorldTickEnd`. It requests seven guard observations at
`0.375 + n/30` montage seconds, surrounding the intended `0.40–0.55` interval.
Acquisition is a shared monotonic clock stamp, not the montage clock. Each component
retains its own generation, configuration and pose revision. Missing observations
and actual gaps remain losses; timestamps are never repaired or interpolated.

Immutable snapshots stay within a 16-snapshot / 256 MiB admission budget until
world cleanup. Replay is exported at teardown to avoid filesystem stalls in the
ongoing RGB/pose capture. A crash before teardown can lose unpublished snapshots;
absence of replay is insufficient evidence. The normal recorder's images and the
native mesh bundles have separate acquisition identities.

Output is `<evidence>/live-batch/live-pairs.json` plus fourteen replay bundles for
a complete acquisition. Bundle names are unique per participant; the two records
within a pair retain the same batch request ID. The older default-policy inventory
is a separate SingleNode eligibility result and may still reject AnimBlueprints.

## Analyze and interpret

```powershell
python Tools/CombatCapture/measure_live_finisher_regions.py '<evidence>/live-batch' --output '<new analysis directory>' --max-pair-tests 1000 --max-node-visits 10000
python -m unittest discover -s Tools/CombatCapture -p test_live_finisher_regions.py
```

Omitting the search switches uses 100,000 triangle tests and 200,000 node visits.
Those limits exhausted after about five minutes on the first full-region trial;
the smaller switches are an explicit diagnostic resource ceiling. Exhaustion
returns insufficient evidence. Successful geometry results and failure controls
are retained individually; a successful tool exit means its controls passed,
not that the requested contact interval qualified.

The tracked [region profile](../../Tools/CombatCapture/regions/finisher-head-katana.json)
retains the exact topology-bound selections from the earlier authored-reference
qualification: 13,599 victim triangles selected by a first-pose centroid box and
all 6,272 weapon triangles. This is a geometric neighborhood, not an anatomical
head/neck segmentation or a blade-only region. Material sections and bone weights
do not establish anatomy. A different intended surface requires a separately
reviewed explicit selection and topology identity; do not silently regenerate the
box against a live pose or narrow the profile to obtain a passing query.

Distances are centimetres; 0.5 cm is a proximity threshold. An exact surface
intersection is neither a penetration depth nor proof of physical contact.
Containment and between-sample behavior remain `not_evaluated`. The pre-material
reference permits explicit morph, cloth, deformer, material-displacement and raster
exclusions. Required visible-surface analysis separately requires material and
raster coverage and remains insufficient when those effects are excluded.

The report retains actual montage bracketing, exact-endpoint flags, monotonic gaps,
capture/export/analysis costs, region sizes, replay hashes, failed acquisitions,
wrong-identity controls and budget controls. It does not claim sustained 30 Hz,
rendered rigid LOD coverage, visual contact acceptance or artistic quality.

Archive and verify both native replay and corresponding RGB/pose evidence before
removing generated images. The trial report records archive members and hashes.
