# Micro-Plan 06: Animation Layering And Content Migration

**Spec:** `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization/06-animation-layering-and-content-migration.md`

**Goal:** Add non-root-motion guard/additive lanes, project-owned derived assets, and deterministic 3x3 reaction presentation without changing gameplay policy.

## Task 1: Add Source-Side Presentation Contracts And Validators

**Modify:**

- `Source/KatanaCombat/Public/Data/HitReactionSettings.h`
- `Source/KatanaCombat/Private/Data/HitReactionSettings.cpp`
- `Source/KatanaCombat/Public/Animation/SamuraiAnimInstance.h`
- `Source/KatanaCombat/Private/Animation/SamuraiAnimInstance.cpp`
- `Source/KatanaCombatEditor/Private/DefenseAssetValidationService.cpp`
- `Source/KatanaCombatTest/Private/DefenseAssetValidationTests.cpp`

- [ ] Add sparse 3x3 additive presentation rows keyed by existing height/lane and deterministic fallback.
- [ ] Expose only animation-facing guard active, reaction class/key, and attack-safe mask state.
- [ ] Validate slot/group topology, non-root-motion overlays, additive compatibility, fallback presence, finite blends, and writable package ownership. Missing layered assets are informational in `Legacy` and blocking in `DeterministicLayered`.
- [ ] Add tests before changing assets; record current expected failures.
- [ ] Commit source/tests: `Add layered combat presentation contracts`.

## Task 2: Add Migration Support And Audit Without Saving

**Create:**

- `Source/KatanaCombatEditor/Public/Commandlets/Operations/ActionReactionAnimationOperation.h`
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/ActionReactionAnimationOperation.cpp`
- `Tools/Codex/manifests/action-reaction-animation.json`

**Modify:**

- `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
- `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`

**Read/Audit:**

- `Content/ProjectFiles/Animation/ABP_SamuraiCharacter.uasset`
- `Content/ProjectFiles/Animation/ABP_Manny_Combat.uasset`
- `Content/ProjectFiles/Animation/Montages/Defense/GateA/*.uasset`
- `Content/ProjectFiles/Animation/Montages/HitReactions/**/*.uasset`
- `Content/ProjectFiles/Data/PDA/HitReaction/HitReactionSettings/DA_HitReaction.uasset`
- `Content/ProjectFiles/Data/PDA/Settings/DA_CombatSettings_Default.uasset`
- `Content/ProjectFiles/Data/PDA/Defense/GateB/DA_CombatSettings_DefenseMatrix.uasset`

- [ ] Register audit/plan/apply routing and test fingerprint binding, dirty-package rejection, loaded-package safety, and exact save-set enforcement.
- [ ] Report actual skeletal mesh/skeleton, slot groups, montage tracks, root-motion flags, additive type/reference pose, source sequences, notifies, sections, and current asset consumers.
- [ ] Resolve the exact skeleton package that must own new slots and place it in the generated allowlist.
- [ ] Fail audit if either dirty map/external-actor path enters the save set.
- [ ] Do not run apply while an interactive Editor process has any target package loaded. Close the Editor or use the reviewed interactive save path, then re-run preflight hashes.
- [ ] Review the report and commit migration source/tests before apply: `Add action reaction animation migration`.

## Task 3: Create Project-Owned Guard Assets

**Create through reviewed apply operation:**

- `Content/ProjectFiles/Animation/Defense/ActionReaction/AS_Guard_Additive.uasset`
- `Content/ProjectFiles/Animation/Defense/ActionReaction/AM_Guard_Additive.uasset`
- `Content/ProjectFiles/Animation/Defense/ActionReaction/AM_BlockImpact_Additive.uasset`

- [ ] Duplicate reviewed source animation into project-owned paths and configure Local Space additive reference.
- [ ] Add `GuardOverlay.Guard` to the audited project skeleton.
- [ ] Ensure no root motion and no gameplay notifies are copied unintentionally.
- [ ] Preserve existing distinct perfect-parry/attacker bridge assets.

## Task 4: Create The Additive Reaction Matrix

**Create through reviewed apply operation:**

- `Content/ProjectFiles/Animation/HitReactions/Additive/AS_Flinch_High_Left.uasset`
- `Content/ProjectFiles/Animation/HitReactions/Additive/AS_Flinch_High_Center.uasset`
- `Content/ProjectFiles/Animation/HitReactions/Additive/AS_Flinch_High_Right.uasset`
- `Content/ProjectFiles/Animation/HitReactions/Additive/AS_Flinch_Middle_Left.uasset`
- `Content/ProjectFiles/Animation/HitReactions/Additive/AS_Flinch_Middle_Center.uasset`
- `Content/ProjectFiles/Animation/HitReactions/Additive/AS_Flinch_Middle_Right.uasset`
- `Content/ProjectFiles/Animation/HitReactions/Additive/AS_Flinch_Low_Left.uasset`
- `Content/ProjectFiles/Animation/HitReactions/Additive/AS_Flinch_Low_Center.uasset`
- `Content/ProjectFiles/Animation/HitReactions/Additive/AS_Flinch_Low_Right.uasset`
- `Content/ProjectFiles/Animation/HitReactions/Additive/AM_Flinch_Additive.uasset`

- [ ] Build one reviewed section per 3x3 cell and configure deterministic row references in `DA_HitReaction`.
- [ ] Add `AdditiveReaction.Reaction` to the same audited skeleton.
- [ ] Keep existing full-body hit montages on `DefaultGroup.DefaultSlot`.

## Task 5: Compose AnimBP Layers

**Modify through allowlisted apply:**

- `Content/ProjectFiles/Animation/ABP_SamuraiCharacter.uasset`
- `Content/ProjectFiles/Animation/ABP_Manny_Combat.uasset` only if audit proves it is an active runtime AnimBP
- exact audited skeleton package

- [ ] Compose locomotion base, Guard overlay, AdditiveReaction overlay, and existing primary full-body slot.
- [ ] Create/use an attack-safe blend mask that excludes the weapon-driving chain during active traces.
- [ ] Keep overlay slots updating source locomotion and disable overlay root motion.
- [ ] Compile and validate every modified Blueprint/package before save.
- [ ] Switch both active CombatSettings assets to `DeterministicLayered` only after every referenced pressure/selector/animation asset passes strict validation.

## Task 6: Prove Then Commit Assets

- [ ] Run commandlet audit after apply and prove only allowlisted project packages changed.
- [ ] Run compatibility tests once in `Legacy` and the complete suite in `DeterministicLayered`; both modes must remain crash-safe.
- [ ] Run asset validators and full editor build/tests.
- [ ] PIE-capture moving guard with no sliding, distinct normal block/parry, nine selection cells, additive trade, heavy full-body interruption, recovery, and weapon socket drift.
- [ ] Reject any additive asset exceeding the measured socket-drift tolerance; adjust mask/asset, not trace logic.
- [ ] Inspect LFS pointers and stage exact assets.
- [ ] Commit: `Add layered guard and hit reaction animations`.

**Gate:** Asset structure is not accepted without visible PIE proof; visual proof is not accepted without validator and allowlist evidence.
