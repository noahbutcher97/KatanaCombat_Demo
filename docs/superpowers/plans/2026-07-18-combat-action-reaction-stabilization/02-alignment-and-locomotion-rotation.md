# Micro-Plan 02: Alignment And Locomotion Rotation

**Spec:** `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization/02-alignment-and-locomotion-rotation.md`

**Goal:** Remove the defense cap from attack warping, add attack-owned reachability and configurable live player steering, and expose locomotion yaw independently.

## Task 1: Add Failing Alignment Tests

**Modify:**

- `Source/KatanaCombatTest/Private/TargetingComponentTests.cpp`
- `Source/KatanaCombatTest/Private/CombatWarpModifierTests.cpp`

**Create:** `Source/KatanaCombatTest/Private/AttackAlignmentResolverTests.cpp`

- [ ] Prove an attack configured for 720 degrees/second is currently capped by a 180-degree defense setting.
- [ ] Add red tests for the 180-degree default turn budget, rate/deadline reachability, over-budget rejection, exact owner release, play-rate invariance, null target, and missing/disabled warp.
- [ ] Add exact and near-180-degree regressions proving the 0.1-degree tie band selects positive yaw and the published bias is `min(0.5, FinalFacingTolerance / 2)`.
- [ ] Prove a queued attack currently rereads mutable movement input instead of retaining the physical edge's world-space facing intent.
- [ ] Add targetless-input tests for the closed facing-source order, including nonzero input, movement `Completed`/`Canceled`, invalidated soft target fallback, and no-input preservation of attack-edge facing.
- [ ] Add table-driven red tests for `Disabled`, `Weighted`, `DeadZoneCurve`, and `ConeClamp`, including finite/range rejection, below/at/above magnitude dead zone, analog strength remapping, zero input, input release, response-rate limiting, zero simulation delta, and deterministic exact/near-180 signs.
- [ ] Prove current targeted refresh hard-faces the target and ignores live player input; prove targetless execution has no terminal-aware live steering path.
- [ ] Add late-input and low-budget cases that require a reason-coded reachable clamp plus a return-to-reference reserve.
- [ ] Prove steering cannot alter target identity/translation and cannot reach AI, defense, paired, counter-sync, or finisher alignment owners.
- [ ] Add noisy-input/meaningful-delta cases that fail if the same warp target is rebuilt every frame.
- [ ] Add a world/latent regression that measures actor yaw; request creation or modifier configuration alone is not a behavioral oracle.
- [ ] Add a source assertion rejecting attack-path `SetActorRotation` calls.
- [ ] Build and run `KatanaCombat.Targeting` plus `KatanaCombat.Defense.Alignment.CombatWarp`; record red evidence.

## Task 2: Make Rotation Ownership And Steering Policy Explicit

**Create:**

- `Source/KatanaCombat/Public/Core/AttackAlignmentResolver.h`
- `Source/KatanaCombat/Private/Core/AttackAlignmentResolver.cpp`

**Modify:**

- `Source/KatanaCombat/Public/CombatTypes.h`
- `Source/KatanaCombat/Public/ActionQueueTypes.h`
- `Source/KatanaCombat/Public/Core/TargetingComponent.h`
- `Source/KatanaCombat/Private/Core/TargetingComponent.cpp`
- `Source/KatanaCombat/Public/Core/CombatComponent.h`
- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- `Source/KatanaCombat/Public/Characters/PlayerCharacter.h`
- `Source/KatanaCombat/Private/Characters/PlayerCharacter.cpp`

- [ ] Add `MaximumAutomaticTurn = 180.0f` to `FAttackWarpConfig` with finite validation and a 0-360 clamp; treat it as cumulative attack yaw budget rather than a defense budget.
- [ ] Add `FinalFacingTolerance = 10.0f` to `FAttackWarpConfig`, with finite validation and a 0.1-45-degree clamp.
- [ ] Add `EAttackAlignmentFailurePolicy { BestEffort, RequireReachable }`; existing assets remain `BestEffort` until audited so the source-first commit is usable.
- [ ] Add nested `FAttackRotationSteeringConfig` and closed `EAttackRotationSteeringMode { Disabled, Weighted, DeadZoneCurve, ConeClamp }` with the spec's exact defaults. Validate input-magnitude dead zone 0-0.95, maximum deviation 0-180, enabled response rate 1-1800, weight 0-1, dead zone 0-90, full influence greater than dead zone and at most 180, and exponent 0.1-5.
- [ ] Add immutable `FAttackAlignmentStartContext` carrying attack instance, exact montage instance/section, weak target, source/fallback provenance, base facing intent, by-value warp config, effective damage deadline position, and live-steering admission.
- [ ] Implement world-free preflight and steering resolution in `FAttackAlignmentResolver`. Apply `InputStrength = Clamp((Magnitude - DeadZone) / (1 - DeadZone), 0, 1)`, signed yaw for every policy, the shared positive-yaw antipodal tie-break, response-rate limiting, reference-cone clamp, remaining rate/time/budget clamp, and return-to-reference budget reservation.
- [ ] Add an immutable attack-facing intent whose normalized world direction is authoritative and whose desired yaw/eight-way branch direction are derived; include capture time/provenance and store it on every normal queue entry.
- [ ] Add one terminal-aware live movement/steering sample to `UCombatComponent` with finite magnitude clamped to 0-1, normalized world direction, input serial, and observation time. `APlayerCharacter` forwards raw Move value plus control yaw before movement application; CombatComponent rejects non-finite components, normalizes direction separately, and attack-edge capture snapshots the same record. Clear it on Move `Completed`, `Canceled`, input teardown/unpossession, CombatComponent EndPlay, and owner EndPlay; do not create another PlayerCharacter/Targeting cache or mutate immutable edge intent.
- [ ] Encode the reason-coded facing-source order: explicit/paired/AI target, canonical player soft target, captured nonzero world direction, then captured facing yaw. Preserve the stored player fallback across target invalidation or later retarget decisions.
- [ ] Resolve signed yaw with `FMath::FindDeltaAngleDegrees`; inside the 0.1-degree antipodal band force positive yaw and publish `180 - min(0.5, FinalFacingTolerance / 2)` without a direct rotation fallback.
- [ ] Build the intent once from the same camera-relative sample used at the physical attack edge. Do not reread `LastMovementInput` or rebuild world yaw from a later character rotation during execution.
- [ ] Add a pure attack-alignment preflight result carrying reason, required yaw, available budget, usable time before the effective damage deadline/warp end, and target/translation validity. The effective deadline is the earliest canonical Active/Hit begin or earlier legacy hit-enable notify in the selected section.
- [ ] Build attack `FAlignmentRequestSpec` exclusively from `FAttackWarpConfig`; remove all defense-setting reads from regular attack setup.
- [ ] Bind steering runtime context to the exact regular-attack alignment handle/generation, attack instance, montage instance/section, effective damage deadline, and registered root-motion modifier. No generic actor tick, notify lookup, or raw Enhanced Input polling may drive it.
- [ ] Add a native context-based setup API. Retain the current Blueprint-callable loose setup only as a deprecated adapter forced to `BestEffort` plus `Disabled` steering with a compatibility reason; audit Blueprint references and prohibit that adapter from strict production validation.
- [ ] Reuse existing owner generation, arbitration, modifier registration, and simulation-time play-rate normalization.
- [ ] Keep execution routing on existing compatibility behavior in this commit; prove the new preflight directly.
- [ ] Build/run focused tests and commit source scaffolding: `Add attack alignment preflight`.

## Task 3: Audit And Migrate Active Alignment Data

**Create:**

- `Source/KatanaCombatEditor/Public/Commandlets/Operations/AttackAlignmentMigrationOperation.h`
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackAlignmentMigrationOperation.cpp`
- `Tools/Codex/manifests/attack-alignment.json`

**Modify:**

- `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
- `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`

- [ ] Audit active AttackData, montage sections, required warp target names/windows, canonical Active/Hit begin, surviving legacy hit-enable notifies, effective damage deadlines, usable pre-contact durations, current rates/budgets, steering modes/envelopes, post-modifier `IA_Move`/IMC dead zones, and maximum observed target-yaw cases without saving.
- [ ] Register audit/plan/apply routing and add option/approved-plan/save-gate regression tests before any apply.
- [ ] Commit migration source/tests after a successful no-save audit: `Add attack alignment migration`.
- [ ] Produce an exact package plan. Fix missing reviewed warp configuration before changing policy; assign conservative non-`Disabled` steering only to active unpaired player attacks and retain `Disabled` for paired/sync content.
- [ ] Apply `RequireReachable` only to validated active attacks whose base turn plus steering envelope fits rate/deadline and whose cumulative budget is at least maximum supported base turn plus twice maximum input deviation; leave any unsupported legacy attack in `BestEffort` and treat it as an explicit acceptance blocker.
- [ ] Commit allowlisted assets separately: `Migrate active attack alignment policy`.

## Task 4: Enforce Alignment And Expose Locomotion Rotation

**Modify:**

- `Source/KatanaCombat/Public/Core/TargetingComponent.h`
- `Source/KatanaCombat/Private/Core/TargetingComponent.cpp`
- `Source/KatanaCombat/Public/Core/CombatComponent.h`
- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- `Source/KatanaCombat/Public/Characters/PlayerCharacter.h`
- `Source/KatanaCombat/Private/Characters/PlayerCharacter.cpp`
- `Source/KatanaCombatTest/Private/StateTransitionTests.cpp`

- [ ] Add an editable `LocomotionRotationRate` defaulting to 540 degrees/second.
- [ ] Make explicit AI target and player soft-target paths enforce `RequireReachable` preflight and reject unreachable starts rather than accept partial facing. Player soft-target failure falls back to captured intent; a remaining explicit-intent failure records disposition `Rejected` with reason `UnreachableAlignment` before montage start and cannot be reported as consumed.
- [ ] Update live rotation from the exact registered modifier's update delegate after current animation position/play rate is known and before UE samples the target. Keep target position/reference tracking separate, publish only changes of at least 0.1 degrees or 1 cm, and do not call broad request re-arbitration every frame.
- [ ] Apply live steering only to the active regular player attack. Freeze the last reachable target at the effective damage deadline; clear steering context on exact attack/alignment terminal, montage replacement, death, paired entry, target invalidation fallback, and EndPlay.
- [ ] Keep target selection, translation, attack branch, queue identity, and paired/defense alignment unchanged by live steering.
- [ ] Apply it to `UCharacterMovementComponent::RotationRate` during initialization without changing attack alignment.
- [ ] Test finite/range fallback and prove changing locomotion rate does not change attack request rate/budget.
- [ ] Do not route movement cancellation here; that belongs to Micro-Plan 04.

## Task 5: Validate Data And Runtime

**Modify if needed:** `Source/KatanaCombatEditor/Private/DefenseAssetValidationService.cpp`, `Source/KatanaCombatTest/Private/DefenseAssetValidationTests.cpp`

- [ ] Extend validation for finite attack rotation rate/budget/tolerance, steering mode parameters and envelope, required Motion Warping notify targets, effective damage deadline discovery, legacy hit-enable precedence, and full supported-angle plus held-from-start steering reachability for strict player attacks.
- [ ] Reject strict production paths that use the loose compatibility setup or lack exact attack/montage/deadline identity.
- [ ] Run no-save audit over active AttackData and reject any accepted active attack left in `BestEffort`.
- [ ] PIE-test isolated no-target player attacks at 45, 90, 135, exact 180, and near-antipodal angles, plus terminal-zero/no-input facing preservation and targeted player/AI attacks at 30, 60, 90, and near-budget yaw.
- [ ] For targeted and targetless player attacks, hold left/right input, reverse it once, and release it during the warp. Record actual actor yaw, stable target/translation identity, bounded offset, response rate, reachable clamp, and return toward base; repeat with each authored steering mode represented by a test asset or transient fixture.
- [ ] Change input just before the damage deadline and prove the contribution is clamped rather than snapping or making the attack unreachable. Inject noisy below-dead-zone samples and prove no target churn.
- [ ] Rotate the camera while holding input and prove live world steering updates while queued/base intent remains unchanged. Prove diagonal digital input clamps to full strength, non-finite samples clear safely, and partial analog input retains graded influence. Pause/hitstop one warp and prove zero target advance or resume jump.
- [ ] Repeat input changes during AI, paired, counter-sync, and defense alignment and prove zero steering contribution.
- [ ] Repeat no-target cases after queue delay and after camera/character movement changes; prove the original world-space edge remains authoritative.
- [ ] Interrupt one attack externally and prove only that attack's alignment ends; let one uninterrupted attack finish and record actual actor-yaw error at first contact and warp end.
- [ ] Run full `KatanaCombat.Targeting`, `KatanaCombat.Defense.Alignment`, and attack execution tests.
- [ ] Commit source integration: `Separate attack and defense rotation limits`.

**Gate:** Attack alignment and live steering must pass without changing target identity/translation, paired or defense behavior, or directly setting actor rotation.
