# Mesh region-pair consumer qualification

This scoped qualification consumes AnimationAnalysis commit
`2fb0dc980dbdba1348b02e3a93b4512606aa5d16`. It changes the dependency pin,
consumer inspection/tests, a Python evaluation adapter and documentation. Existing
finisher authoring WIP and all assets remain separately owned.

## Criteria fixed before acquisition

The selected live workflow is ThirdPerson's completed rear-reference finisher:
the equipped katana surface versus the victim's head/upper-neck neighborhood,
during attacker montage time 0.40–0.55 seconds. The question is sampled geometric
proximity/intersection around the authored strike, measured in centimetres with
0.5 cm proximity tolerance. This is not an artistic or physical-contact verdict.
Required live coverage is current finalized gameplay pose, bone/rigid geometry,
and any effective deformation changing the selected surface. Visible-surface claims
add material displacement and raster visibility requirements. Missing required
coverage is insufficient. Containment stays `not_evaluated`.

Target cadence is 30 Hz with maximum acquisition gap 0.05 seconds, exact recorded
interval endpoints, unchanged topology/component generations and all requested
samples eligible. Source/montage time locates the intended phase; acquisition time
is the producer's immutable monotonic stamp. Never replace one with the other.

A separate authored reference uses the same source sequences and meshes in an
explicit paused single-node fixture, same-heading initial roots 100 cm apart,
source times 0.40, 0.433333, 0.466667, 0.50 and 0.533333 seconds. Its intended
surfaces are expressly pre-material bone/rigid triangles, accepting excluded morph,
cloth, mesh-deformer, material-displacement and raster-visibility coverage. It
cannot qualify the live workflow. Root motion is applied from the authored source.

Region mappings must list explicit global triangle ordinals bound to topology SHA-256.
The weapon region is its complete triangle surface; the victim region is a recorded
geometric selection around the first reference pose's head/neck, fixed across the
interval: a world-axis box centered on the recorded head point, half extents
12/12/15 cm, selecting triangle centroids. Spatial selection is documented and reviewed; material sections and bone
weights are not anatomical labels. No tolerance/region adjustment follows results.
If acquisition or clock compatibility blocks the pair, retain the rejection and
stop short of a distance claim. Same-observation regions may exercise the analysis
API as a separately labeled control only.

## Execution and verification

1. Preserve WIP/content identity; install the exact commit with the existing installer.
2. Build all affected editor modules. Run isolated package checks, dependency/capture
   Python tests and focused native/rendered compatibility tests.
3. Inspect effective live components during the selected finisher and attempt both
   native acquisition enrollments without changing pose evaluation.
4. Export authored reference observations if eligible. Exercise pair/interval APIs
   and retain identity, unsupported coverage, gap and budget controls, timings and
   all failed attempts. Pairing requires the shared API's actual clock contract.
5. Write the consumer report and shared-worker handoff, archive/hash-check replay
   before pruning owned images, and commit verified consumer slices only.
