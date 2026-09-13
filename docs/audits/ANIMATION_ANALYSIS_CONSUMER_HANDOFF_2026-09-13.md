# Shared mesh acquisition requirements from Katana

Consumer qualification of **exact merged commit
`2fb0dc980dbdba1348b02e3a93b4512606aa5d16`** found two independent blockers.
This note is for the AnimationAnalysis worker. No shared implementation was changed
by the consumer integration. It does not assess subsequent development commits.

1. **Finalized AnimBlueprint pose and its rigid attachment.** During ThirdPerson's
   completed paired finisher at montage time 0.400092 s, both effective ordinary
   skeletal components used `ABP_SamuraiCharacter_C`, animation mode 0, LOD 0,
   89 required bones, no running parallel evaluation at the inventory callback,
   no post-process instance/class, leader, reference override or physics blending.
   Both CPU and GPU enrollment rejected with:

   ```text
   unavailable: pose ordering requires finalized single-node animation without leader, post-process, physics blending or reference-pose override
   ```

   The equipped static katana directly attached at `weapon_r` receives the same
   CPU rejection through its skeletal parent. The smallest useful qualification
   is this ordinary AnimGraph/montage path's finalized pose, with correct evaluated
   bones/LOD and attachment transform witnesses. Arbitrary physics/post-process
   support is not justified by this case. Do not change the consumer to single-node.

2. **A coherent multi-component observation relationship.** The separate authored
   single-node fixture exports all ten native CPU/rigid observations. Each passes
   its explicit reference coverage requirement. All five two-component pairs fail
   `second:acquisition_mismatch`, including when their engine frame is identical.
   `FAnimationCaptureMeshReference::Prepare` independently assigns
   `FPlatformTime::Seconds()` to each observation; the GPU producer also calls that
   preparation. `measure_mesh_pair` requires both stamps to equal the pair stamp.
   Sequential export increased the observed skew, but eliminating export cannot
   make independent acquisition calls share an identical clock identity.

   Review a native batch/group acquisition contract or an explicitly witnessed
   coherence relationship understood by the portable API. Keep per-component
   acquisition/completion stamps, generations and observer-local pose revisions.
   Same engine frame alone, nearby timestamps, frozen-looking poses, or consumer
   timestamp replacement are insufficient fixes. Include two distinct native
   components through replay into a successful shared pair call, plus stale pose,
   one-side replacement, late completion, unsupported coverage and gap controls.

The victim also has effective masked/PDO facial-hair materials. The reference's
explicit geometric head-neighborhood mapping includes 2,282 triangles from those
material slots; this count is a post-selection coverage audit, not anatomical
inference. Current pre-material geometry cannot qualify that visible surface.
No effective nonzero/external morphs, mapped cloth sections or LOD-0 deformer
instances were found at the live callback. This does not qualify their absence
through the whole interval or across other workflows.

The [integration report](MESH_REGION_PAIR_INTEGRATION_2026-09-13.md) carries final
run identities, clocks, controls, exact replay archive/hash and commands. Original
and final attempts are retained separately. The shared deferral to revisit first
is live pose support; coherent pair acquisition is an additional required contract
fix. Mask/PDO coverage matters if the result sought is visible facial contact.
Broader deformation, containment, swept collision and artistic evaluation remain
separate work.
