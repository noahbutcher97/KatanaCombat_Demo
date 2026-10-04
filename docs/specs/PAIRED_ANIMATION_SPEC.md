# Paired Animation System - Technical Specification

> **Version**: 2.3 | **Date**: 2026-07-18 | **Status**: Runtime, Gate A, and scoped single-player Gate B accepted
> **Reference Plan**: `.claude/plans/synthetic-painting-ritchie.md`

---

## 1. SYSTEM OVERVIEW

### 1.1 Purpose
A production-quality paired animation system enabling synchronized two-character combat sequences including:
- **Finishers**: Cinematic kill animations triggered by explicit low-health, stagger, or contextual eligibility
- **Counters**: Reactive attack animations following successful parries
- **Parries**: Defensive paired animations with attacker/defender roles

### 1.2 Design Goals
1. **Self-Correcting**: Runtime procedural adjustments heal sync drift and contact misalignment
2. **Self-Healing**: Automatic recovery from edge cases (death, interruption, component destruction)
3. **Data-Driven**: Configuration via UPairedAnimationData assets + procedural math
4. **Editor-Time Analysis**: Pre-calculate montage timing, contact points, reach requirements
5. **Production Quality**: Inspired by AC3, Ghost of Tsushima, Batman Arkham series

### 1.3 Architecture Layers

| Layer | Purpose | Timing |
|-------|---------|--------|
| **Editor-Time** | Analyze montages, populate viable fields, validate pairs | Asset import/save |
| **Runtime Procedural** | IK corrections, position nudges, sync point healing | Every frame during paired anim |
| **Utility Libraries** | Reusable math, geometry, spatial, skeletal analysis | On-demand |

---

## 2. DATA STRUCTURES

### 2.1 Core Data Asset: UPairedAnimationData

```cpp
// Single unified data asset (not split by reaction type)
UCLASS(BlueprintType)
class UPairedAnimationData : public UPrimaryDataAsset
{
    // Animation References
    UAnimMontage* AttackerMontage;
    UAnimMontage* VictimMontage;
    FName AttackerMontageSection;  // Optional section for multi-finisher montages
    FName VictimMontageSection;

    // Timing
    float SyncPointTime;           // When damage/effect triggers
    float AttackerBlendIn;
    float VictimBlendIn;
    float VictimStartOffset;       // Timing offset for victim montage

    // Positioning
    FVector VictimRelativeOffset;  // Offset from attacker
    bool bVictimFacesAttacker;

    // Warp Configuration
    FPairedWarpConfig AttackerWarp;
    FPairedWarpConfig VictimWarp;

    // Combat
    float BaseDamage;
    float DamageMultiplier;
    bool bIsLethal;                // Force kill regardless of current health
    EReactionOutcome VictimDeathOutcome;  // Ragdoll, FreezeAtPose, etc.

    // Cinematic Effects
    bool bApplySlowMotion;
    float SlowMotionScale;
    float SlowMotionDuration;
    TSubclassOf<UCameraShakeBase> ImpactCameraShake;
    bool bApplyHitPause;
    float HitPauseDuration;

    // Audio (Wired in v3.5.0 - f27a068)
    USoundBase* ImpactSound;           // Plays at contact point
    USoundBase* VictimReactionSound;   // Plays on victim
    USoundBase* AttackerVoiceLine;     // Plays on attacker
    float MusicDuckingDB;              // [Scaffold] Not wired

    // VFX (Wired in v3.5.0 - f27a068)
    UNiagaraSystem* ImpactVFX;         // Spawns at contact point
    UMaterialInterface* SlowMoPostProcessMaterial;  // [Scaffold] Not wired
    UMaterialInterface* ScreenBloodMaterial;        // [Scaffold] Not wired
    bool bSpawnBloodDecals;            // [Scaffold] Not wired
};
```

### 2.2 Combat Math

Direction math is owned by `Utilities/CombatMath.h` (guarded angles, cones, signed yaw,
four-way classification and the `DirectionToAttacker` hit-direction convention). Target
gathering is owned by `Utilities/CombatTargetQuery.h`. The earlier CombatMathTypes.h /
CombatMathEnums.h foundation had no consumers and was removed in 2026-09.

---

## 3. RUNTIME FLOW

### 3.1 Finisher Execution Flow

```
Player Input (Attack during finisher opportunity)
    │
    ▼
PairedAnimationComponent::TryExecuteFinisher()
    ├─ Validate: Target in range (SoftAimRange)
    ├─ Validate: Target vulnerable (IsVulnerableToFinisher)
    ├─ Validate: Path clear (ValidatePairedAnimation)
    ├─ Acquire: paired-input ownership lease
    ├─ Set: bIsFinisherTarget = true (mutex, on PairedAnimationComponent)
    ├─ Store: CurrentFinisherVictim reference (on PairedAnimationComponent)
    │
    ▼
Setup Warp Tracking (TargetingComponent)
    ├─ SetupAttackerPairedWarp() → Owned tracking toward victim
    ├─ SetupVictimWarp() → Owned tracking relative to attacker
    └─ Register paired partner/collision ownership for both roles
    │
    ▼
Play Montages (Both Characters)
    ├─ Attacker: AttackerMontage at configured section
    └─ Victim: VictimMontage with timing offset
    │
    ▼
AnimNotifyState_PairedAnimationSync (Sync Point Reached)
    ├─ Legacy sequence owner validates its accepted target (warn if > MaxContactDistance)
    ├─ Apply optional owner-authorized nudge; retained defense keeps scoped alignment
    ├─ Trigger hitstop through actor time-dilation leases
    ├─ Trigger slow motion through a world time-dilation lease
    ├─ Trigger camera shake (TriggerCameraShake)
    ├─ If primary and bApplyDamage: commit generation-owned damage exactly once
    │   ├─ The exact paired partner bypasses stale guard, i-frame, invulnerability, and resistance state
    │   └─ Lethal finishers enter Dying immediately; the paired death outcome remains pending
    └─ Broadcast OnPairedAnimationSyncPoint delegate
    │
    ▼
OnMontageEnded → CompletePairedAnimation()
    ├─ Apply damage only as a compatibility fallback when no authored damage sync fired
    ├─ Finalize any pending paired death outcome after lethal health was committed
    └─ Release only this operation's input, warp, time, collision, and partner ownership
```

### 3.2 Vulnerability Triggers (Priority Order)

1. **Explicit Context** (Highest) - Scripted or tagged finisher eligibility owned by the active combat interaction
2. **Staggered** - Contextual stagger is active
3. **Low Health** (Lowest) - Below the configured threshold

Posture depletion and posture-based guard break are deprecated compatibility behavior and are not finisher triggers for new runtime logic or content.

### 3.3 Chain Counter Runtime Contract

Chain Counter is required behavior. It is not experimental and is not accepted through protected helper calls. The source-level flow below is implemented; Gate A provides accepted rendered asset/runtime evidence for the canonical path.

Runtime flow:
1. Defender presses Block.
2. `UCombatComponent` gathers incoming attacks once and resolves input intent through the centralized defense resolver.
3. Perfect parry requires attacker-side `AnimNotifyState_ParryWindow`, `Attack.Defense.Parryable`, hostile team policy, stable attack identity, and reachable alignment.
4. On committed perfect parry, `UPairedAnimationComponent` stores an active Chain context containing parried target, source attack metadata, current Chain state, generation, timeout and ownership handles. Selected counter `UAttackData`, `CounterData`, and `FinisherData` remain null until attack input selects them in `CounterWindow`.
5. The parry bridge enters `CounterWindow` only from the exactly-one reviewed marker on its configured driver role. The marker opens the response window without releasing bridge pose ownership; the window's deadline owns it from then on. When the defender's bridge montage ends, the defender is released for the rest of the window (input, movement, partner collision, alignment, stage slow motion and paired status), while the source attacker stays held in its ready pose, paired-victim state, movement lease and AI suppression until the window resolves. A defender hit reaction, stun or stagger ends the sequence and releases the attacker. The no-montage fallback retains the same sequence context until its simulation-time callback and releases the defender when the window opens. Normal Block fallback does not create a Chain context.
6. Light or Heavy input while Chain is waiting is captured as `ChainOnly`, resolves `UAttackData` through `UCombatComponent::GetAttackForInput()`, and calls `UPairedAnimationComponent::TryAdvanceChainCounter(UAttackData*)`. Failure expires that edge without normal-attack fallthrough.
7. The counter step uses selected `UAttackData::CounterData` first, attacker notify `SpecificCounterData` only when explicitly allowed, then non-paired counter fallback.
8. Counter paired steps cannot reduce the target below one health by default. Lethality requires both authored `bIsLethal` data and the component's explicit `bAllowLethalCounterPairedData` opt-in; otherwise the finisher owns lethal damage.
9. Paired counter completion either transitions in place to `FinisherActive` with stored context or deliberately enters a timed `FinisherReady` state without dropping sequence ownership; once the counter montage has ended, the defender is released for that window as for `CounterWindow`. Chain-only attack input retries retained `FinisherData`; it cannot fall through to a normal attack. A Light or Heavy press during `CounterActive` is captured `ChainOnly` when the counter can reach `FinisherReady` (it has finisher data and either waits for input or has a retryable automatic finisher): one newest-wins slot keyed to the interaction, executed when `FinisherReady` opens, expired when an automatic finisher starts without it, and discarded at cleanup. During a counter with no finisher to reach, presses stay rejected.
10. The sequence owns a scoped `Context.ParryCounter` lease through all active Chain states. Timeout, montage interruption, paired cancel, partner death, owner death, invalid target, failed montage start, and normal completion release it and all other stage-owned handles exactly once. A stage start that fails after it already stopped the defender's outgoing montage rolls back to the previous stage and treats that montage as ended: in a waiting window the defender is released at once, and after a failed automatic finisher it is released when `FinisherReady` opens. An outgoing montage still playing after the rollback keeps its own end callback.

Canonical paired bridge and stage data must provide valid sections, named rotation-warp targets for both roles, and `bWarpRotation` for both roles. The driver montage contains exactly one matching transition marker. Adjacent stages cannot reuse the same montage for the same role because Unreal's montage-end callback does not carry enough instance identity to disambiguate the retired callback. A stage also refuses to start while either role still has an unresolved retired callback for its montage. Ready sections or reviewed terminal-pose compatibility are required wherever the sequence waits for input. A parry bridge's defender montage must keep auto blend-out enabled: its end releases the defender and catches a missed marker, and a montage that holds its last frame never ends.

### Chain Counter Implementation Evidence

- Public Block-input commit is covered by `KatanaCombat.Defense.Parry.BlockPressConsumesAttack` and `KatanaCombat.Defense.Input.NewBlockPressRetriesParry`.
- Public attack-input ownership and no-fallthrough behavior are covered by `KatanaCombat.Defense.Input.ChainPreflightFailureExpires` and `KatanaCombat.Defense.Chain.RetryableFinisher`.
- Active target/context retention, marker-driven handoff, and terminal cleanup are covered by `KatanaCombat.Defense.Chain.RetainedStageLifecycle`, `.MarkerIdentity`, and `.CancellationMatrix`.
- Counter damage is nonlethal by default and lethal only through explicit opt-in, covered by `KatanaCombat.CounterSystem.ChainCounterDamagePolicyNonLethalByDefault` and `KatanaCombat.Defense.Chain.LethalCounterOptIn`.
- Attack input resolves selected `UAttackData` in `UCombatComponent` and advances through `UPairedAnimationComponent::TryAdvanceChainCounter(UAttackData*)`.
- Counter data resolution remains selected `UAttackData::CounterData`, explicit notify fallback, then non-paired fallback.
- Asset-backed montage proof remains separate from source-level automation. Gate A evidence is recorded in `docs/handoffs/2026-07-17-defense-gate-a-evidence.md`; Gate B acceptance and residual limits are recorded in `docs/handoffs/2026-07-16-defense-gate-b-acceptance.md`.
- Gate A rendered PIE proves the canonical bridge/counter/finisher path, marker handoff, bounded alignment, VFX/audio, damage staging, cleanup, and adjacent-frame continuity across `ParryActive -> CounterWindow -> CounterActive -> FinisherActive`. This does not generalize production-quality pose compatibility across the attack catalog.

### Chain Counter Semantics Ownership

Chain Counter keeps state and results explicit. `EChainCounterState` owns the active state machine; `UCombatComponent` resolves `EInputType` into selected `UAttackData`; `UPairedAnimationComponent` owns retained target/context and paired continuation. Tags may gate contextual eligibility, such as `Context.ParryCounter`, but tags do not replace Chain state.

Counter and finisher payloads are data references. The selected defender `UAttackData::CounterData` and `FinisherData` identify the paired animations to play. Booleans such as `bHasCounterVariant` and `bCanTriggerFinisher` are readiness gates and must be validated against those references.

Attack and defense properties that are extensible across authored content belong in tags. `Attack.Property.Unblockable`, `Attack.Defense.Parryable`, and `Attack.Defense.BlockInterruptible` are implemented inputs to the centralized defense resolver, which returns an explicit enum decision before any montage, damage, VFX, or audio payload is executed.

The ownership policy is defined in `docs/superpowers/specs/2026-07-02-combat-semantics-ownership-design.md`.
The revised target defense outcome matrix, rich-contact handoff, attack identity,
alignment, parry bridge, and retained counter-to-finisher transition contract are
defined in `docs/superpowers/specs/2026-07-16-defense-interaction-design.md`. It
is the accepted implementation authority for defense integration. Source slices 1-5 implement the runtime contracts, Gate A accepts the canonical asset-backed Chain path, and Gate B accepts the scoped single-player matrix, attacker responses, simultaneous-threat arbitration, unblockable contact, and perfect-parry regression. Multiplayer ordering, production AI, and catalog-wide pose quality remain outside that acceptance.

---

### 3.4 Upright Paired Targets and Ground Support

Paired attacker and victim targets use the partner's yaw to rotate `RelativeOffset` during setup and each motion-warp pre-update. `FPairedWarpConfig::FacingPolicy` independently selects `FacePartner` (the existing asset default), `FaceAwayFromPartner`, or `MatchPartnerHeading`. Enabled facing is yaw-only; terrain height and partner pitch/roll cannot tilt the character. Coincident horizontal positions retain the owner's heading for positional policies; matching uses the partner's yaw even at coincidence. Disabling rotation retains the owner's current rotation at setup and refresh.

`UPairedAnimationUtilityLibrary::ResolvePairedFacingRotation` is the actor-independent resolver used by direct paired targets and retained alignment. Direct victim positional facing is measured from its computed warp endpoint; direct attacker and retained positional facing are measured from the owner, preserving those existing contracts. Retained requests carry the role policy and resolve it at acquisition/update; this change does not add an automatic pre-update subscription to retained alignment. Their coincidence fallback is the request's desired heading. Bridge/stage turn-budget checks use the selected policy, while default defender bridge checks retain the resolved defense-contact yaw. Unknown policies fail validation before replacing ownership. `KatanaCombat.Targeting.PairedFacing.*` and `KatanaCombat.Defense.Chain.StagePreflightGeometry` cover policy resolution, refresh, lifecycle and retained-stage propagation.

When terrain adjustment is enabled, `UDebugUtils::SampleWalkableGroundAtLocation` searches downward along world Z for static or dynamic environment support. Pawns and their owned/attached objects are ineligible even if their collision object type advertises environment geometry. Accepted hits must satisfy the character movement component's walkability policy, permit character step-up and block the character's capsule channel. Rejected components are skipped, with at most 32 trace attempts. A missing eligible hit preserves the requested location; a valid hit supplies ground Z plus capsule half-height. This is a support sample, not a capsule-clearance or reachability guarantee.

`Combat.Debug.GroundSampling 1` logs queried locations, actual hit actor/component identity and ground eligibility without debug drawing; its default is zero. `KatanaCombat.Targeting.PairedGrounding.*` covers initial targets, continuous refresh, character-owned geometry, steep ground, moving platforms, missing ground and upright facing through public APIs. See [grounding verification](../audits/PAIRED_WARP_GROUNDING_2026-09-10.md) for the rendered finisher evidence and remaining translation/contact defects. Target construction does not override montage modifier flags or enable CharacterMovement when a paired collision notify disables it.

### 3.5 Sync Correction Ownership

`UPairedAnimationComponent::GetOwnedSyncCorrectionTarget` resolves a direct sync
correction only for an active legacy paired sequence's accepted current target.
Partner-list order cannot select the target, and tracking a partner alone does
not establish authority. Participant notifies, removed targets and ended
participation cannot reposition either actor. Retained defense sequences keep
their scoped alignment requests and turn budgets; sync notifies do not add direct
position corrections to those sequences.

The notify resolves this authority after damage/presentation callbacks, since
those callbacks can end participation. Primary-sync, validation, nudge-enable and
distance gates still apply. The existing nudge remains an instantaneous position
correction; ownership does not establish bounded entry motion or visible contact.
`KatanaCombat.PairedAnimation.SyncAlignment.*` exercises notify order, accepted
targets, inactive participation and authored gates through public APIs.
`KatanaCombat.Defense.Chain.SyncPreservesAlignmentOwnership` exercises committed
defense ownership through public Block input.

The [ownership verification and entry-motion audit](../audits/PAIRED_SYNC_ALIGNMENT_OWNERSHIP_2026-09-11.md)
records the rendered correction roles, remaining motion-warp amplification and
contact limitations.

### 3.6 Bounded Legacy Paired Entry

`UPairedAnimationData::Entry` optionally prepares the accepted legacy pair before either montage starts. It defaults to disabled. `VictimRelativeTransform` declares the victim's upright preparation pose relative to the initiator; it is separate from the montage warp endpoint. No existing asset is automatically opted in.

`AlignmentMotionLibrary` calculates bounded translation/yaw steps without actor dependencies. `UTargetingComponent` executes them through swept character movement under a scoped alignment request. The initiator holds its accepted world pose; the victim approaches the relative goal. Accepted duration, speed, travel, turn-rate, turn-budget and tolerances cannot be extended by updating the request. Limits use world simulation seconds: component delta is divided by positive actor time dilation. Zero or invalid actor time dilation fails closed; global pause/slowdown retain world-clock semantics.

Preflight rejects invalid policy, missing playback dependencies and initially unreachable geometry before takeover. Preparation owns exact alignment and paired-state leases, stops locomotion and ignores collision only between the accepted partners. Environment collision remains active. Both live poses must satisfy readiness before playback. Obstruction, deadline/budget exhaustion, participant loss, ownership preemption or cancellation releases those leases and aborts without paired damage or a teleport back. Completion and sync callbacks cannot advance a pending preparation. An entry-enabled pair also suppresses the legacy sync nudge after preparation.

Preparation bounds do not cap subsequent montage root motion. This is a collision-aware placement mechanism, not navigation, an authored approach animation, foot-support validation or contact approval. Those require separate policy and evidence. Coverage lives in `KatanaCombat.Targeting.BoundedAlignment` and `KatanaCombat.PairedAnimation.Entry`; the rendered runner accepts transient entry overrides described in the [evaluation guide](../guides/PAIRED_ANIMATION_EVALUATION.md).

## 4. EDITOR-TIME ANALYSIS

### 4.1 UMontageAnalyzerTools (Base Class)

```cpp
// Timing Analysis
static float GetMontageDuration(UAnimMontage* Montage, FName Section);
static TArray<FAnimNotifyEvent> GetNotifiesInRange(float StartTime, float EndTime);
static float FindSyncPointTime(UAnimMontage* Attacker, UAnimMontage* Victim);

// Bone Trajectory Analysis
static TArray<FBoneFrameTransform> SampleBoneTrajectory(int32 SampleCount);
static FVector GetBoneVelocityAtTime(float Time);
static float GetMaxBoneSpeed(FName BoneName);

// Contact Point Prediction
static FContactPointPrediction PredictContactPoint(FName AttackBone, FName VictimBone);
static TArray<FContactPointPrediction> FindAllContactPoints();

// Reach Analysis
static FReachQueryResult AnalyzeReachRequirement(FName EffectorBone, FVector TargetOffset);
static bool ValidateReachability(const FSkeletalHierarchy& Skeleton, FName ChainTip, FVector Target);

// Validation
static bool ValidatePairedMontages(TArray<FString>& OutErrors);
static bool ValidateSyncPointAlignment(TArray<FString>& OutWarnings);
```

### 4.2 SMontageAnalysisDashboard (Slate UI)

Features:
- Timeline view with both montages aligned
- Bone trajectory 3D preview
- Contact point markers with confidence scores
- Sync point alignment indicators
- Reach envelope visualization
- Error/warning list with auto-fix suggestions
- Export analysis to PairedAnimationData fields

---

## 5. ANIMINSTANCE INTEGRATION

### 5.1 SamuraiAnimInstance Extensions

```cpp
// Paired Animation State (synced from PairedAnimationComponent)
bool bInPairedAnimation;
TWeakObjectPtr<AActor> PairedPartner;
float CurrentSyncProgress;  // 0-1 through sync point

// IK Targets (set by procedural system, consumed by Control Rig)
FVector ContactPointIKTarget;
FName EffectorBone;
float IKBlendAlpha;  // 0 = animation pose, 1 = IK target

// Spine Pitch (For Honor technique)
float SpinePitchDegrees;

// Runtime Correction API
void ApplyContactPointCorrection(FName Bone, FVector WorldTarget, float BlendSpeed);
FVector GetIKTargetForBone(FName BoneName);
void CalculateSpinePitchForPartner(AActor* Partner);
```

### 5.2 ABP_SamuraiAnimInstance Blueprint Integration

Minimal Blueprint nodes:
- Read `ContactPointIKTarget` → Feed to Control Rig IK node
- Read `SpinePitchDegrees` → Apply to spine bones via additive layer
- Read `IKBlendAlpha` → Control blend between animation and IK

---

## 6. UTILITY LIBRARIES

### 6.1 Existing Libraries (Extend)

| Library | Functions to Add |
|---------|------------------|
| PairedAnimationUtilityLibrary | Contact point calculation, reach validation |
| CinematicEffectsUtilityLibrary | Unified time dilation coordinator |

### 6.2 Shared Math and Queries

| Library | Purpose |
|---------|---------|
| CombatMath | Direction math and the hit-direction convention |
| CombatTargetQuery | Shared, filtered target gathering |

---

## 7. DEBUG VISUALIZATION

### 7.1 CVars

```
Combat.Debug.PairedAnim 1             // Enable all paired animation debug
Combat.Debug.PairedAnim.Warp 1        // Warp targets (cyan crosshairs)
Combat.Debug.PairedAnim.Partners 1    // Partner connections (yellow lines)
Combat.Debug.PairedAnim.Sync 1        // Sync points (magenta spheres)
Combat.Debug.PairedAnim.Vulnerability 1 // Finisher vulnerability indicators
```

### 7.2 HUD Panel Elements

```
╔═══════════════════════════════════════╗
║ PAIRED ANIMATION DEBUG                ║
╠═══════════════════════════════════════╣
║ State: EXECUTING_FINISHER             ║
║ Role: ATTACKER                        ║
║ Partner: BP_EnemyCharacter_01         ║
╠───────────────────────────────────────╣
║ Warp Tracking:                        ║
║   Attacker Warp: ACTIVE (120u)        ║
║   Victim Warp:   ACTIVE (85u)         ║
║   Distance:      142.3u (Max: 300)    ║
╠───────────────────────────────────────╣
║ Vulnerability:                        ║
║   Target Vulnerable: YES              ║
║   Reason: GuardBroken                 ║
║   Health: 18% (Threshold: 25%)        ║
╠───────────────────────────────────────╣
║ Sync Point:                           ║
║   Name: FinisherImpact                ║
║   Progress: 0.65s / 0.80s             ║
║   Alignment: OK (32u < 150u max)      ║
╠───────────────────────────────────────╣
║ Effects Active:                       ║
║   SlowMo: 0.3x (1.2s remaining)       ║
║   Hitstop: ---                        ║
║   CameraShake: Queued                 ║
╚═══════════════════════════════════════╝
```

---

## 8. VERIFICATION CRITERIA

### 8.1 Functional Requirements

| Requirement | Test Criteria |
|-------------|---------------|
| Finisher triggers | Explicit context, configured low health, or contextual stagger; never deprecated posture guard break |
| Victim dies correctly | No double death animation, direct ragdoll/freeze |
| Input blocking | Player cannot attack/evade during finisher |
| Warp tracking | Both characters track each other continuously |
| Sync point | Distance < 150u at impact, auto-nudge if < 50u |
| Time dilation | Overlapping world and actor leases recompute effective dilation and restore the captured prior value |
| Hitstop | Both characters freeze at impact |
| Camera shake | Triggers at sync point |
| Partner death | Animation cancels gracefully on either death |
| Obstacle validation | Finisher blocked if path obstructed |

### 8.2 Test Coverage

34 automated tests in PairedAnimationTests.cpp covering:
- Sync point timing and delegates
- Warp tracking and cleanup
- Collision management
- Finisher flow and vulnerability
- Cinematic effects

---

## 9. FILES REFERENCE

### 9.1 Core Implementation

| File | Purpose |
|------|---------|
| PairedAnimationComponent.cpp | TryExecuteFinisher, CompletePairedAnimation, TriggerSyncPointEffects |
| HitReactionComponent.cpp | IsVulnerableToFinisher, death flag handling |
| TargetingComponent.cpp | SetupVictimWarp, SetupAttackerPairedWarp |
| AnimNotifyState_PairedAnimationSync.cpp | Sync point validation, effects trigger |
| CinematicEffectsUtilityLibrary.cpp | ApplySlowMotion, ApplyHitstop |
| PairedAnimationUtilityLibrary.cpp | ValidatePairedAnimation, IsPathClear |

### 9.2 Data Types

| File | Purpose |
|------|---------|
| PairedAnimationData.h | Main data asset |
| PairedAnimationTypes.h | FPairedWarpConfig, FFinisherTriggerConfig |
| Utilities/CombatMath.h | Direction math and hit-direction convention |
| Utilities/CombatTargetQuery.h | Shared target gathering |

### 9.3 Editor Tools (Phase 5d)

| File | Purpose |
|------|---------|
| MontageAnalyzerTools.h/.cpp | Base montage analysis |
| PairedMontageAnalyzer.h/.cpp | Paired-specific analysis |
| MontageAnalysisDashboard.h/.cpp | Slate UI dashboard |

---

## 10. PHASE ROADMAP

| Phase | Focus | Status |
|-------|-------|--------|
| 5a | Foundation (data assets, utilities) | ✅ Complete |
| 5b | Core finisher flow, effects, safety | ✅ Complete |
| 5c | Math & physics utility libraries | ✅ Complete |
| 5d | Editor tools & analysis dashboard | ✅ Complete |
| 5e | AnimInstance procedural integration | ⏳ Pending |
| 5f | Awareness subsystem (runtime healing) | 🔮 Deferred |

---

**Document Maintained By**: Claude Code
**Last Updated**: 2026-02-09
