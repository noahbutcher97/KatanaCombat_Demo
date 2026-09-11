# Finisher contact and motion diagnosis

Date: 2026-09-10 (America/New_York; generated artifacts cross into September 11 UTC).

Follow-up: [sync reporting repair and movement experiment](FINISHER_MOVEMENT_EXPERIMENT_2026-09-10.md) completes correction steps 1 and 2 below. It fixes the exporter and confirms the movement blocker with transient runtime controls, while identifying a victim warp discontinuity. The findings below preserve the state at the original diagnosis.

The current finisher has a demonstrated conflict between authored root motion and movement-disabling collision notifies. Both actors stay fixed after the opening alignment snap while their skeletal poses advance. Changing pair spacing alone has not satisfied the provisional blade/torso criterion. A separate evaluator defect hides the attacker's opening damage sync. Correct the reporting boundary and isolate runtime movement behavior before adjusting contact tolerances or saving animation assets.

This is a diagnostic disposition and concrete correction proposal. No gameplay, editor implementation, profile, or Content changes were made. The paired interaction is not visually approved or fixed by this investigation.

## Evidence and method

Evidence root: `Saved/Logs/FinisherContactDiagnosis-20260910-201713/`. The JSON, scripts, logs and [motion figure](../../Saved/Logs/FinisherContactDiagnosis-20260910-201713/finisher-motion-diagnosis.png) are generated local artifacts. This tracked report preserves the findings if those artifacts are cleaned.

- Reanalyzed the fresh ThirdPerson and DefenseMatrix completed-finisher captures from the [sampling investigation](COMBAT_CAPTURE_SAMPLING_2026-09-10.md); no new gameplay scenario was run during this diagnostic pass.
- Loaded current assets in isolated Unreal Python commandlets without saving packages. Inspected pair/weapon configuration, source root-motion flags, montage lengths and notify states through Unreal APIs.
- Sampled both source animation sequences at 60 Hz with raw root motion included and extracted. Independently composed skeletal/weapon socket transforms and compared against the existing native montage evaluator.
- Ran four new native authored-placement evaluations with unchanged geometry, intervals and tolerances. Each experiment uses a scratch JSON profile under the evidence root.
- Read the runtime lease, sync and warp implementations, and the installed UE 5.6 CharacterMovement implementation. This establishes mechanisms; it does not replace a future controlled runtime correction experiment.

## Confirmed findings

### Root motion is enabled, but movement is suppressed

Current loaded `GhostSamurai_Ambush01` and `GhostSamurai_Ambushed01` both have `enable_root_motion=true`, `force_root_lock=false`, and reference-pose root locking. The July audit's disabled-root-motion finding is obsolete for these current bytes.

The attacker montage's paired-collision notify has `disable_movement=true` over approximately 0.0001–1.9837 s. The victim has the same flag over approximately 0.0001–2.9068 s. These overlap their motion-warp windows, approximately 0.0001–0.4338 s and 0.0001–0.8730 s respectively.

`AnimNotifyState_PairedAnimationCollision.cpp` passes that flag to the paired state lease. `UPairedAnimationComponent::RecomputePairedState` calls `CharacterMovement::DisableMovement`. UE 5.6 `UCharacterMovementComponent::PerformMovement` explicitly consumes/clears animation root motion and returns without moving when movement mode is `MOVE_None`.

On **both maps**, 137 observations per actor from approximately paired time 0.0167 to 2.2839 s show **0 cm actor displacement** and **0 cm skeletal component-root offset** after the initial snap. In contrast, independent unwarped source playback over the shared 2.4333 s interval has maximum root displacement of **60.50 cm attacker / 202.81 cm victim**. These source distances are not desired gameplay warp distances; they establish that the clips contain substantial root travel that is absent from the observed actors.

The montage/lease settings and engine behavior explain a movement blocker during the notify windows. The capture does not record movement mode or lease transitions, so attribution of every later stationary frame, including the attacker's interval after its collision notify, still needs a controlled runtime experiment.

### Damage and a hard alignment correction occur at entry

The attacker's `FinisherImpact` sync is primary, damage-enabled, and has actual trigger time **-0.0001 s** due to the start-boundary trigger offset. Its state duration is 0.63 s; damage occurs on **NotifyBegin**, not at the end of that duration. The victim's `Impact` sync is also configured primary/damage-enabled at **+0.0001 s**, but configured flags alone do not establish authority to commit damage.

At the first captured finisher pose (montage time approximately 0.0167 s), victim health is already zero and the victim is dying. ThirdPerson's runtime log records `FinisherImpact` snapping the victim from **141.4 cm to 80.0 cm** from the attacker. This follows the notify's enabled nudge policy: threshold 100 cm, maximum 150 cm, target 80% of threshold. The victim's own sync logs a missing partner. Neither the data asset's legacy `SyncPointTime=0.55` nor the provisional 0.3–1.5 s blade criterion describes that actual damage-commit moment.

The opening commit and snap are observable facts. The intended visual impact time is not established by those facts and must not be inferred from a default or retimed merely to satisfy a test.

### Start-boundary sync reporting is incomplete

`PairedContactProfileEvaluation.cpp` discards any sync for which `Event.GetTriggerTime() < SectionStart`. Consequently its report excludes `FinisherImpact` at -0.0001 s while runtime executes it. The report retains only the victim's +0.0001 s sync. Neither source sequence contains extra notifies; this is a montage-boundary filtering defect, not an inherited-sequence-notify finding.

The existing `applies_damage` field is also only the notify's configured flag. It does not express primary status or successful runtime ownership/commit. Until corrected, `authored_sync_events` must not be treated as a complete effective runtime damage timeline.

### Warp flags need an explicit contract check

The pair's attacker warp config has translation disabled and rotation enabled, while the actual attacker `RootMotionModifier_SkewWarp` has **both translation and rotation enabled**. `SetupAttackerPairedWarp` uses the attacker's own location as the translation target when the pair config disables translation; it does not switch off the modifier's translation flag. The victim config and modifier both enable translation/rotation.

This is a concrete configuration/consumer mismatch to test after removing the movement blocker. Do not assume disabling the collision movement flag alone will produce correct choreography, or globally change warp behavior on the strength of this one asset.

### Simple placement changes did not fix the declared contact

All experiments retain the 18 cm torso sphere, blade trace segment, 0.3–1.5 s interval, maximum signed gap 8 cm, and all other contact criteria. The attacker initial mesh transform remains position zero / yaw -90 degrees. Victim positions below are mesh origins, not an inferred required actor separation.

| Authored placement | Victim X / yaw | Minimum blade/torso signed gap | Blade criterion |
| --- | --- | --- | --- |
| Existing profile | 50 cm / +90 degrees | 34.64 cm | Fail |
| Same facing, common origin | 0 cm / -90 degrees | 21.90 cm | Fail |
| Same facing, offset | 50 cm / -90 degrees | 13.80 cm | Fail |
| Opposed, common origin | 0 cm / +90 degrees | 31.93 cm | Fail |
| Opposed, runtime entry distance | 80 cm / +90 degrees | 36.45 cm | Fail |

All four new commandlets completed successfully (native exit 0); their Python runners returned 1 because criteria failed. They are successful diagnostic executions, not passing quality evaluations. These are four bounded controls, not an exhaustive fit or proof that no placement could work.

Current runtime minimum signed gaps remain **44.7470 cm ThirdPerson / 44.7471 cm DefenseMatrix**, with maximum contact observation gaps **16.786 / 16.676 ms**. Sampled misses do not prove continuous absence between observations.

### Weapon geometry and preview parity

The weapon uses `DA_Weapon_Katana`, mesh `SKM_Katana`, attachment `weapon_r`, identity attachment offset, unit scale, and explicit static-mesh `weapon_start`/`weapon_end` sockets. Runtime capture metadata and authored setup nominate that same attached mesh; the measurements are not silently using similarly named character sockets.

Independent Unreal source-sequence poses, start-root normalization and weapon-socket composition agree with all **73** native authored blade-gap observations within **0.0001342 cm**. This supports the sampled preview geometry and rules out a material double-root-motion error for this measured curve. It does not establish full AnimGraph parity, correct artistic grip, skin contact, or intended target anatomy. Four reviewed frames suggest coordinated body interaction but do not establish that the currently equipped blade should strike the provisional upper-torso proxy at the proposed interval. No weapon rotation correction is justified yet.

The attacker montage is **2.4333 s** long and the victim **4.2000 s**. In the capture, the victim montage is stopped around attacker completion (victim clock approximately 2.434 s), followed by terminal cleanup. That may be an intentional ragdoll handoff; a duration mismatch alone is not a defect. The unplayed victim tail needs review when choosing the intended finish/release behavior.

## Concrete correction sequence

1. **Repair the reporting boundary first.** Preserve nominal notify time and actual trigger offset separately; include start-boundary events that runtime executes without treating arbitrary earlier events as in-section. Export primary/configured-damage metadata separately from runtime commit authority. Add focused controls for a start-offset notify, an ordinary interior notify, a truly out-of-section notify, and role/primary distinctions. Reevaluate the unchanged finisher and demonstrate that `FinisherImpact` is visible. If segment/section traversal cannot be mapped faithfully, report that limitation explicitly.
2. **Run a transient movement experiment.** On duplicate in-memory finisher montages, first change only paired-collision `bDisableMovement` from true to false on both roles. Preserve pawn collision policy and existing independent input ownership. Declare the in-memory overrides in capture provenance; disk hashes alone are insufficient for an altered runtime asset. Record movement mode, lease/window boundaries, root-motion consumption and warp modifier state. Compare against the unchanged control on both maps. Check death-side movement suppression independently.
3. **Resolve the attacker warp flag mismatch as a separate variable.** If the intended meaning of attacker translation-disabled is to preserve authored root travel, test disabling translation on that montage's modifier separately from step 2. Keep the victim warp and nudge policies unchanged until each effect is visible. Do not combine movement, warp, nudge and placement edits into one unexplained improvement.
4. **Review contact and commit timing after movement is understood.** Use the corrected motion/notify reports and rendered frames to choose the actual interaction points, a defensible interval/minimum duration, grip and intended victim finish. Replace the root-distance teleport with a justified alignment policy only if the controlled evidence supports it. Freeze reviewed criteria before verifying an asset correction; do not widen the current proxy to manufacture a pass.
5. **Verify the selected correction.** Run completed and interrupted captures on both maps, including input buffering/suppression, recovery, one authorized damage commit, terminal victim cleanup and bystanders. Require contact evidence eligibility and reviewed visual evidence separately from gameplay passes. Then return to counter runtime evaluation, steering/reachability and the broader stabilization baseline.

No package-save proposal is ready yet: the mechanical blockers are concrete, while the intended blade contact and best runtime correction remain unproven. This diagnostic pass is complete; steps 1–5 are the next corrective work, not claimed results.

## Reproduction and verification limits

The evidence root retains copies of `inspect_finisher_pose_api.py`, `sample_finisher_poses.py`, `inspect_finisher_notifies.py`, `evaluate_finisher_placements.py`, `check_finisher_pose_parity.py`, and `analyze_finisher_motion.py`. Successful sequence sampling used `unreal.AnimPoseExtensions` on **UAnimSequence**, not directly on UAnimMontage. Direct montage evaluation through that engine Python API asserted in an isolated commandlet (exit 3); that failed attempt is retained in `authored-poses-stdout.log`. Earlier protected-property probes exited -1. Subsequent supported property/notify/sequence inspection runs exited 0. The existing native montage evaluator completed all four placement experiments without a process failure.

The [motion summary](../../Saved/Logs/FinisherContactDiagnosis-20260910-201713/motion-diagnosis.json), [notify inspection](../../Saved/Logs/FinisherContactDiagnosis-20260910-201713/notify-inspection.json), [pose parity](../../Saved/Logs/FinisherContactDiagnosis-20260910-201713/pose-parity.json), and [placement experiments](../../Saved/Logs/FinisherContactDiagnosis-20260910-201713/placement-experiments.json) retain exact values and artifact paths.

No build or automation suite rerun was needed for this documentation/read-only diagnosis. Existing gameplay captures and their prior passes are explicitly reused; no new full-suite or corrected-runtime pass is claimed. Scope verification compares the current source/config/tool identity with the start-of-pass snapshot and preserves all 54 protected Content paths, including prior absences and additional-change detection. No files were staged or committed.
