# Defense Interaction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the accepted guard, normal-block, perfect-parry, attacker-response, alignment, and retained Chain Counter contracts, then prove them first with one reviewed attack and finally with the complete defense matrix.

**Architecture:** Preserve the five gameplay components. `UCombatComponent` owns defense decisions and state, `ABaseCombatCharacter` owns the native rich-contact commit boundary, `UWeaponComponent` owns contact transport and post-receipt hit accounting, `UTargetingComponent` owns alignment execution, `UHitReactionComponent` owns selected presentation, and `UPairedAnimationComponent` owns retained paired stages. `FDefenseResolver` is pure. `UCombatEffectsWorldSubsystem` owns only time-dilation leases and is not a gameplay component.

**Tech Stack:** Unreal Engine 5.6 C++, Gameplay Tags, Motion Warping, Enhanced Input, StateTree, Unreal Automation tests, Katana asset-migration commandlet, PowerShell, Editor/PIE evidence.

**Authority:** Implement against `docs/superpowers/specs/2026-07-16-defense-interaction-design.md`. Current source is the compatibility baseline, not the target behavior. When this plan and the accepted spec differ, stop and reconcile the documents before code changes.

## Global Constraints

- Work in the seven slices below and keep each slice independently buildable and testable.
- Do not create a sixth gameplay actor component or move defense decisions into `UTargetingComponent`, `UWeaponComponent`, `UHitReactionComponent`, AnimBP, StateTree, or delegates.
- Keep shared reflected contracts and cross-component delegates in `Source/KatanaCombat/Public/CombatTypes.h`. Keep pure policy in `Source/KatanaCombat/Public/Defense/` and `Private/Defense/`.
- Preserve generic `IDamageableInterface` compatibility. Rich defense guarantees apply only to `ABaseCombatCharacter` contacts.
- Do not let the rich-contact path call legacy `ApplyDamage`, `IsBlocking`, or `CanBlockHit` after resolution.
- Never use `SetActorRotation` or an uncapped Motion Warping modifier for defense alignment. One owned alignment request may rotate an actor in a frame.
- Use simulation time for animation/deadline work and unscaled time for response windows, tombstones, hitstop restoration, and watchdogs.
- Capture every input edge before eligibility checks. A failed `ChainOnly` edge expires and never reaches the normal attack queue.
- Treat `Attack.Property.Unblockable`, `Attack.Defense.Parryable`, and `Attack.Defense.BlockInterruptible` as semantic tags, not animation promises.
- No broad notify seeding, global content scan, blanket package save, or unreviewed montage timing. Stage files by explicit path only.
- Headless tests can prove contracts and asset structure. Only Editor/PIE plus telemetry can prove animation quality, alignment, continuity, or visible Gate A/Gate B acceptance.
- Keep AC3 counter tests as legacy-mode coverage. They cannot satisfy Chain acceptance and are not removed unless a later accepted design sunsets AC3.
- Do not start a UI pass. Use logs, debug drawing, and structured telemetry until gameplay contracts are stable.

## Empirical Starting Point

- Block input currently attempts `UPairedAnimationComponent::TryCounter()` before it is recorded, then calls `BeginBlock()` or `EndBlock()` synchronously.
- A failed Chain attack preflight currently falls through into the normal action queue.
- `BeginBlock()` performs a one-shot direct `SetActorRotation`; it has no stable threat lock or alignment owner.
- `UWeaponComponent::ProcessHit()` adds a target to `HitActors` before `OnWeaponHit`, so ignored contacts consume dedupe and `MaxHitCount` budget.
- `ABaseCombatCharacter::OnWeaponHitTarget()` and `ApplyDamage_Implementation()` independently recompute team, invulnerability, and block state.
- Chain currently moves from `ParryActive` to `CounterWindow` in the same call, has no `FinisherActive`, and tears down shared state between stages.
- Paired code and cinematic utilities use raw time-dilation restore calls that can clobber overlapping owners.
- The accepted asset baseline contains no reviewed parry window among the 20 audited attacks. `LightAttack_1` has counter/finisher references but no parry timing; HeavyAttack_1 through HeavyAttack_4 retain separate notify debt.

## Planned File Layout

Add focused runtime files:

- `Source/KatanaCombat/Public/Defense/DefenseResolver.h`
- `Source/KatanaCombat/Private/Defense/DefenseResolver.cpp`
- `Source/KatanaCombat/Public/Defense/DefensePresentationSelector.h`
- `Source/KatanaCombat/Private/Defense/DefensePresentationSelector.cpp`
- `Source/KatanaCombat/Public/Data/DefenseConfiguration.h`
- `Source/KatanaCombat/Private/Data/DefenseConfiguration.cpp`
- `Source/KatanaCombat/Public/Subsystems/CombatEffectsWorldSubsystem.h`
- `Source/KatanaCombat/Private/Subsystems/CombatEffectsWorldSubsystem.cpp`
- `Source/KatanaCombat/Public/Animation/AnimNotify_ChainStageTransition.h`
- `Source/KatanaCombat/Private/Animation/AnimNotify_ChainStageTransition.cpp`
- `Source/KatanaCombat/Public/Animation/CombatAnimNotifyIdentity.h`
- `Source/KatanaCombat/Private/Animation/CombatAnimNotifyIdentity.cpp`
- `Source/KatanaCombat/Public/Debug/DefenseTelemetry.h`
- `Source/KatanaCombat/Private/Debug/DefenseTelemetry.cpp`

Add focused automation files instead of expanding the already large legacy suites:

- `Source/KatanaCombatTest/Private/DefenseResolverTests.cpp`
- `Source/KatanaCombatTest/Private/DefensePresentationSelectorTests.cpp`
- `Source/KatanaCombatTest/Private/DefenseContactTests.cpp`
- `Source/KatanaCombatTest/Private/DefenseInputThreatTests.cpp`
- `Source/KatanaCombatTest/Private/DefenseAlignmentTests.cpp`
- `Source/KatanaCombatTest/Private/DefenseParryTests.cpp`
- `Source/KatanaCombatTest/Private/CombatAnimNotifyIdentityTests.cpp`
- `Source/KatanaCombatTest/Private/DefenseChainTests.cpp`
- `Source/KatanaCombatTest/Private/CombatEffectsLeaseTests.cpp`
- `Source/KatanaCombatTest/Private/DefenseArchitectureSourceTests.cpp`
- `Source/KatanaCombatTest/Private/DefenseAssetValidationTests.cpp`

Maintain one durable execution checkpoint throughout implementation:

- `docs/handoffs/2026-07-16-defense-interaction-execution.md`

The checkpoint is not an acceptance report. It records the current commit, authority version, completed and active work, changed files/assets, verification evidence, unresolved findings, proof gaps, and exact next action so work can resume safely after compaction or a worker/session change.

Extend the existing editor framework:

- `Source/KatanaCombatEditor/Public/DefenseAssetValidationService.h`
- `Source/KatanaCombatEditor/Private/DefenseAssetValidationService.cpp`
- `Source/KatanaCombatEditor/Public/Commandlets/Operations/DefenseProofMigrationOperation.h`
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/DefenseProofMigrationOperation.cpp`
- `Tools/Codex/manifests/defense-gate-a.json`
- `Tools/Codex/manifests/defense-gate-b.json`

## Spec-To-Slice Coverage

| Accepted contract | Owning slice |
|---|---|
| Current-source/API/content feasibility and execution checkpoint | 0A |
| IDs, immutable prediction/contact records, reasoned outcome matrix, tags, sparse selector | 1 |
| Native rich contact, in-progress/cached receipts, team/invulnerability, weapon budget | 2 |
| Input history/routes, threat publication/lock, alignment arbitration, normal block | 3 |
| Window generations, perfect-parry downgrade, attack consume, AI token, bridge preflight | 4 |
| Retained Chain stages, context/time/collision/input ownership, marker handoff, rollback | 5 |
| Commandlet/preflight, concrete `LightAttack_1` content, telemetry, visible vertical slice | 6 |
| Nine height/lane cells, semantic attack cases, two-active-threat fixture, broad acceptance | 7 |
| Context refresh, adversarial review, evidence classification, and intent traceability | Every slice; final closure in 7 |

Event ordering/reentrancy is introduced in Slice 2 and re-proved for attack consumption and paired callbacks in Slices 4-5. Documentation state is updated only after the corresponding Gate evidence exists.

## Slice Acceptance Rule

For every slice:

1. Run the context-refresh preflight below and confirm that the accepted spec, current source, prior evidence, and working tree still support the slice assumptions.
2. Revalidate every risky project or UE 5.6 API used by the slice against current primary source. Stop and reconcile the spec and plan when an assumption is unsupported or has drifted.
3. Add the smallest failing automation test first. Add only compile-safe API scaffolding needed to load the test; scaffolding must return an intentionally incorrect neutral result rather than implement policy.
4. Build `KatanaCombatEditor` so the new test is present, then run the focused test and record the behavioral failure. If the deliberate red state is a compiler/linker failure, record the failing `Build.bat` output and do not run a stale `UnrealEditor-Cmd` binary.
5. Implement only the slice contract, build `KatanaCombatEditor`, then run the focused test roots.
6. Run the slice validation ladder, including applicable static/source checks, commandlet checks, asset checks, and Editor/PIE proof. Keep source, asset, and runtime evidence distinct.
7. Perform the slice adversarial review below. Fix every high/medium finding and add a regression test before continuing; document justified low findings and proof limits.
8. Trace every contract assigned to the slice back to the accepted spec and classify it as `Proven`, `Partial`, `Not Implemented`, or `Out Of Scope`. `Partial` and `Not Implemented` block slice acceptance when the contract is in scope.
9. Inspect `git diff --check`, the complete task diff, and `git status --short`. Classify unrelated user WIP without modifying it.
10. Update the durable execution checkpoint, then commit only that slice with an imperative message.

Do not continue when a focused regression is unexplained. Do not weaken a test merely to preserve current behavior that the accepted spec replaces.

### Context-Refresh Preflight

Run this before Task 0A, before every implementation slice, after any compaction/session/worker transition, and whenever source, assets, or branch state changes unexpectedly:

```powershell
git status --short --branch
git log -3 --oneline
git diff --name-status HEAD~1..HEAD
```

Then read the current task in this plan, its owning sections in the accepted defense spec, the narrowest architecture references, the previous checkpoint entry, and the exact source/assets being changed. Re-run narrow searches instead of trusting remembered line numbers or behavior. Confirm:

- Current `HEAD`, branch, dirty-worktree classification, and user WIP boundaries.
- Last completed slice and its build/test/commandlet/PIE evidence.
- Unresolved audit findings, accepted proof limits, and whether any dependency API changed.
- Files and assets already changed, files/assets this slice may change, and the exact next action.

Update `docs/handoffs/2026-07-16-defense-interaction-execution.md` before a long-running operation, before pausing, and immediately after completing a slice. A resumed worker must treat that file as orientation evidence, verify it against live Git/source state, and correct stale entries before editing.

### Slice Adversarial Review

Review the implemented slice as a hostile caller and an interrupted lifecycle. At minimum challenge duplicate/reentrant calls, stale generations, participant destruction, partial start/rollback, owner-only lease release, overlapping owners, mutable second reads, clock-domain drift, null/missing assets, configuration fallback, input fallthrough, source/target invalidation, proof false positives, and claims based on the wrong evidence tier. Also ask what can bypass the new boundary through a legacy API, Blueprint path, notify callback, StateTree task, generic damage path, or direct transform/time mutation.

The reviewer must compare the actual diff and tests to the spec, not only to this plan. Record each finding with severity, evidence, disposition, and regression proof in the execution checkpoint. No high/medium finding may be deferred merely to keep the slice moving.

---

### Task 0: Establish The Execution Baseline

**Files:**
- Read: `CLAUDE.md`
- Read: `docs/architecture/ARCHITECTURE_QUICK.md`
- Read: `docs/specs/PAIRED_ANIMATION_SPEC.md`
- Read: `docs/superpowers/specs/2026-07-16-defense-interaction-design.md`
- Read: `Source/KatanaCombatTest/README.md`

**Produces:** A clean, reproducible pre-implementation evidence point and an explicit dirty-worktree classification.

- [x] **Step 1: Confirm branch and workspace scope**

```powershell
git status --short --branch
git log -3 --oneline
```

Expected: the accepted design commits are present. Classify any later user WIP before touching overlapping files. Do not revert it.

- [x] **Step 2: Run the standard baseline**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "Tools\Codex\run-agent-baseline.ps1"
```

Expected: `BASELINE GREEN`, a successful editor build, and zero automation failures/errors in the timestamped `Saved/Logs/Codex-Agent-Baseline-*` evidence. If memory pressure blocks the build, rerun the build separately with UBT parallelism capped and record that deviation; do not call an incomplete run green.

- [x] **Step 3: Capture architecture hazards as source assertions**

Record current matches without modifying them yet:

```powershell
rg -n "SetActorRotation|ApplyDamage_Implementation|CanBlockHit|HitActors.Add|RestoreTimeDilation|ParryActive|CounterWindow" Source/KatanaCombat
```

Expected: matches identify the migration sites described in the empirical starting point. Later source-gate tests will narrow these searches to prohibited defense paths.

---

### Task 0A: Research And Feasibility Audit

**Files:**
- Read: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- Read: `Source/KatanaCombat/Private/Core/WeaponComponent.cpp`
- Read: `Source/KatanaCombat/Private/Core/TargetingComponent.cpp`
- Read: `Source/KatanaCombat/Private/Core/HitReactionComponent.cpp`
- Read: `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`
- Read: `Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp`
- Read: `Source/KatanaCombat/Private/Animation/AnimNotifyState_CombatWarp.cpp`
- Read: `Source/KatanaCombat/Public/Animation/AnimNotifyState_PairedAnimationCollision.h`
- Read: `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
- Read: `Source/KatanaCombatEditor/Private/Commandlets/Operations/EnemyAIProofAssetsOperation.cpp`
- Add: `docs/handoffs/2026-07-16-defense-interaction-execution.md`

**Produces:** A version-pinned feasibility matrix, refreshed source/asset baseline, resolved API assumptions, initial execution checkpoint, and an explicit stop/go decision for Slice 1. It changes no runtime code or assets.

- [x] **Step 1: Run the context-refresh preflight and initialize the checkpoint**

Record branch/`HEAD`, accepted spec and plan commits, dirty-worktree classification, baseline evidence paths, current source hazards, current asset proof paths, and `Task 1` as the proposed next action. Do not copy stale evidence without reopening it.

- [x] **Step 2: Verify UE 5.6 APIs against installed engine source**

Use the installed `C:\Program Files\Epic Games\UE_5.6\Engine` source as primary evidence. Verify notify montage-instance context and a runtime-safe notify-event identity (including why editor notify GUIDs are unavailable), `UMotionWarpingComponent::AddModifierFromTemplate`, runtime modifier update hooks and effective play rate, `UCharacterMovementComponent` rotation/tick ordering, `UAnimMontage::ExtractRootMotionFromTrackRange`, `FTSTicker`, and package-save APIs. Record exact header/source paths and signatures. Use official Epic 5.6 documentation only when local source is insufficient, and record any version mismatch.

- [x] **Step 3: Trace the current project call paths end to end**

Trace Block input, attack prediction inputs, phase-driven hit detection, weapon candidate generation, target damage/health/death mutation, presentation/delegate broadcasts, AI token ownership, paired start/cleanup, collision/movement restoration, warp-target clearing, and time-dilation restoration. Identify every synchronous callback and every legacy path capable of bypassing the proposed owner.

- [x] **Step 4: Audit existing proof and migration infrastructure**

Verify report-row cardinality, dirty-package snapshots, save ordering, external-actor handling, and whether a reviewed Plan artifact constrains Apply/ApplyAndSave. Inventory what `EnemyAIProofAssets` already owns for StateTree, AI controller, `IA_Block`, `IMC_Combat`, player Blueprint, enemy Blueprint, and `Lvl_ThirdPerson1`; the defense operation must reuse or validate that ownership rather than duplicate it.

- [x] **Step 5: Run read-only Gate A asset inventory**

Confirm the exact `LightAttack_1`, `AM_Light_Combo_1` section `Attack_1`, `Lvl_ThirdPerson1`, input assets, proof actors, paired dependencies, current notifies, and external actor packages. Do not mutate, resave, or infer timing. Record unavailable live/visual facts as proof gaps requiring Editor/UEMCP review later.

- [x] **Step 6: Write the feasibility matrix**

For every risky assumption, record `Assumption`, `Primary Evidence`, `Observed API/Behavior`, `Plan Impact`, `Status`, and `Required Change`. Status is one of `Supported`, `Supported With Constraint`, `Unsupported`, or `Needs Editor Evidence`. Any `Unsupported` architecture assumption or unresolved implementation-blocking ambiguity requires a spec/plan reconciliation before Task 1.

- [x] **Step 7: Run an adversarial feasibility review**

Challenge the proposed boundaries for reentrancy, actor destruction, cross-stage first-commit ordering, selected-payload immutability, collision/input/movement overlap, telemetry survival, play-rate changes, test determinism, commandlet approval drift, and false headless confidence. Add missing research and regression obligations to the plan before declaring `GO`.

- [x] **Step 8: Record the stop/go decision**

Update the checkpoint with all findings and exact required plan/spec edits. `GO` requires no unresolved high/medium feasibility finding and a concrete test/proof route for every Slice 1 contract. Commit the initial checkpoint as a docs-only commit such as:

```powershell
git add docs/handoffs/2026-07-16-defense-interaction-execution.md
git commit -m "Record defense implementation feasibility"
```

---

### Task 1: Add Typed Contracts, Pure Resolution, And Deterministic Selection

**Files:**
- Modify: `Source/KatanaCombat/Public/CombatTypes.h`
- Modify: `Source/KatanaCombat/Private/CombatTypes.cpp`
- Modify: `Source/KatanaCombat/Public/Data/AttackData.h`
- Modify: `Source/KatanaCombat/Public/Utilities/CombatGameplayTags.h`
- Modify: `Source/KatanaCombat/Private/Utilities/CombatGameplayTags.cpp`
- Modify: `Config/DefaultGameplayTags.ini`
- Add: `Source/KatanaCombat/Public/Data/DefenseConfiguration.h`
- Add: `Source/KatanaCombat/Private/Data/DefenseConfiguration.cpp`
- Add: `Source/KatanaCombat/Public/Defense/DefenseResolver.h`
- Add: `Source/KatanaCombat/Private/Defense/DefenseResolver.cpp`
- Add: `Source/KatanaCombat/Public/Defense/DefensePresentationSelector.h`
- Add: `Source/KatanaCombat/Private/Defense/DefensePresentationSelector.cpp`
- Add: `Source/KatanaCombatTest/Private/DefenseResolverTests.cpp`
- Add: `Source/KatanaCombatTest/Private/DefensePresentationSelectorTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/GameplayTagContractTests.cpp`

**Consumes:** Existing `FAttackStateMachine::AttackGeneration`, `FHitReactionInfo`, attack tags, montage/section state, and `UPairedAnimationData` references.

**Produces:** Closed defense enums, stable IDs, immutable query/decision/resolution records, attack defense profiles, sparse presentation configuration, pure resolver APIs, and deterministic selector APIs. No production contact or input path uses them yet.

- [ ] **Step 1: Add the closed enums and identity value types**

Add to `CombatTypes.h`:

```cpp
UENUM(BlueprintType) enum class EDefenseQueryStage : uint8 { InputIntent, Contact };
UENUM(BlueprintType) enum class EDefenseOutcome : uint8
{
	Rejected, GuardEntered, PerfectParry, NormalBlock, Hit, UnblockableHit,
	IgnoredFriendly, IgnoredInvulnerable, IgnoredConsumed, IgnoredInvalid
};
UENUM(BlueprintType) enum class EDefenseDamageDisposition : uint8
{
	ApplyRequestedDamage, SuppressDamage, NoContactSideEffects
};
UENUM(BlueprintType) enum class EAttackerResponse : uint8 { None, Continue, Recoil, ParryStagger };
UENUM(BlueprintType) enum class EDefenseCommitStatus : uint8
{
	NewCommit, Cached, InProgress, RejectedBeforeRegistration
};
UENUM(BlueprintType) enum class EDefensePredictionConfidence : uint8 { None, Low, High };
UENUM(BlueprintType) enum class EAttackHeight : uint8 { High, Middle, Low };
UENUM(BlueprintType) enum class EIncomingAttackLane : uint8 { Left, Center, Right };
UENUM(BlueprintType) enum class EAttackWindowKind : uint8 { Hit, Parry, Counter };
UENUM(BlueprintType) enum class EAttackConsumeReason : uint8 { PerfectParry, Death, PairedTakeover, Cancelled };
UENUM(BlueprintType) enum class EDefenseAlignmentPolicy : uint8
{
	None, GuardFacing, BlockContact, PerfectParryBridge
};
UENUM(BlueprintType) enum class EDefenseReason : uint8
{
	None, InvalidDefenderState, NoHostileCandidate, StaleAttack, NoParryWindow,
	MissingParryCapability, PredictionInsufficient, OutsideHardCone,
	PerfectAlignmentUnreachable, NotGuarding, OutsideBlockTolerance, Unblockable,
	FriendlyFireDisabled, Invulnerable, Consumed, InvalidParticipant,
	Duplicate, CommitInProgress
};
```

Use explicit `USTRUCT` value types for `FCombatantStableId`, `FAttackInstanceId`, `FAnimNotifyRuntimeSourceId`, `FAttackWindowInstanceId`, `FWeaponTraceInstanceId`, `FContactInstanceId`, `FDefenseInteractionKey`, and `FDefenseInteractionId`. Implement `IsValid`, equality, and `GetTypeHash`; never hash pointer addresses as an ordering key. `FAnimNotifyRuntimeSourceId` contains the source animation `FSoftObjectPath` and the exact event index in `UAnimSequenceBase::Notifies`. Resolve that index by comparing the event-reference pointer only while locating its owning array element; never retain, hash, sort, or serialize the pointer. `FAttackWindowInstanceId` includes attack ID, kind, window generation, runtime notify source ID, montage-instance ID, simulation start, and simulation end. The montage-instance ID comes from `UE::Anim::FAnimNotifyMontageInstanceContext`, not from an asset-global notify object. A missing source/index or montage-instance context is a production-notify rejection with telemetry; only explicitly deprecated compatibility adapters may omit it.

- [ ] **Step 2: Add immutable prediction, query, decision, and receipt records**

Add the accepted fields to these reflected structs:

```cpp
struct FAttackThreatPrediction;
struct FPredictedDefenseContact;
struct FActualDefenseContact;
struct FAttackExecutionSnapshot;
struct FDefenseThreatSelectionContext;
struct FDefenseThreatSelectionResult;
struct FDefenseQuery;
struct FDefenseDecision;
struct FDefensePresentationPayload;
struct FDefenseResolution;
struct FDefenseContactRequest;
struct FDefenseContactReceipt;
struct FDefenseSequenceContext;
```

Keep source bearing and incoming trajectory as separate vectors. Keep predicted and actual contacts as separate records. `FDefenseDecision` contains no montage, sound, VFX, hitstop, or mutable handle. `FDefenseResolution` contains the selected payload and immutable IDs but not live alignment/time/context lease handles. `FDefenseContactReceipt` adds commit status, applied damage, `bAcceptsWeaponHit`, and `bConsumesHitBudget`.

A null-`AttackData` compatibility snapshot carries only `FWeaponTraceInstanceId` dedupe identity. It can resolve contact geometry and normal block but cannot open a parry, provide authored defense tags, or fabricate an attack generation.

Threat comparison is exact and ordered: credible hostile intent, earliest non-negative contact/window deadline, reachable alignment, confidence, absolute yaw, distance, then stable ID. The comparator must remain a strict weak ordering and may not consult mutable state while sorting.

Add native deferred event records and delegates:

```cpp
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAttackConsumedNative, const FAttackConsumedEvent&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDefenseResolvedNative, const FDefenseResolution&);
```

- [ ] **Step 3: Add attack authoring fields and semantic tags**

Add `FDefenseAttackProfile` to `AttackData.h` with `Height`, `NominalLane`, `SwingShape`, `SourceContactSocketOverride`, `DefenderTargetBoneFallback`, and optional blocked-impact audio/VFX overrides. Add one `DefenseProfile` property to `UAttackData`; adapt `DefaultContactBone` as the fallback rather than creating a second competing bone value.

Register and expose:

```cpp
FGameplayTag KatanaCombatGameplayTags::AttackDefenseParryable();
FGameplayTag KatanaCombatGameplayTags::AttackDefenseBlockInterruptible();
```

Add `Attack.Defense.Parryable` and `Attack.Defense.BlockInterruptible` to `DefaultGameplayTags.ini`. Extend `GameplayTagContractTests.cpp` to require all five semantic tags, including the existing unblockable and context tags.

- [ ] **Step 4: Add `UDefenseConfiguration` and sparse table contracts**

Define the provisional defaults from the accepted spec, including 70-degree hard cone/max turn, 180 deg/s turn rate, 35-degree block tolerance, 10-degree parry tolerance, 0.15-second lock, 0.10-second switch lead, 0.05-second refresh, 0.10-second max high-confidence prediction age, 1000 cm range, 1.0-second tombstone, 128 terminal records, 0.15-second no-montage bridge, 2.0-second CounterWindow/FinisherReady, and 10-second lease watchdog.

Use one `UPrimaryDataAsset` with explicit field names so runtime, validation, and telemetry share terminology:

```cpp
float HardGuardConeHalfAngle = 70.0f;
float MaximumAutomaticTurn = 70.0f;
float DefenseTurnRate = 180.0f;
float NormalBlockFinalTolerance = 35.0f;
float PerfectParryFinalTolerance = 10.0f;
float CenterLaneHalfAngle = 12.0f;
float ThreatLockMinSeconds = 0.15f;
float ThreatSwitchLeadSeconds = 0.10f;
float GuardedThreatRefreshSeconds = 0.05f;
float MaximumHighConfidencePredictionAge = 0.10f;
float DefenseThreatRange = 1000.0f;
float GuardManualOverrideThreshold = 0.25f;
float GuardAutoFacingResumeSeconds = 0.10f;
float InteractionTombstoneSeconds = 1.0f;
int32 TerminalInteractionCacheCap = 128;
float NoMontageParryBridgeSeconds = 0.15f;
float CounterWindowSeconds = 2.0f;
float FinisherReadySeconds = 2.0f;
float TimeDilationLeaseWatchdogSeconds = 10.0f;
```

Also store zero-centimeter authored normal-block translation allowance, 1 cm numerical drift tolerance, and 75 cm perfect-parry per-role translation cap.

Add explicit defender bone-to-height rows, optional guard enter/exit montage references, default block/parry impact VFX and audio, and generic defender/attacker fallback rows. Bone rows are data, not a second attack-height authority: resolution checks exact hit bone, supplied skeleton-parent chain, then the attack profile's authored height while preserving provenance.

Define exact-or-wildcard presentation keys, required/excluded tags, integer priority, blend/warp/FX/hitstop/contact payload, reviewed deflection marker, paired bridge reference, and row name. Add `IsGenericFallback()` and specificity helpers. Tie after exact-field count, required-tag count, and priority is invalid authoring; lexical row name is runtime safety only. A null configuration returns C++ gameplay defaults and no-montage presentation without dereferencing assets.

- [ ] **Step 5: Write failing table-driven resolver tests**

In `DefenseResolverTests.cpp`, table every input and contact matrix row. The test table must include unblockable-plus-parryable, null `AttackData`, stale identity, hard-cone failure, reachable normal but unreachable perfect alignment, friendly, invulnerable, consumed, not guarding, contact outside tolerance, and normal block.

Representative assertion:

```cpp
const FDefenseDecision Decision = FDefenseResolver::Resolve(Query);
TestEqual(TEXT("Outcome"), Decision.Outcome, Case.ExpectedOutcome);
TestEqual(TEXT("Damage disposition"), Decision.DamageDisposition, Case.ExpectedDamage);
TestEqual(TEXT("Attacker response"), Decision.AttackerResponse, Case.ExpectedResponse);
```

Add deterministic threat tests with explicit `FCombatantStableId` values, equal deadlines/yaw/distance, and reversed candidate-array order. Both orders must select the same stable ID. Add reachability boundary tests for `abs(Y) == H`, `abs(Y) == F + AvailableTurn`, zero/negative deadline, and cumulative max-turn exhaustion.

Add direction tests proving source bearing drives the cone while incoming trajectory drives defender-relative lane. Actual lane precedence is nonzero weapon velocity, accepted trace segment, then an explicitly flagged low-confidence nominal-lane fallback. Exact hit bone, mapped parent, and authored height remain distinct provenance values.

- [ ] **Step 6: Write failing selector tests**

In `DefensePresentationSelectorTests.cpp`, prove exact fields outrank wildcards, required-tag count outranks priority, excluded tags reject, equal ranking reports ambiguity, lexical fallback is deterministic, attacker response reads the attacker's configuration, missing config returns no montage, and bridge usability fallback never changes `EDefenseOutcome::PerfectParry`.

- [ ] **Step 7: Run the tests to confirm the intended red state**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="KatanaCombat.uproject" -Progress -NoHotReload
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Defense.Resolver;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Before this step, add compile-safe resolver/selector declarations and neutral stubs only as needed for the test module to link. Expected: the editor build succeeds and the focused test fails on the unimplemented behavior. If the first red state is instead a deliberate compiler/linker failure, the failing build is the evidence; do not invoke `UnrealEditor-Cmd` until the test has been compiled into a fresh binary. Record the specific missing behavior, then implement.

- [ ] **Step 8: Implement pure resolution and selection**

Expose only pure/static APIs:

```cpp
class KATANACOMBAT_API FDefenseResolver
{
public:
	static FDefenseThreatSelectionResult SelectThreat(
		const TArray<FAttackExecutionSnapshot>& Candidates,
		const FDefenseThreatSelectionContext& Context);
	static FDefenseDecision Resolve(const FDefenseQuery& Query);
	static FDefenseReachability CalculateReachability(
		float YawError, float TimeToDeadline, float TurnRate,
		float FinalTolerance, float HardCone, float RemainingTurnBudget);
};
```

The resolver must not access `UWorld`, components, assets, selectors, delegates, or mutable actor state. Implement `FTableDefensePresentationSelector` separately against a supplied immutable resolution context and configuration.

- [ ] **Step 9: Verify and commit Slice 1**

Build the editor target first, then run `KatanaCombat.Defense.Resolver`, `KatanaCombat.Defense.Presentation`, and `KatanaCombat.GameplayTags`. Complete the slice adversarial/spec-coverage gate, inspect `git diff --check`, and commit:

```powershell
git commit -m "Add typed defense resolution contracts"
```

---

### Task 2: Introduce The Native Rich-Contact Commit Boundary

**Files:**
- Modify: `Source/KatanaCombat/Public/Core/CombatComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- Modify: `Source/KatanaCombat/Public/Core/WeaponComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/WeaponComponent.cpp`
- Modify: `Source/KatanaCombat/Public/Characters/BaseCombatCharacter.h`
- Modify: `Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp`
- Modify: `Source/KatanaCombat/Public/Core/HitReactionComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/HitReactionComponent.cpp`
- Add: `Source/KatanaCombatTest/Private/DefenseContactTests.cpp`
- Add: `Source/KatanaCombatTest/Private/DefenseArchitectureSourceTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/WeaponComponentTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/DamageApplicationTests.cpp`

**Consumes:** Slice 1 IDs/query/result, current weapon traces, team interfaces, hit-reaction resistance/i-frame state, and health mutation.

**Produces:** One synchronous target-owned resolution/commit, bounded cache/tombstones, rich-contact receipt transport, and post-accept source accounting. Legacy generic contacts remain operational.

- [ ] **Step 1: Add failing rich-contact and accounting tests**

Cover `NewCommit`, `Cached`, same-key synchronous `InProgress`, friendly, invalid/dead, invulnerable/i-frame, consumed, null-attack compatibility, normal block, hit, and unblockable hit. Assert exact health delta, exact `OnDamageReceived` count, exact presentation/effect count, and returned `AppliedDamage` after resistance.

Add weapon-budget cases for `MaxHitCount = 1`:

```cpp
TestEqual(TEXT("Friendly does not consume budget"), Weapon->GetAcceptedHitCount(), 0);
TestEqual(TEXT("Hostile hit consumes budget"), Weapon->GetAcceptedHitCount(), 1);
TestEqual(TEXT("Cached duplicate does not consume again"), Weapon->GetAcceptedHitCount(), 1);
```

Add a multi-target test proving a friendly or invulnerable first target does not prevent a later hostile target. Add a same-frame test proving first committed contact prevents retroactive parry, while prior attack consumption returns `IgnoredConsumed` for contact.

- [ ] **Step 2: Add a static rich-path source gate**

`DefenseArchitectureSourceTests.cpp` loads `BaseCombatCharacter.cpp` and extracts the body of `ResolveAndCommitCombatContact`. Fail if that body contains `ApplyDamage`, `IsBlocking`, or `CanBlockHit`. Also fail if `WeaponComponent::ProcessHit` calls `AddHitActor` before obtaining a rich receipt.

Use a balanced-brace extractor that strips C++ comments before token checks. A raw whole-file substring test is too prone to false positives from compatibility methods and documentation.

- [ ] **Step 3: Add canonical cache ownership to `UCombatComponent`**

Add:

```cpp
EDefenseCommitStatus BeginDefenseInteraction(
	const FDefenseInteractionKey& Key,
	FDefenseInteractionId& OutId,
	FDefenseContactReceipt& OutExistingReceipt);
void FinalizeDefenseInteraction(
	const FDefenseInteractionId& Id,
	const FDefenseContactReceipt& Receipt);
void MarkDefenseContactSourceTerminal(const FContactInstanceId& ContactId, double UnscaledNow);
void SweepDefenseInteractionCache(double UnscaledNow);
```

Install an in-progress entry before any health or presentation call. Cache lookup occurs before live-generation validation. Retain active records, age terminal tombstones for the configured duration, and evict only the oldest terminal record when over cap. Track source participants so attack/window/compatibility-trace terminal notifications start tombstone aging; cache access and teardown perform a lazy fallback sweep.

Allocate a target-local monotonic interaction epoch only when a `(stage, attack/contact, defender)` key is first registered. Duplicate lookups reuse that `FDefenseInteractionId`; they never allocate a new epoch.

- [ ] **Step 4: Add the native target and source adapter methods**

Add to `ABaseCombatCharacter`:

```cpp
FDefenseContactReceipt ResolveAndCommitCombatContact(const FDefenseContactRequest& Request);
FDefenseContactReceipt ResolveWeaponContactCandidate(
	AActor* Target, const FDefenseContactRequest& Request);
void FinalizeResolvedWeaponContact(
	AActor* Target, const FDefenseContactReceipt& Receipt);
```

Keep `CommitResolvedDefenseDamage(const FDefenseResolution&)` private and native. It applies only the closed disposition and returns an internal `FDefenseGameplayCommitResult`: call a no-reclassification, no-presentation hit-reaction damage method for `ApplyRequestedDamage`, return zero for `SuppressDamage`, and perform nothing for `NoContactSideEffects`. The result records actual damage, old/new health, any newly committed dying state, and immutable deferred hit/health/death notification data. Snapshot team, death, paired participation, `CanBeDamaged`, and i-frame state once before pure resolution. Finalize the receipt atomically before returning.

Do not call the existing synchronous `UHitReactionComponent::ApplyDamage()`, `ABaseCombatCharacter::ModifyHealth()`, or `HandleDeath()` from the rich path. Split silent gameplay mutation from observable work: resistance and health/death state commit before receipt finalization; `OnDamageReceived`, `OnHealthChanged`, `OnCharacterDying`, hit/death montages, and optional effects dispatch only from the deferred result after the finalized receipt and source weapon accounting are coherent. Legacy entry points retain their immediate compatibility behavior by invoking the same silent primitive and then flushing it locally.

Build `FActualDefenseContact` from the complete immutable `FHitReactionInfo`, accepted trace segment, active source socket, weapon velocity, source bearing, trajectory-derived lane, exact/parent bone-height provenance, and actual surface/contact data. Do not overwrite the retained prediction with actual values.

Equal non-neutral teams resolve `IgnoredFriendly` when friendly fire is disabled. Explicit hostility may interact. Neutral/unknown rich contacts preserve the current damageable policy but are not eligible for parry/threat selection. The proof configuration keeps enemy-versus-enemy damage disabled; add a regression test using two enemy-team actors and one player-team actor.

`ApplyDamage_Implementation` remains a compatibility adapter. It may build a best-effort request when source identity exists, but the rich path must never call it recursively.

- [ ] **Step 5: Move weapon accounting after the receipt**

Increment `FWeaponTraceInstanceId::Generation` in `EnableHitDetection()`. Build `FContactInstanceId` from the active hit-window identity, or from the compatibility trace generation when no valid authored attack exists.

For a rich target, `ProcessHit()` must:

```cpp
const FDefenseContactReceipt Receipt = SourceCharacter->ResolveWeaponContactCandidate(HitActor, Request);
if (Receipt.CommitStatus == EDefenseCommitStatus::NewCommit && Receipt.bAcceptsWeaponHit)
{
	AddHitActor(HitActor);
	if (Receipt.bConsumesHitBudget)
	{
		++AcceptedHitCount;
	}
}
SourceCharacter->FinalizeResolvedWeaponContact(HitActor, Receipt);
```

Revalidate the weak source owner after the target call before touching weapon state. Generic targets keep the existing delegate path and weapon-owned dedupe. Friendly filtering for rich targets moves out of `ShouldIgnoreHitActor`; otherwise no explicit `IgnoredFriendly` receipt can exist.

Dead, invulnerable, i-frame, and null-`AttackData` prefilters also move behind the rich target boundary so they produce canonical receipts. Keep best-effort prefilters for generic targets. Add a test-only accepted-hit-count accessor. `Hit`, `UnblockableHit`, and `NormalBlock` each consume one rich hit-budget unit; ignored, cached, in-progress, and rejected contacts consume none.

- [ ] **Step 6: Enforce commit/event ordering and reentrancy**

Direct silent gameplay mutation and receipt finalization happen before presentation/delegates. Source finalization uses only the finalized receipt plus its target-owned deferred gameplay result, completes weapon accounting, attempts defender presentation and attacker response, revalidates both actors after every cross-actor call, then broadcasts immutable events. Before each optional dispatch, verify that the owning actor and interaction generation remain valid; participant destruction suppresses remaining optional work without changing the committed receipt. Add a generation-keyed one-shot end-of-frame fallback when the source coordinator becomes invalid. A cached or in-progress receipt never replays effects or broadcasts.

Add reentrancy tests whose `OnDamageReceived`, `OnHealthChanged`, and `OnCharacterDying` listeners query the same contact, begin another action, or destroy a participant. Assert that same-key reentry sees `InProgress` or the finalized cached receipt as appropriate, all listeners observe the finalized receipt, damage/death transition and each public event occur exactly once, weapon accounting is already coherent, and no optional presentation runs after its owner is destroyed.

- [ ] **Step 7: Verify and commit Slice 2**

Build the fresh editor/test modules, then run:

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="KatanaCombat.uproject" -Progress -NoHotReload
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.Defense.Contact;Quit" -unattended -nopause -NullRHI -nosplash -stdout
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.WeaponComponent;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Then run damage/death focused suites and complete the slice adversarial/spec-coverage gate. Commit:

```powershell
git commit -m "Make defense contact commits idempotent"
```

---

### Task 3: Centralize Input Capture, Threat Locking, Alignment, And Normal Block

**Files:**
- Modify: `Source/KatanaCombat/Public/ActionQueueTypes.h`
- Modify: `Source/KatanaCombat/Public/Core/CombatComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- Modify: `Source/KatanaCombat/Public/Core/TargetingComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/TargetingComponent.cpp`
- Modify: `Source/KatanaCombat/Public/Core/HitReactionComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/HitReactionComponent.cpp`
- Modify: `Source/KatanaCombat/Public/Data/CombatSettings.h`
- Modify: `Source/KatanaCombat/Private/Data/CombatSettings.cpp`
- Modify: `Source/KatanaCombat/Public/Animation/AnimNotifyState_CombatWarp.h`
- Modify: `Source/KatanaCombat/Private/Animation/AnimNotifyState_CombatWarp.cpp`
- Modify: `Source/KatanaCombat/Public/Characters/PlayerCharacter.h`
- Modify: `Source/KatanaCombat/Private/Characters/PlayerCharacter.cpp`
- Modify: `Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp`
- Add: `Source/KatanaCombatTest/Private/DefenseInputThreatTests.cpp`
- Add: `Source/KatanaCombatTest/Private/DefenseAlignmentTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/TargetingComponentTests.cpp`

**Consumes:** Slice 1 resolver/configuration, Slice 2 rich contact, current action queue, current target enumeration, Character Movement, and Motion Warping.

**Produces:** Unconditional input history, explicit routes/dispositions, published attack predictions, stable threat selection with hysteresis, owned alignment arbitration, normalized turn caps, and visible normal-block presentation.

- [ ] **Step 1: Add input record and route contracts**

Add to `ActionQueueTypes.h`:

```cpp
UENUM(BlueprintType) enum class ECombatInputRoute : uint8
{
	StatefulControl, ChainOnly, NormalQueue
};
UENUM(BlueprintType) enum class ECombatInputDisposition : uint8
{
	Captured, Consumed, Queued, Rejected, Expired
};

USTRUCT(BlueprintType)
struct FCombatInputRecord
{
	GENERATED_BODY()
	uint64 Serial = 0;
	EInputType InputType = EInputType::None;
	EInputEventType EventType = EInputEventType::Press;
	EInputDirection Direction = EInputDirection::None;
	double SimulationTimestamp = 0.0;
	double UnscaledTimestamp = 0.0;
	ECombatInputRoute Route = ECombatInputRoute::NormalQueue;
	ECombatInputDisposition Disposition = ECombatInputDisposition::Captured;
};
```

Keep a bounded 64-record history on `UCombatComponent`. `OnInputEvent()` creates the record before settings lookup, `CanProcessInput`, block/counter routing, or `CanAcceptNewInput`. Add a test-only accessor and production debug accessor returning a const view.

- [ ] **Step 2: Add failing route tests**

Prove:

- Block Press/Release are captured and never deferred.
- A rejected Block Press is terminal.
- Block held before a later parry window remains normal guard.
- Pressing Block again during the active window creates a new perfect-parry attempt.
- Light/Heavy in `CounterWindow` is `ChainOnly`.
- Failed Chain preflight marks the edge `Expired`, leaves the window active, and does not change action-queue size.
- Normal attack inputs outside an owning Chain route keep last-input-wins behavior.

Rewrite `KatanaCombat.CounterSystem.Input.BlockStartsChainParry` here so it no longer expects `CounterWindow` in the Block call. It should initially assert `ParryActive`; Task 5 will assert the later marker transition.

- [ ] **Step 3: Publish immutable attack execution records**

Add to `UCombatComponent`:

```cpp
const FAttackExecutionSnapshot BuildAttackExecutionSnapshot() const;
void PublishAttackThreatPrediction(const FAttackThreatPrediction& Prediction);
void InvalidateAttackThreatPrediction(EThreatInvalidationReason Reason);
void SetAttackIntentTarget(AActor* IntendedTarget);
```

Create the active attack ID from owner plus `AttackStateMachine.AttackGeneration`. Capture the explicit target when the attack starts, not when defense queries it. AI calls `SetAttackIntentTarget(CombatTarget)` before execution. Player attack setup records the same target selected by attack targeting/warp. Republish on target, montage-rate, window, or attack-path change. High confidence requires an intended defender, reviewed contact/window deadline, and source-to-contact path intersection with that defender's configured threat capsule.

Assign each `UCombatComponent` one immutable `FCombatantStableId` from a process-monotonic 64-bit serial at registration. Add a test-only explicit setter under `WITH_AUTOMATION_TESTS`. Do not use `GetUniqueID`, pointer value, transient GUID, or query order.

- [ ] **Step 4: Add threat lock and guarded refresh ownership**

Add:

```cpp
FDefenseThreatSelectionResult SelectDefenseThreat(double SimulationNow);
void RefreshGuardThreat(EThreatRefreshReason Reason);
void ClearGuardThreat(EThreatClearReason Reason);
```

Call `UTargetingComponent::GetAllTargetsInRange()` exactly once per selection opportunity, cap the query by both defense and targeting ranges, then filter the immutable result. Preserve the lock for `ThreatLockMinSeconds`; switch only for invalidation or the configured earlier-contact lead. Coalesce publication events in the same frame. Use a 0.05-second simulation-time timer only while guarding and candidates exist. Block release and paired takeover cancel it.

Line-of-sight loss disqualifies input-intent parry and auto-facing but never suppresses a physical contact. Neutral/unknown actors may retain generic contact compatibility but are never selected as guard/parry threats without explicit hostility.

Tests use two explicit stable IDs and scripted deadlines to prove lock stability, switch lead, invalidation, low/stale confidence downgrade, same-candidate guard fallback, and no second enumeration after failed parry evaluation.

- [ ] **Step 5: Add opaque alignment handles and priority arbitration**

Add `FAlignmentRequestHandle`, `FAlignmentRequestSpec`, `EDefenseAlignmentPriority`, and `EAlignmentExecutor` to shared types. Add to `UTargetingComponent`:

```cpp
FAlignmentRequestHandle AcquireAlignmentRequest(const FAlignmentRequestSpec& Spec);
bool UpdateAlignmentRequest(FAlignmentRequestHandle Handle, const FAlignmentRequestSpec& Spec);
void ReleaseAlignmentRequest(FAlignmentRequestHandle Handle);
void ReleaseAllAlignmentRequests(EAlignmentReleaseReason Reason);
```

Priorities, high to low, are death/terminal cancellation, paired/parry bridge, block contact/attacker response, active attack warp, and guard facing. A higher request suspends but does not destroy a lower valid request. Only owner handle release is allowed except death/component teardown. Capture movement/controller rotation settings when the first request starts and restore those exact values after the last release.

Enable `UTargetingComponent` tick only while a smooth-rotation request exists. Apply capped yaw through `UCharacterMovementComponent::MoveUpdatedComponent` with sweep; do not call `SetActorRotation`. Motion Warping requests own named targets and remove only those names.

Set the targeting tick to `TG_PrePhysics` and add Character Movement's primary tick as a prerequisite so smooth alignment evaluates after Character Movement/root-motion composition in that group. Disable Character Movement/controller-driven actor yaw while an alignment owner is active, restore the exact captured settings after the last release, and permit only one rotation executor per actor per frame. Motion-Warping frames measure after Character Movement and must not also apply smooth yaw.

`ClearMotionWarp(NAME_None)` is permitted only for death or component teardown. Add a source/test gate that normal request release removes its own target names and cannot clear another request.

- [ ] **Step 6: Normalize Motion Warping rotation**

Fix `UAnimNotifyState_CombatWarp::AddRootMotionModifier_Implementation()` so it never mutates or explicitly reuses mutable state from the shared `RootMotionModifier` template. Call `UMotionWarpingComponent::AddModifierFromTemplate()` first, cast and configure only the returned engine-owned clone, set the request-owned target and `RotationMethod = EMotionWarpRotationMethod::ConstantRate`, and register that clone against its alignment handle.

Bind clone-local update/deactivation delegates through `UTargetingComponent`. UE 5.6 `URootMotionModifier::Update()` writes the current effective `PlayRate` before invoking `OnUpdateDelegate`, so the update callback recomputes:

```cpp
RuntimeModifier->WarpMaxRotationRate = EffectiveDefenseTurnRate /
	FMath::Max(FMath::Abs(EffectiveMontagePlayRate), SmallRate);
```

Recompute every active update so montage-rate changes cannot retain a stale cap. At or below `SmallRate`, or for reverse playback, disable the owned modifier and select the no-warp fallback. Deactivation and request release unregister only that clone. Telemetry validates final composed actor yaw against `EffectiveDefenseTurnRate * SimulationDeltaSeconds`; modifier configuration alone is not success.

Tests cover 0.5x, 1.0x, and 2.0x rates, world/actor dilation, request suspension/resume, owner-only release, exact settings restoration, total automatic-turn budget, and one executor per frame.

- [ ] **Step 7: Implement sustained guard manual override**

Feed normalized yaw input from `APlayerCharacter` look handling into `UCombatComponent::SetDefenseManualYawInput`. Suspend only `GuardFacing` at or above 0.25 magnitude. Resume after 0.10 unscaled seconds below threshold, revalidating the locked threat without resetting its interaction turn budget. Committed block/parry/paired alignment retains ownership over steering.

Camera/control yaw may remain responsive, but Character Movement actor yaw while guarding must still be capped by the same resolved defense rate for manual and automatic sources. Test final actor yaw, not controller yaw.

- [ ] **Step 8: Wire configuration precedence and normal-block presentation**

Add `UDefenseConfiguration* DefenseConfiguration` to `UCombatSettings` and `UDefenseConfiguration* DefenseConfigurationOverride` to `UCombatComponent`. Add scoped stance-override acquire/release handles. Resolve in this order: active scoped stance override, component override, combat settings, C++ defaults.

On `NormalBlock`, use the committed resolution to call new `UHitReactionComponent` presentation entry points:

```cpp
bool PlayDefensePresentation(const FDefenseResolution& Resolution);
bool PlayAttackerResponse(const FDefenseResolution& Resolution);
```

They may select/play assets but may not recalculate block, damage, team, or geometry. Apply blocked-impact precedence from the spec. Guard enter/exit montages are optional; held guard remains AnimBP state through `bIsBlocking`.

Remove `FaceThreatForBlock()` and its direct rotation. Remove the AI attack-start `SetActorRotation`; active attack alignment/warp owns that facing operation.

- [ ] **Step 9: Verify and commit Slice 3**

Build first, then run `KatanaCombat.Defense.Input`, `.Threat`, `.Alignment`, `.CounterSystem.Input`, and `.Targeting`. Run the source gate, confirm no defense path uses `SetActorRotation` and no shared warp template is mutated, and complete the slice adversarial/spec-coverage gate. Commit:

```powershell
git commit -m "Route guard input through owned defense alignment"
```

---

### Task 4: Commit Perfect Parry, Consume Attack Generations, And Start The Bridge

**Files:**
- Modify: `Source/KatanaCombat/Public/Core/CombatComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- Modify: `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`
- Add: `Source/KatanaCombat/Public/Animation/CombatAnimNotifyIdentity.h`
- Add: `Source/KatanaCombat/Private/Animation/CombatAnimNotifyIdentity.cpp`
- Modify: `Source/KatanaCombat/Public/Animation/AnimNotify_AttackPhaseTransition.h`
- Modify: `Source/KatanaCombat/Private/Animation/AnimNotify_AttackPhaseTransition.cpp`
- Modify: `Source/KatanaCombat/Public/Animation/AnimNotifyState_ParryWindow.h`
- Modify: `Source/KatanaCombat/Private/Animation/AnimNotifyState_ParryWindow.cpp`
- Modify: `Source/KatanaCombat/Public/Animation/AnimNotifyState_CounterWindow.h`
- Modify: `Source/KatanaCombat/Private/Animation/AnimNotifyState_CounterWindow.cpp`
- Modify: `Source/KatanaCombat/Public/Interfaces/CombatInterface.h`
- Modify: `Source/KatanaCombat/Public/Characters/BaseCombatCharacter.h`
- Modify: `Source/KatanaCombat/Public/Core/WeaponComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/WeaponComponent.cpp`
- Modify: `Source/KatanaCombat/Public/AI/EnemyCombatAIComponent.h`
- Modify: `Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp`
- Modify: `Source/KatanaCombat/Public/AI/EnemyCombatStateTreeTasks.h`
- Modify: `Source/KatanaCombat/Private/AI/EnemyCombatStateTreeTasks.cpp`
- Modify: `Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp`
- Add: `Source/KatanaCombatTest/Private/DefenseParryTests.cpp`
- Add: `Source/KatanaCombatTest/Private/CombatAnimNotifyIdentityTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/ParryDetectionTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/EnemyCombatAITests.cpp`
- Modify: `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`

**Consumes:** Captured Block Press, published threat, pure decision, interaction cache, selector, alignment, current attack state machine, weapon trace, StateTree attack token, and paired component.

**Produces:** Generation-safe parry windows, edge-triggered perfect parry with downgrade, atomic whole-attack consumption, exact AI termination, attacker `ParryStagger`, and post-decision bridge preflight.

- [ ] **Step 1: Make notify windows generation-safe**

Include `Animation/ActiveMontageInstanceScope.h` and extract:

```cpp
const UE::Anim::FAnimNotifyMontageInstanceContext* Context =
	EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
const int32 MontageInstanceId = Context ? Context->MontageInstanceID : INDEX_NONE;
const FAnimNotifyRuntimeSourceId NotifySourceId =
	ResolveRuntimeNotifySourceId(EventReference);
```

Implement `ResolveRuntimeNotifySourceId` in `CombatAnimNotifyIdentity.*`. It uses `EventReference.GetSourceObject()`, casts it to `UAnimSequenceBase`, and finds the exact `EventReference.GetNotify()` address in that source's runtime `Notifies` array. It returns source soft path plus event index; UE 5.6's `FAnimNotifyEvent::Guid` is editor-only and must never appear in runtime code.

`NotifyBegin` calls `OpenAttackWindow(Kind, NotifySourceId, MontageInstanceId, Duration)` and stores the returned generation in the component record keyed by attack ID, runtime notify source ID, and montage instance. `NotifyEnd` calls `CloseAttackWindow(Kind, NotifySourceId, MontageInstanceId)`; it can close only that matching record. Hit detection enable/disable uses the same model with a hit-window generation. Add stale End tests for old attack, same montage/new instance, same source event/new generation, invalid source index, missing montage context, and interrupted montage.

`AnimNotify_AttackPhaseTransition(Active)` and `(Recovery)` are distinct point events. Add an identity-bearing combat-interface context call for production notifies: Active opens a Hit window using its source ID and montage instance; Recovery may close only the current attack's Hit generation for that same montage instance and records its own source ID as the close trigger. The old context-free phase call remains a deprecated compatibility adapter and cannot satisfy canonical authored-window proof. Deprecated `AnimNotify_ToggleHitDetection` may use only the compatibility trace generation and is prohibited from Gate A/B montages. Test stale Recovery from an old attack/montage, duplicate Active/Recovery, interruption cleanup, and compatibility isolation.

Keep compatibility `SetParryWindowActive`/counter methods only as deprecated adapters for legacy tests/assets; production notifies use identities.

- [ ] **Step 2: Add failing perfect-parry matrix and downgrade tests**

Enter through `UCombatComponent::OnInputEvent(Block, Press)`. Cover valid parry, held-before-window non-parry, no tag, no window, stale generation, low/stale prediction, hard cone, normal-reachable/perfect-unreachable downgrade, unblockable-plus-parryable, dead/paired/attacking defender rejection, and bridge missing/blocked/over-budget.

Assert bridge usability never changes a committed `PerfectParry` decision. Assert a failed parry and guard fallback retain the same selected attack ID. Assert contact-first versus input-first same-frame ordering.

- [ ] **Step 3: Implement atomic `ConsumeActiveAttack`**

Add:

```cpp
bool ConsumeActiveAttack(
	const FAttackInstanceId& AttackId,
	EAttackConsumeReason Reason);
```

Before callbacks, mark the matching generation consumed. Close matching hit/parry/counter windows, disable matching trace generation, cancel matching alignment, reject queued combo continuation, and mark the montage consumed-pending-presentation. Emit one internal native termination event immediately and queue one public `OnAttackConsumed` event for deferred flush. Duplicate/stale calls return false without side effects.

Track every defender interaction under the generation so a perfect parry suppresses remaining targets. Previously committed damage remains committed.

- [ ] **Step 4: Terminate AI attack ownership exactly once**

Store the attack generation when `UEnemyCombatAIComponent::ExecuteAttack()` starts. Bind its native handler before public events:

```cpp
void HandleAttackConsumedInternal(const FAttackConsumedEvent& Event);
```

Abort only the matching StateTree task, release the combat token once, and transition to recovery/ready state. `OnParried`, `OnCountered`, and montage-end compatibility handlers may play/clean presentation but must not release the same token again. StateTree task polling compares the captured generation and recognizes consumed termination rather than waiting forever for montage end.

- [ ] **Step 5: Commit perfect parry and select attacker response**

On a Block Press resolver result of `PerfectParry`, install/finalize the input-intent interaction, consume the source attack, apply `ParryStagger`, and retain guard ownership as specified. A Block Release after commit does not cancel the owned sequence. A presentation listener cannot rewrite the result or re-consume the attack.

Normal block selects `Continue` unless `BlockInterruptible` requests `Recoil`; perfect parry always requests `ParryStagger`. Remove the current hardcoded two-second stagger from canonical Chain flow and resolve response duration/playback from the attacker's defense configuration.

- [ ] **Step 6: Preflight and start the parry bridge after decision**

Add:

```cpp
bool UPairedAnimationComponent::BeginDefenseSequence(
	const FDefenseResolution& Resolution);
```

Select the bridge only after `PerfectParry` is immutable. Preflight both actors, montages/sections, driver marker, warp targets, collision sweep, remaining alignment time, and per-role translation/rotation budgets. Try deterministic fallback rows. If none is usable, start a stage-generation-keyed 0.15-second simulation timer. Bridge failure closes Chain safely but never restores the consumed attack or converts parry to damage.

After terminal bridge failure, return the defender to guard only when Block remains held and guard eligibility is still valid; otherwise restore the correct non-guard state. Never use this fallback to reopen the consumed source attack.

For paired bridge data, map the defender/sequence initiator to `UPairedAnimationData::AttackerMontage` and the consumed source attacker to `VictimMontage`; validation and telemetry print semantic and authored role names.

- [ ] **Step 7: Verify and commit Slice 4**

Build first, then run `KatanaCombat.Defense.Parry`, `.ParryDetection`, `.EnemyCombatAI`, and relevant contact tests. Run source gates for generation-blind window close, duplicate token release, and legacy block recalculation, then complete the slice adversarial/spec-coverage gate. Commit:

```powershell
git commit -m "Consume attacks on committed perfect parry"
```

---

### Task 5: Retain Chain Stages And Replace Shared Time-State Restoration

**Files:**
- Modify: `Source/KatanaCombat/Public/CombatTypes.h`
- Modify: `Source/KatanaCombat/Public/Core/CombatComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- Modify: `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`
- Modify: `Source/KatanaCombat/Public/Data/PairedAnimationData.h`
- Modify: `Source/KatanaCombat/Private/Data/PairedAnimationData.cpp`
- Add: `Source/KatanaCombat/Public/Animation/AnimNotify_ChainStageTransition.h`
- Add: `Source/KatanaCombat/Private/Animation/AnimNotify_ChainStageTransition.cpp`
- Add: `Source/KatanaCombat/Public/Subsystems/CombatEffectsWorldSubsystem.h`
- Add: `Source/KatanaCombat/Private/Subsystems/CombatEffectsWorldSubsystem.cpp`
- Modify: `Source/KatanaCombat/Public/Utilities/CinematicEffectsUtilityLibrary.h`
- Modify: `Source/KatanaCombat/Private/Utilities/CinematicEffectsUtilityLibrary.cpp`
- Modify: `Source/KatanaCombat/Private/Animation/AnimNotifyState_PairedAnimationCollision.cpp`
- Add: `Source/KatanaCombatTest/Private/DefenseChainTests.cpp`
- Add: `Source/KatanaCombatTest/Private/CombatEffectsLeaseTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/PairedAnimationTests.cpp`

**Consumes:** Committed perfect parry, selected bridge, Chain-only input, existing counter/finisher data, paired montage/warp/collision operations, context tags, and cinematic effects.

**Produces:** `ParryActive -> CounterWindow -> CounterActive -> FinisherReady -> FinisherActive -> None`, retained pose/ownership between stages, scoped context/time leases, generation-safe markers/callbacks, and rollback-safe two-actor starts.

- [ ] **Step 1: Extend the state and paired authoring contracts**

Add `FinisherActive` to `EChainCounterState`. Add `EPairedAnimationRole`, `EChainStageTransitionType` (`OpenCounterWindow`, `AutoContinue`), and a compact `FPairedChainTransitionPolicy` to `UPairedAnimationData` containing driver role, required marker, compatible ready sections/terminal-pose flags, positive window override, auto-continue policy, and finisher retryability.

Add `FDefenseSequenceContext` fields from the accepted spec: originating resolution/snapshot, weak actors, reflected strong counter/finisher references, current state, stage generation, per-role montage-instance IDs, active presentation, timeout handles, and opaque context/alignment/time/collision/input ownership handles. Revalidate weak actors on every transition.

- [ ] **Step 2: Replace direct context tag mutation with leases**

Add:

```cpp
FCombatContextLeaseHandle AcquireContextTagLease(FGameplayTag Tag, FName Owner);
void ReleaseContextTagLease(FCombatContextLeaseHandle Handle);
```

Reference-count tags by valid handle. Keep old add/remove calls as legacy ref-counted adapters; `ClearActiveContextTags()` is teardown-only. `UPairedAnimationComponent` alone acquires `Context.ParryCounter` for the sequence and releases it once on terminal cleanup.

- [ ] **Step 3: Add generation-keyed stage markers**

`UAnimNotify_ChainStageTransition` extracts montage-instance context exactly as Task 4 and calls:

```cpp
void HandleChainStageTransition(
	EChainStageTransitionType Transition,
	int32 MontageInstanceId,
	FAnimNotifyRuntimeSourceId NotifySourceId);
```

Only the authored driver role with the active interaction, stage generation, montage instance, and valid runtime source event may advance. Partner markers and duplicates record telemetry only. Bridge marker enters `CounterWindow`; it never drops montage/pose ownership. Counter auto-continue allocates and retires generations before starting the successor, so synchronous outgoing interruption cannot clean the new stage.

- [ ] **Step 4: Implement rollback-safe two-actor start**

Use an explicit transaction local to `UPairedAnimationComponent`:

1. Validate actors, state, animation instances, montages, sections, markers, and warp targets.
2. Allocate successor generation; acquire stage-owned context, input, collision, alignment, warp, and time leases.
3. Start both roles with explicit blend settings.
4. Mark active only after both starts succeed.
5. On partial failure, stop the started role and release only successor-owned state.

Never claim the interrupted outgoing pose can be restored. Unexpected bridge montage end before counter input is failure cleanup. A failed counter start expires only that input and keeps the original `CounterWindow` until timeout. A retryable automatic-finisher failure enters `FinisherReady`; invalid/non-retryable data performs terminal cleanup.

Canonical stages acquire collision and movement leases before either role starts. `UAnimNotifyState_PairedAnimationCollision` becomes a compatibility adapter that delegates Begin/End to per-actor, per-montage-instance component records keyed by runtime notify source ID; it stores no cached owner, saved response/movement mode, partner array, or mutable active flag on the asset-shared notify object. Stale End can release only its matching record, and terminal sequence cleanup releases any surviving stage-owned lease.

- [ ] **Step 5: Preserve ownership across successful stages**

Counter completion that auto-continues must not call broad paired cleanup, clear the action queue, expose `ChainState::None`, restore collision/movement/input, remove `Context.ParryCounter`, clear warp targets owned by the successor, or release time leases. Terminal cleanup is idempotent and keyed by interaction plus active generation.

Start `CounterWindow` only at the bridge driver marker and start its 2.0-second default timeout then, not at Block Press. `CounterWindow` and `FinisherReady` use unscaled real time; montage/marker progress and no-montage bridge fallback use simulation time. Light/Heavy input in either response state is `ChainOnly`; a failed finisher attempt expires only that edge and leaves `FinisherReady` retryable until its original timeout.

Drive unscaled response deadlines from `FPlatformTime::Seconds()` through generation-keyed `FTSTicker` callbacks, not a world timer affected by dilation. Store every returned `FDelegateHandle`; cancellation, component teardown, and world teardown remove it explicitly. Every callback validates weak world/actors, interaction, and stage generation before transition or cleanup and returns `false` on any stale owner.

Rewrite public-flow counter tests named in the spec. Retire `KatanaCombat.CounterSystem.Internal.ChainParryTransition`. Rewrite `Internal.ChainParryStaggersEnemy` as data-driven `ParryStagger`. Keep narrow null/no-op primitives but do not count them as feature proof.

- [ ] **Step 6: Add overlap-safe world and actor time leases**

Implement `UCombatEffectsWorldSubsystem` APIs:

```cpp
FTimeDilationLeaseHandle AcquireWorldLease(FName Owner, float AbsoluteScale, double WatchdogSeconds);
FTimeDilationLeaseHandle AcquireActorLease(AActor* Actor, FName Owner, float AbsoluteScale, double WatchdogSeconds);
void ReleaseLease(FTimeDilationLeaseHandle Handle);
```

The first lease captures the prior value. Effective value is the minimum of baseline and active absolute requests. Releasing recomputes; the last release restores baseline. Use `FTSTicker` only for active unscaled watchdog/restoration callbacks; the subsystem itself does not tick continuously. Retain every ticker `FDelegateHandle`, remove it on normal release and `Deinitialize`, and make callbacks weak-world-safe. World teardown releases safely without dereferencing actors.

Migrate `UCinematicEffectsUtilityLibrary` and hitstop to the subsystem. New paired code stores handles. Legacy Apply/Restore calls delegate through named compatibility leases and diagnose direct external dilation changes while leases are active. Remove static global actor hitstop restoration state after actor-lease tests replace it.

- [ ] **Step 7: Add adversarial lifecycle tests**

Test bridge marker delay, hold pose ownership, counter retry, auto-finisher success/failure, timeout, cancel, owner death, partner death, missing montage, one-role start failure, stale marker, stale montage end, duplicate callback, listener destruction, listener starting another action, two actors sharing one notify asset, overlapping collision/movement windows, stale collision End, overlapping world leases, overlapping actor freezes, baseline restoration, duplicate release, watchdog, and world teardown.

Every successful multi-stage test asserts no intermediate `None`, queue clear, input restore, collision restore, tag release, or warp clear. Every terminal path asserts each owner is restored exactly once.

- [ ] **Step 8: Verify and commit Slice 5**

Build first, run `KatanaCombat.Defense.Chain`, `.CombatEffects`, `.CounterSystem`, and `.PairedAnimation`, then run the full `KatanaCombat` automation root. Complete the slice adversarial/spec-coverage gate before committing:

```powershell
git commit -m "Retain defense ownership across Chain stages"
```

## Counter Test Migration Ledger

Apply this ledger during Tasks 3-5. Do not preserve a stale expected state merely because a test predates the accepted spec.

| Current coverage | Required disposition |
|---|---|
| `TryCounter.ChainUsesParryWindow` | Rewrite through captured Block Press, pure selection, committed parry, and bridge start. |
| `ChainStoresParriedTarget` | Rewrite to assert `FDefenseSequenceContext` retains the selected attack/actors and revalidates weak references. |
| `ChainPairedCancelClearsContext` | Rewrite against a real active stage and assert every owned lease releases exactly once. |
| `Input.BlockStartsChainParry` | Replace same-call `CounterWindow` expectation with `ParryActive`, then marker-driven `CounterWindow`. |
| `ChainAttackInputAdvancesCounter` | Enter through `OnInputEvent`, assert `ChainOnly`, and prove no normal-queue fallthrough. |
| `ChainAdvanceRejectsNullAttackData` | Keep the window retryable and expire only the captured edge. |
| Nonlethal counter policy | Prove through committed counter resolution; finisher owns lethality unless data explicitly opts in. |
| `Internal.ChainParryStaggersEnemy` | Replace with data-driven `EAttackerResponse::ParryStagger`. |
| `Internal.ChainCancelResetsState` | Rewrite for cancellation from real `ParryActive`, `CounterWindow`, `CounterActive`, `FinisherReady`, and `FinisherActive` generations. |
| `Internal.ChainParryTransition` | Retire. Immediate `ParryActive -> CounterWindow` is invalid behavior. |
| `Internal.CancelChainCounterNoopWhenNone` | Keep as primitive idempotence only. |
| `Internal.ExecuteChainCounterAttackRequiresCounterWindow` | Keep as primitive state guard only. |
| `Internal.ChainNullAttackerFails` | Keep as primitive null safety only. |
| AC3 tests | Keep separate and label as legacy-mode coverage; never count toward Chain acceptance. |

---

### Task 6: Build Defense Content Validation And Prove Gate A

**Files:**
- Add: `Source/KatanaCombat/Public/Debug/DefenseTelemetry.h`
- Add: `Source/KatanaCombat/Private/Debug/DefenseTelemetry.cpp`
- Modify: `Source/KatanaCombat/KatanaCombat.cpp`
- Modify: `Source/KatanaCombat/Public/Core/CombatComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- Modify: `Source/KatanaCombat/Public/Core/TargetingComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/TargetingComponent.cpp`
- Modify: `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`
- Modify: `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`
- Add: `Source/KatanaCombatEditor/Public/DefenseAssetValidationService.h`
- Add: `Source/KatanaCombatEditor/Private/DefenseAssetValidationService.cpp`
- Add: `Source/KatanaCombatEditor/Public/Commandlets/Operations/DefenseProofMigrationOperation.h`
- Add: `Source/KatanaCombatEditor/Private/Commandlets/Operations/DefenseProofMigrationOperation.cpp`
- Modify: `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h`
- Modify: `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationRunner.h`
- Modify: `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
- Add: `Config/AssetMigrations/DefenseGateATargets.txt`
- Add after inventory: `Tools/Codex/manifests/defense-gate-a.json`
- Modify: `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`
- Add: `Source/KatanaCombatTest/Private/DefenseAssetValidationTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`
- Modify after reviewed plan: named Gate A `.uasset` packages only

**Known Gate A roots:**

- Attack: `/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1`
- Montage: `/Game/ProjectFiles/Animation/Montages/Katana/Light/AM_Light_Combo_1.AM_Light_Combo_1`
- Section: `Attack_1`
- Map: `/Game/ProjectFiles/Levels/Lvl_ThirdPerson1.Lvl_ThirdPerson1`

**Produces:** Reusable structured defense manifests, audit/plan/apply/save gates, root-motion/marker/config validation, structured runtime telemetry, exact Gate A content edits, and visible one-attack proof.

- [ ] **Step 1: Add structured telemetry before touching content**

Define `FDefenseTelemetryRecord` with interaction/attack/window/stage IDs, both timestamps, actors/stable IDs, candidate/lock/switch reason, outcome/reason, predicted/actual axes, yaw/deadline/rates, alignment owner/executor, configured engine warp rate, final frame yaw, displacement, pelvis delta, cache/weapon disposition, selected row/fallback, and cleanup reason.

Each `UCombatComponent` keeps a bounded ring. Targeting and paired owners append through explicit sink methods; no global gameplay singleton is introduced. Add console commands in `KatanaCombat.cpp`:

```text
Combat.Defense.Debug 0|1
Combat.Defense.DumpTelemetry <absolute-or-project-relative-csv-path>
Combat.Defense.ClearTelemetry
```

CSV output must be stable and machine-readable. Sample transforms immediately before presentation/stage and after each evaluated frame. Record expected authored/warp contribution rather than treating all movement as error.

- [ ] **Step 2: Define the checked-in JSON manifest schema**

`DefenseProofMigration` consumes only explicit JSON paths listed in `DefenseGateATargets.txt`/`DefenseGateBTargets.txt`; global scan is rejected. Use UE JSON APIs, not string parsing. Schema version 1 includes:

```json
{
  "schemaVersion": 1,
  "gate": "A",
  "map": "/Game/ProjectFiles/Levels/Lvl_ThirdPerson1.Lvl_ThirdPerson1",
  "defenseConfiguration": "required object path",
  "combatSettings": ["required player and enemy settings object paths"],
  "attacks": [{
    "name": "LightAttack_1",
    "attackData": "/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1",
    "montage": "/Game/ProjectFiles/Animation/Montages/Katana/Light/AM_Light_Combo_1.AM_Light_Combo_1",
    "section": "Attack_1",
    "expectedHeight": "Middle",
    "expectedLane": "Center",
    "expectedTags": ["Attack.Defense.Parryable"],
    "parryWindow": {"startSeconds": 0.0, "endSeconds": 0.0, "reviewed": false}
  }],
  "presentations": [],
  "pairedDependencies": [],
  "expectedCases": []
}
```

The snippet defines shape, not accepted Gate A timing. Validation must reject `reviewed=false`, zero/negative windows, empty required paths/arrays, duplicate case names, assets outside `/Game`, and any field not consistent with loaded assets. Do not check in the concrete manifest until inventory replaces every required value and a human/agent visual review supplies real section-relative times.

- [ ] **Step 3: Build read-only inventory and validation first**

`FDefenseAssetValidationService` must inventory:

- Attack profile/tag/window agreement.
- Defense configuration assignment and fallback coverage.
- Exact/wildcard ambiguity.
- Block, bridge, recoil/stagger, counter, finisher, VFX, audio, sockets, target bones, markers, and both role sections.
- Counter/finisher boolean/reference agreement.
- Bridge driver and compatible `CounterReady` pose ownership.
- Paired sync/collision notifies.
- `IA_Block` and `IMC_Combat` assignment, the required thumb-mouse mapping, and player input binding.
- Guard AnimBP state wiring plus optional enter/exit montage assignment.
- Bone-height exact/parent/fallback provenance.
- Sampled montage root yaw and horizontal translation.
- Complete dependency closure for the named gate, including map/Blueprint/settings references.

Use the UE 5.6 three-argument `UAnimMontage::ExtractRootMotionFromTrackRange(StartTime, EndTime, ExtractContext)` overload over the exact section/ranges, with an explicitly initialized `FAnimExtractContext` that enables root-motion extraction. Do not use the deprecated two-argument overload. A normal-block montage above 1 cm horizontal numerical drift or with authored yaw that can violate the cap is invalid. A perfect-parry role above 75 cm horizontal displacement is invalid.

- [ ] **Step 4: Extend the commandlet without weakening save gates**

Register `DefenseProofMigration` in `FKatanaAssetMigrationRunner`. Extend report rows/schema with manifest/gate/case, profile fields, tag/window agreement, selected rows/fallbacks, marker/role checks, root-motion measurements, bone fallback provenance, dependency paths, coverage cells, and save packages.

Upgrade the reusable migration report schema with backward-compatible common fields plus operation-specific details. `DefenseProofMigration` emits one row per manifest attack/presentation/paired/expected case, not one aggregate row, and includes a separate deterministically ordered package ledger with package role, initial dirty state, planned action, actual action, save result, and post-save reload result. Placeholder booleans from unrelated operations are not accepted as defense evidence.

Modes:

- `Audit`: load and validate only.
- `Plan`: report exact property/notify/assignment changes and packages and emit a deterministic `plan_fingerprint` over operation/schema, canonical manifest content, current asset facts, ordered proposed changes, and ordered package ledger.
- `Apply`: mutate in memory only and require `-ApprovedPlanReport` plus the reviewed `-ApprovedPlanFingerprint`; recompute the current plan before mutation and reject any structural or fingerprint drift.
- `ApplyAndSave`: require the same reviewed-plan binding plus `-AllowPackageSave`; require `-AllowTimingMutation` for a parry-window edit; reject initially dirty packages unless explicitly allowed; require the actual changed-package set to equal the approved ledger; save only those packages.

Use a stable engine hash API over canonical UTF-8 bytes; this fingerprint is a drift/approval binding, not an authentication mechanism. No mode invents parry timing, presentation assets, sockets, bones, or fallback rows. Apply is idempotent. World Partition actor/object packages are listed individually, saved through a verified editor/package path, and reloaded before success is reported. Post-save Audit must return `Unchanged` with zero errors.

- [ ] **Step 5: Add commandlet and validation tests**

Use transient AttackData/montages/configurations plus small checked-in test fixtures. Prove malformed JSON, unknown schema/fields, missing assets, false review flag, timing outside section, duplicate parry window, tag/window mismatch, ambiguous rows, missing generic fallback, root-motion budget failure, marker-role ambiguity, incomplete dependency closure, per-case row cardinality, canonical fingerprint stability, edited manifest/asset/plan drift refusal, missing or mismatched approval arguments, plan/apply idempotence, changed-package-set mismatch, dirty-package refusal, save-gate refusal, external-actor package reporting/reload, and JSON report serialization.

- [ ] **Step 6: Run Gate A inventory and make the manifest concrete**

Start with a read-only target that inventories the known attack/map. Use the report plus Editor/UEMCP visual inspection to choose exact existing or newly created defense assets and reviewed parry timing. The agent performs the asset creation/assignment; do not ask the user to wire the level manually.

Write `Tools/Codex/manifests/defense-gate-a.json` only after all required fields are concrete, then set `Config/AssetMigrations/DefenseGateATargets.txt` to that one manifest path. Record every selected dependency and expected case. If a suitable bridge/counter/finisher pair does not visually align, create or select a different explicit pair instead of accepting template provenance.

- [ ] **Step 7: Run Audit and Plan, then review exact package scope**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseProofMigration -Mode=Audit -TargetsFile="Config/AssetMigrations/DefenseGateATargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-audit.json" -unattended -nopause -NullRHI -nosplash -stdout
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseProofMigration -Mode=Plan -TargetsFile="Config/AssetMigrations/DefenseGateATargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-plan.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: Audit reports current gaps; Plan names only the reviewed attack, montage, configuration/settings, paired/presentation assets, and assignment packages. Stop if unrelated maps/assets appear.

Review every row and package-ledger entry. Record the exact `plan_fingerprint` only after that review; extracting a fingerprint and immediately applying without scope review does not satisfy approval.

- [ ] **Step 8: Prove mutation in memory before saving**

```powershell
$approvedPlan = "Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-plan.json"
$approvedFingerprint = "<reviewed plan_fingerprint>"
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseProofMigration -Mode=Apply -TargetsFile="Config/AssetMigrations/DefenseGateATargets.txt" "-ApprovedPlanReport=$approvedPlan" "-ApprovedPlanFingerprint=$approvedFingerprint" -AllowTimingMutation -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-apply.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Expected: changed-in-memory rows match Plan, no package is saved, and reapplying in the same test context is idempotent.

- [ ] **Step 9: Apply and save only reviewed Gate A packages**

Close the Editor first. Confirm `git status --short` and initially dirty package state. Then:

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseProofMigration -Mode=ApplyAndSave -TargetsFile="Config/AssetMigrations/DefenseGateATargets.txt" "-ApprovedPlanReport=$approvedPlan" "-ApprovedPlanFingerprint=$approvedFingerprint" -AllowTimingMutation -AllowPackageSave -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-save.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Rerun Audit. Expected: `Unchanged`, no errors, and only named packages changed. Inspect every binary path before staging.

- [ ] **Step 10: Execute visible Gate A PIE proof**

Open `Lvl_ThirdPerson1`; the level must be load-and-playable without manual reassignment. Capture telemetry and video for:

1. Held guard entered with no current parry target.
2. One normal block with selected montage, VFX, audio, zero damage, and intended `Continue` or `Recoil`.
3. One Block Press in a reviewed window producing perfect parry, delayed marker-driven `CounterWindow`, paired counter, finisher continuity, AI token release/recovery, and no repeated damage.
4. Out-of-hard-cone contact producing hit.
5. Valid timing with insufficient perfect alignment downgrading to guard/normal block.
6. Four enemies obeying default one-attacker token policy and not damaging one another.
7. The same enemy can recover and initiate another attack while the player remains in range; no leave/re-enter reset is required.

Telemetry acceptance: final per-frame yaw is within `rate * simulation delta + 0.1 degrees`; normal-block horizontal drift is at most 1 cm; each parry-bridge actor moves at most 75 cm; stage-handoff unexpected displacement is at most 10 cm; pelvis discontinuity is at most 15 cm. Missing visible assets or missing telemetry blocks Gate A.

- [ ] **Step 11: Verify and commit Slice 6**

Build first, then run editor validation/migration tests, all defense tests, and full automation. Complete the slice adversarial/spec-coverage gate. Update `docs/guides/HEADLESS_ASSET_MIGRATIONS.md` with exact operation usage and create a short Gate A evidence handoff under `docs/handoffs/` naming reports, telemetry, changed assets, and proof limits. Commit source/docs separately from reviewed binary assets when practical:

```powershell
git commit -m "Add defense proof validation workflow"
git commit -m "Prove the LightAttack_1 defense slice"
```

Gate A does not authorize Gate B or broad defense claims.

---

### Task 7: Build And Accept The Gate B Defense Matrix

**Files:**
- Add after inventory: `Tools/Codex/manifests/defense-gate-b.json`
- Add: `Config/AssetMigrations/DefenseGateBTargets.txt`
- Modify: `Source/KatanaCombatTest/Private/DefenseAssetValidationTests.cpp`
- Modify: `Source/KatanaCombatTest/Private/DefenseInputThreatTests.cpp`
- Modify after reviewed plan: exact Gate B AttackData/montage/configuration packages
- Add through commandlet/UEMCP: `Content/ProjectFiles/Levels/Test/Lvl_DefenseMatrix.umap`
- Add through commandlet/UEMCP: dedicated proof configuration assets required by that map
- Modify: `CLAUDE.md`
- Modify: `docs/architecture/ARCHITECTURE_QUICK.md`
- Modify: `docs/specs/PAIRED_ANIMATION_SPEC.md`
- Modify: `docs/superpowers/specs/2026-07-16-defense-interaction-design.md`
- Add: `docs/handoffs/2026-07-16-defense-gate-b-acceptance.md`

**Produces:** Reviewed High/Middle/Low x Left/Center/Right proof, semantic attacker-response coverage, two-active-threat evidence, broad automation/build evidence, and canonical documentation of implemented versus remaining behavior.

- [ ] **Step 1: Inventory and select the smallest complete attack set**

Use read-only commandlet reports and Editor review to select at least three authored attacks covering High, Middle, and Low. Each attack must support controlled Left, Center, and Right trajectories, yielding all nine matrix cells through exact or explicitly documented fallback rows.

The manifest must also name:

- One `BlockInterruptible` attack producing `Recoil`.
- One blockable continuing attack producing `Continue`.
- One unblockable attack producing `UnblockableHit`.
- An unblockable-plus-parryable case only when deliberately authored and visually reviewed.
- Exact source sockets, target bones, block/parry/response rows, VFX/audio, counter/finisher dependencies, and expected outcomes/reasons.

Do not use HeavyAttack_1 through HeavyAttack_4 until their separate notify debt is resolved and their timing is reviewed.

- [ ] **Step 2: Make Gate B coverage a commandlet invariant**

Extend post-report validation to fail unless all nine height/lane cells exist, all required semantic response cases exist, every specialized row has deterministic fallback provenance, and the dependency manifest is closed. A report that merely loads assets is not success.

- [ ] **Step 3: Create a dedicated playable proof map**

Use commandlet/UEMCP/Editor automation to create and save `Lvl_DefenseMatrix` with controlled attacker spawn anchors for Left/Center/Right and deterministic High/Middle/Low case selection. Create dedicated proof AI/combat-token settings allowing exactly two concurrent attackers for the multi-threat fixture; do not change the shipping/default one-attacker policy. The map must load and run without user wiring.

Add a deterministic debug controller or existing StateTree configuration that starts named cases, records scripted predicted deadlines/stable IDs, and restores the fixture between cases. Do not add UI; console/debug controls are sufficient.

- [ ] **Step 4: Plan, apply, save, and re-audit exact Gate B assets**

Use the same Audit -> Plan -> Apply -> ApplyAndSave -> Audit sequence as Gate A with distinct `defense-gate-b-*.json` reports. Require explicit timing/save gates. Review the changed-package set before save and before staging.

- [ ] **Step 5: Prove multi-active-threat behavior**

Automation and PIE must create two simultaneous valid active attacks with scripted deadlines. Prove stable-ID tie break, lock hysteresis, switch lead, current-threat invalidation, stale-prediction downgrade, one candidate enumeration per opportunity, and same selected identity across failed parry/guard fallback. Restore/reload the fixture after capture and verify default gameplay settings still allow only one attacker.

- [ ] **Step 6: Execute all nine visible cases and semantic cases**

Capture outcome/reason, selected/fallback row, contact bone, lane/height/swing, VFX/audio, damage, attacker response, yaw/displacement, stage continuity, and cleanup for each cell. Repeat at representative 0.5x, 1.0x, and 2.0x montage rates and under time dilation for cap parity. Any threshold breach, ambiguous row, repeated effect, friendly damage, stuck token, frozen pose, direct rotation, or incomplete cleanup blocks acceptance.

- [ ] **Step 7: Run final automated verification**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "Tools\Codex\run-agent-baseline.ps1"
powershell -ExecutionPolicy Bypass -File ".agents/skills/katana-verify/scripts/summarize-automation-log.ps1"
git diff --check
git status --short
```

Expected: editor build succeeds, all `KatanaCombat` automation completes with zero failures/errors, commandlet post-audits are clean, and only reviewed source/docs/manifests/assets are changed.

- [ ] **Step 8: Reconcile canonical documentation with proven state**

Mark each contract as implemented only where automated and PIE evidence exists. Keep future replication, Chooser backend, Contextual Animation migration, and guard-break resource out of scope. Record exact report/telemetry/video paths and residual tuning debt in the Gate B handoff. Do not rewrite historical docs as though they had always described the final implementation.

- [ ] **Step 9: Adversarial final review**

Review for duplicate resolution, stale callback cleanup, cross-owner warp/time/context release, input fallthrough, mutable second reads, event reentrancy, first-commit ordering, terminal tombstone eviction, actor destruction, neutral-team compatibility, missing fallback assets, configuration precedence drift, root-motion cap bypass, and headless/PIE overclaim. Fix every high/medium finding and add a regression test before merge readiness.

- [ ] **Step 10: Run the final intent-satisfaction and traceability audit**

Build a closure table in `docs/handoffs/2026-07-16-defense-gate-b-acceptance.md` with one row for every normative contract in the accepted spec. Each row records:

```text
Spec section/contract | Owning slice | Implementation files/assets |
Automation evidence | Commandlet/static evidence | PIE/telemetry evidence |
Status | Residual risk
```

Use only `Proven`, `Partial`, `Not Implemented`, or `Out Of Scope` for status. Source structure alone cannot produce `Proven` for runtime or animation behavior. Reconcile the original branch intent explicitly: held guard, contact-driven normal block, edge-triggered perfect parry, immutable defender/attacker presentation, bounded alignment, attack consumption, AI recovery, retained counter-to-finisher continuity, friendly-fire policy, repeatable proof fixtures, and no premature UI coupling.

Compare the final branch diff to the Task 0/0A baseline and classify every changed source, config, manifest, and binary package as required, supporting evidence, or unrelated. Any in-scope `Partial`/`Not Implemented` row, unexplained package, stale canonical claim, or missing evidence blocks completion. Update the durable execution checkpoint with the final table location, remaining out-of-scope work, and exact merge-readiness decision.

- [ ] **Step 11: Commit Gate B and prepare review**

Stage intentional paths only. Separate large binary content from source when that improves reviewability. Use an imperative commit such as:

```powershell
git commit -m "Prove the full defense interaction matrix"
```

## Completion Definition

The feature is complete only when Task 0A records a supported `GO`, every slice has a refreshed checkpoint and closed high/medium adversarial findings, all seven slices are committed, the full suite is green, Gate A and Gate B post-audits are clean, both visible proof captures satisfy telemetry thresholds, the maps load without manual setup, and the final intent table has no in-scope `Partial` or `Not Implemented` row. Canonical docs must distinguish proven behavior from future work. A green build, source inspection, commandlet load, remembered prior result, or one-attack playtest alone is insufficient.

## Execution Handoff

Recommended execution is `superpowers:subagent-driven-development` with one fresh worker per slice and the mandatory context, validation, adversarial, and spec-coverage gates after each slice. `superpowers:executing-plans` is acceptable when work proceeds in a separate session. Every fresh or resumed worker starts from the live context-refresh preflight and verified execution checkpoint; conversation memory or a prior worker summary is orientation only. Do not parallelize slices that modify `CombatTypes.h`, `CombatComponent`, or paired state; their contracts are sequential. Editor inventory and independent test-file preparation may run in parallel only after the owning slice APIs are stable.
