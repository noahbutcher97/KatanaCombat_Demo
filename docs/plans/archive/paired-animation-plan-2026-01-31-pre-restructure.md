# Paired Animation System - Implementation Plan

> **Last Updated**: 2026-01-30 | **Status**: Phase 5b CORE COMPLETE, Expanding to Phase 5c-5f
> **Previous Plan Archive**: `docs/plans/archive/paired-animation-plan-v2-2026-01-30.md`
> **Latest Commit**: pending - Death animation fix + finisher completion fixes
> **Expansion**: Phase 5c-5f adds sophisticated procedural systems, editor tools, and AnimInstance integration

---

## EXECUTIVE SUMMARY: Current State

**Current Status**: Phase 5b-1 through 5b-4 COMPLETE + All Critical Bug Fixes (~95% of finisher system)

**⭐ READY FOR TESTING**:
All critical finisher bugs have been fixed. Rebuild and test to verify:
1. ✅ Finisher triggers below 20% health
2. ✅ Victim dies and ragdolls/holds pose directly (no double death animation)
3. ✅ Player input unblocked after finisher completes
4. ✅ Damage applied with intelligent calculation (currentHealth + 1 for lethal)

**⭐ LATEST FIX: Death Animation Handling (Gap 21.1)**:
- **Problem**: After finisher, victim played second death animation (AM_Deaths) before ragdolling
- **Solution**: Added `bDeathHandledByPairedAnimation` flag to HitReactionComponent
- **Flow**: `SetDeathHandledByPairedAnimation()` → damage applied → `Die()` → `PlayDeathReaction()` checks flag → skips animation, applies outcome directly
- **Files**: HitReactionComponent.h/.cpp, CombatComponent.cpp

**⭐ ARCHITECTURE DECISION: Single Unified UPairedAnimationData**:
Per architecture analysis (Agent a9a13c0), using **Option A** - single data asset with `EditCondition`-based field hiding per `ReactionType`. Scales for parry → counter → finisher flow. Counter/parry-specific fields added when those systems are implemented.

**⭐ DESIGN DECISION: SoftAimRange for Finisher Distance (Gap 19.4)**:
Using `SoftAimRange` for finisher distance validation is **INTENTIONAL**, not a bug. Finisher-specific range detection wasn't detecting targets properly. Current approach works. If dedicated FinisherRange desired later, must first fix the detection issue.

**COMPLETED FIXES (This Session)**:
- ✅ Gap 21.1: Death animation handled by paired animation flag system
- ✅ Gap 20.1-20.4, 20.6: Finisher completion, damage, input, guard flag, victim tracking

**PREVIOUSLY COMPLETED**:
- ✅ Phase 5b-1/2/3/4: Core infrastructure, warp tracking, effects, validation, tests
- ✅ Gap 18.x: P0 fixes (warp cleanup, flag clearing, rollback)
- ✅ Gap 17.5: Time dilation stacking prevention
- ✅ Gap 19.1-19.3, 19.14: Warning logs, validity checks, EndPlay cleanup

**NEXT PRIORITIES** (Expanded Scope):
1. **Phase 5c**: Math & Physics Foundation - Utility libraries for spatial/skeletal analysis
2. **Phase 5d**: Editor Tools & Dashboard - UMontageAnalyzerTools + analysis dashboard
3. **Phase 5e**: AnimInstance Integration - SamuraiAnimInstance procedural extensions
4. **Phase 5f**: Awareness Subsystem - Runtime self-healing and procedural correction

**Remaining P1 Gaps** (Phase 5b):
- Gap 20.5: Sync point distance drift (characters drifting 90+ units)
- Gap 19.6: No fallback when warp blocked by obstacle
- Gap 3.3: Montage section support

**Total Gaps Tracked**: 121 (120 previous + 1 new) | **Done**: 31 | **Pending**: 76 | **Deferred**: 14

---

## EXPANDED SCOPE: Sophisticated Procedural System (Phases 5c-5f)

This expansion adds the infrastructure for a self-correcting, self-healing paired animation system with:
- **Character self-awareness** (anatomical, kinematic)
- **Enemy awareness** (gameplay and physical perspective)
- **Runtime procedural corrections** (IK, position adjustments)
- **Editor-time pre-calculation** (montage analysis, viable field population)

### Architecture Decision: Utility Libraries First, Subsystem Later

Per user consultation, the approach is:
1. **Expand existing utility libraries** (PairedAnimationUtilityLibrary, CinematicEffectsUtilityLibrary)
2. **Create new specialized utility libraries** (skeletal analysis, geometry, spatial queries)
3. **Create editor tools** (UMontageAnalyzerTools with full analysis dashboard)
4. **Later: Graduate to subsystem** once utilities are substantial

### Two-Layer Architecture

| Layer | Purpose | Timing |
|-------|---------|--------|
| **Editor-Time Analysis** | Pre-calculate static data (bone chains, reach envelopes, contact points) | Asset import/save |
| **Runtime Procedural** | Dynamic adjustments (IK corrections, position nudges, sync healing) | Every frame during paired anim |

### Files Already Created (Foundation)

| File | Purpose | Status |
|------|---------|--------|
| `Math/CombatMathEnums.h` | Foundation enums (EDistanceFormula, ESpatialQueryShape, EBoneChainType, etc.) | ✅ Created |
| `Math/CombatMathTypes.h` | Foundation structs (FSkeletalHierarchy, FBoneChain, FReachQueryResult, etc.) | ✅ Created |

---

## DOCUMENTATION UPDATE (To Apply to CLAUDE.md)

The following comprehensive update should replace the "Active Development" section in CLAUDE.md to accurately reflect the current project state:

```markdown
## Active Development & System Status

Track ongoing work across sessions. This section provides detailed status of all major systems.

### Paired Animation System (Phase 5) - PRIMARY FOCUS

**Overall Status**: ~95% of Finisher system complete, ready for testing

#### Fully Implemented (Production Ready)
| Component | Files | Description |
|-----------|-------|-------------|
| Finisher Execution Flow | CombatComponent.cpp | `TryExecuteFinisher()` → `CompletePairedAnimation()` |
| Finisher Vulnerability | HitReactionComponent.h/.cpp | `IsVulnerableToFinisher()`, `GetFinisherTriggerReason()` |
| Symmetric Warp Tracking | TargetingComponent.h/.cpp | `SetupVictimWarp()`, `SetupAttackerPairedWarp()` with continuous tracking |
| Partner Collision Management | CombatComponent.h/.cpp | `PairedAnimationPartners` array + `IgnoreActorWhenMoving()` |
| Input Blocking | CombatComponent.cpp | `bBlockCombatInput` flag in `CanProcessInput()` |
| State Transition Safety | CombatComponent.cpp | `OnPairedPartnerDeath()`, `CancelPairedAnimation()`, EndPlay cleanup |
| Death Animation Handling | HitReactionComponent.h/.cpp | `bDeathHandledByPairedAnimation` flag prevents double death |
| Damage Application | CombatComponent.cpp | Intelligent calc: `Max(damage, currentHealth + 1)` for lethal |
| Guard Flags | CombatComponent.cpp | `bCompletingPairedAnimation` prevents double execution |
| Distance Validation | CombatComponent.cpp | Uses SoftAimRange (intentional - see design decisions) |
| Sync Point Validation | AnimNotifyState_PairedAnimationSync.cpp | Alignment check with auto-nudge |
| Cinematic Effects | CinematicEffectsUtilityLibrary.h/.cpp | `ApplySlowMotion()`, `TriggerCameraShake()`, `RestoreTimeDilation()` |
| Obstacle Validation | PairedAnimationUtilityLibrary.cpp | `ValidatePairedAnimation()`, `IsPathClear()` |
| Debug Visualization | CombatDebugHUD.cpp, DebugUtils.cpp | CVars for warp targets, partner connections, sync points |
| Test Suite | PairedAnimationTests.cpp | 34 tests covering core functionality |

#### Scaffolded (Property Slots Exist, Not Wired)
| Component | Files | What Exists | What's Missing |
|-----------|-------|-------------|----------------|
| Audio Effects | PairedAnimationData.h | `ImpactSound`, `VictimReactionSound`, `AttackerVoiceLine`, `MusicDuckingDB` | No `PlaySoundAtLocation()` calls at sync points |
| VFX Effects | PairedAnimationData.h | `ImpactVFX`, `SlowMoPostProcessMaterial`, `ScreenBloodMaterial`, `bSpawnBloodDecals` | No Niagara spawning, no post-process application |
| Selective Hitstop | CinematicEffectsUtilityLibrary.h | `FreezeActors()`, `RestoreActors()` functions | Not called in finisher flow - uses world slow-mo instead |

#### Planned (Not Yet Started)
| Component | Priority | Blocker |
|-----------|----------|---------|
| Montage Section Support (Gap 3.3) | P1 | Need `AttackerMontageSection`, `VictimMontageSection` fields |
| Counter-Specific Fields | P2 | Awaiting parry→counter system design |
| Parry-Specific Fields | P2 | Awaiting parry→counter system design |
| AI Attack Token System | P2 | Phase 5b-5 - `UCombatTokenSubsystem` |

#### Key Design Decisions
1. **SoftAimRange for Finisher Distance**: Intentional. Finisher-specific detection wasn't working. SoftAimRange is proven to work.
2. **Single UPairedAnimationData**: Architecture analysis recommends Option A - single data asset with EditCondition-based field hiding per ReactionType.
3. **World Slow-Mo Over Selective Hitstop**: Simpler implementation, similar visual effect. Selective freeze available if needed later.
4. **Death Handled by Paired Animation Flag**: Prevents HitReactionComponent from playing AM_Deaths after finisher - victim montage IS the death animation.

#### Entry Points for Finisher Flow
```
Player Input → CombatComponent::TryExecuteFinisher()
  └→ HitReactionComponent::IsVulnerableToFinisher() (check target)
  └→ TargetingComponent::SetupAttackerPairedWarp() (attacker positioning)
  └→ TargetingComponent::SetupVictimWarp() (victim positioning)
  └→ PlayMontage (both characters)
  └→ AnimNotifyState_PairedAnimationSync (sync point trigger)
  └→ OnMontageEnded → CompletePairedAnimation() (damage, cleanup)
     └→ HitReactionComponent::SetDeathHandledByPairedAnimation()
     └→ IDamageableInterface::ApplyDamage() → Die() → PlayDeathReaction()
        └→ Checks flag → Skips AM_Deaths → Applies outcome directly
```

### Core Combat System - STABLE

| Component | Status | Notes |
|-----------|--------|-------|
| 4-Component Architecture | ✅ Stable | Combat, Targeting, Weapon, HitReaction |
| Input Buffering | ✅ Stable | FIFO queue, input always captured |
| Combo System | ✅ Stable | ComboWindow-based chaining |
| Posture/Guard | ✅ Stable | Guard break mechanics NOT yet implemented |
| Hit Detection | ✅ Stable | Socket-based weapon traces |
| Death System | ✅ Stable | Directional deaths, ragdoll transitions |
| Terrain Warping | ✅ Stable | Ground sampling, Z-adjustment |

### Deferred Systems (Post Phase 5)

| System | Reason | Dependency |
|--------|--------|------------|
| Predictive Terrain Analysis | Polish feature | Core combat complete |
| Foot IK Integration | Uses DebugUtils terrain awareness | Animation polish pass |
| Multi-Victim Finishers | Complex design | Single-victim finishers proven |
| Environmental Finishers | Needs architecture | Standard finishers proven |
| Network Replication | Major feature | All systems locally verified |
```

**Plans**: See `docs/plans/synthetic-painting-ritchie.md` for detailed paired animation plan and gap tracking.

---

## Overview
Implementing a production-quality paired animation system for KatanaCombat inspired by:
- **Assassin's Creed 3** (primary reference) - Parry→Counter→Finisher flow, 3200 fight animations
- **Ghost of Tsushima** - Weapon-based paired animations, "Lethality Contract", stance system
- **Batman Arkham Knight** - Freeflow combat, token-based AI coordination, IK contact points

---

## Implementation Status

### Phase 5a: Foundation (COMPLETED - Commit e7c8354)
| Component | File | Status |
|-----------|------|--------|
| `UPairedAnimationUtilityLibrary` | Utilities/PairedAnimationUtilityLibrary.h/.cpp | ✅ Complete |
| `UPairedAnimationData` data asset | Data/PairedAnimationData.h/.cpp | ✅ Complete |
| `FPairedWarpConfig` struct | Data/PairedAnimationTypes.h | ✅ Complete |
| `FFinisherTriggerConfig` struct | Data/PairedAnimationTypes.h | ✅ Complete |
| `AnimNotifyState_PairedAnimationSync` | Animation/AnimNotifyState_PairedAnimationSync.h/.cpp | ✅ Complete |
| Paired fields in AttackData | Data/AttackData.h | ✅ Complete |
| Paired animation delegates | CombatTypes.h | ✅ Complete |

### Phase 5b-1: Core Infrastructure (COMPLETED - Commit 9e67693)
| Component | File | Status |
|-----------|------|--------|
| `PairedAnimationPartners` array | CombatComponent.h/.cpp | ✅ Complete |
| `AnimNotifyState_PairedAnimationCollision` | Animation/AnimNotifyState_PairedAnimationCollision.h/.cpp | ✅ Complete |
| Movement disabling during paired anims | (in AnimNotifyState) | ✅ Complete |
| `SetupVictimWarp()` | TargetingComponent.h/.cpp | ✅ Complete |
| Victim continuous warp tracking | TargetingComponent.cpp | ✅ Complete |
| Dynamic obstruction detection | PairedAnimationUtilityLibrary + AnimNotifyState | ✅ Complete |
| Sakurai-style hitstop (platform time) | AnimNotifyState_PairedAnimationSync.cpp | ✅ Complete |

### Phase 5b-2: Delegate Wiring & Effects (COMPLETED - Commit 9e67693)
| Component | File | Status |
|-----------|------|--------|
| `CinematicEffectsUtilityLibrary` | Utilities/CinematicEffectsUtilityLibrary.h/.cpp | ✅ Complete |
| `ApplySlowMotion()` / `RestoreTimeDilation()` | CinematicEffectsUtilityLibrary | ✅ Complete |
| `TriggerCameraShake()` | CinematicEffectsUtilityLibrary | ✅ Complete |
| `ApplyHitstop()` (Sakurai-style) | CinematicEffectsUtilityLibrary | ✅ Complete |
| Time dilation restore safeguard | CinematicEffectsUtilityLibrary | ✅ Complete |
| Audio property slots (`ImpactSound`, `VictimReactionSound`) | PairedAnimationData.h | ✅ Scaffolded |
| VFX property slots (`ImpactVFX`, `SlowMoPostProcess`) | PairedAnimationData.h | ✅ Scaffolded |

### Phase 5b-3: State & Safety (COMPLETED - Commit 9e67693)
| Component | File | Status |
|-----------|------|--------|
| `TryExecuteFinisher()` | CombatComponent.h/.cpp | ✅ Complete |
| `OnPairedPartnerDeath()` | CombatComponent.h/.cpp | ✅ Complete |
| `CancelPairedAnimation()` | CombatComponent.h/.cpp | ✅ Complete |
| `bBlockCombatInput` flag | CombatComponent.h | ✅ Complete |
| Input blocking in `CanProcessInput()` | CombatComponent.cpp | ✅ Complete |
| `IsVulnerableToFinisher()` | HitReactionComponent.h/.cpp | ✅ Complete |
| `GetFinisherTriggerReason()` | HitReactionComponent.h/.cpp | ✅ Complete |
| `bIsFinisherTarget` mutex | HitReactionComponent.h | ✅ Complete |
| `SetupAttackerPairedWarp()` | TargetingComponent.h/.cpp | ✅ Complete |
| Attacker continuous warp tracking | TargetingComponent.cpp | ✅ Complete |

### Phase 5b-4: Validation & Polish (COMPLETED - Commit 37faa08)
| Component | Status | Priority |
|-----------|--------|----------|
| Sync point alignment validation | ✅ Complete | P1 |
| Time dilation stacking prevention | ✅ Complete | P1 |
| Finisher distance validation | ✅ Complete | P1 |
| Comprehensive test suite (34 tests) | ✅ Complete | P2 |
| Montage length validation (`IsDataValid()`) | ⏳ Deferred | P2 |
| Manual testing (flat/slope/obstacles) | ⏳ Runtime | P2 |
| API documentation in ATTACK_CREATION.md | ⏳ Pending | P3 |

### Phase 5b-DEBUG: Debug Visualization (⭐ PRIMARY PRIORITY)

**Purpose**: Enable efficient manual testing by visualizing every step of the paired animation process.

**Design Principles**:
- Follow existing CVar pattern (`Combat.Debug.*` namespace)
- Single data generation per frame, consumed by 3D and 2D renderers
- Three-tier information hierarchy (Primary/Secondary/Tertiary)
- Color coding: Green=success, Yellow=in-progress, Red=error, Cyan=warp, Magenta=sync

| Component | Status | File |
|-----------|--------|------|
| `Combat.Debug.PairedAnim` CVar | ⏳ Pending | Debug/DebugConfig.h/.cpp |
| `FPairedAnimDebugData` struct | ⏳ Pending | Debug/CombatDebugHUD.h |
| Paired animation HUD panel | ⏳ Pending | Debug/CombatDebugHUD.cpp |
| 3D warp target visualization | ⏳ Pending | Debug/DebugUtils.cpp |
| 3D partner connection lines | ⏳ Pending | Debug/DebugUtils.cpp |
| 3D sync point sphere | ⏳ Pending | Debug/DebugUtils.cpp |
| 3D obstruction scan visualization | ⏳ Pending | Debug/DebugUtils.cpp |
| Finisher vulnerability indicator | ⏳ Pending | Debug/CombatDebugHUD.cpp |
| Finisher setup guide | ⏳ Pending | docs/FINISHER_TESTING.md |

**3D Visualization Elements**:
1. **Warp Targets** (Cyan crosshairs): Attacker warp target, Victim warp target, offset arrows
2. **Partner Connections** (Dashed yellow line): Line between attacker and all paired partners
3. **Sync Points** (Magenta pulse sphere): Sync point location, pulse animation at trigger
4. **Obstruction Scan** (Orange sweep): Fan arc showing obstruction detection area
5. **Distance Indicators**: Range circle for finisher max distance, current distance text

**2D HUD Panel Elements** (top-right corner):
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

**Console Commands**:
- `Combat.Debug.PairedAnim 1` - Enable paired animation debug
- `Combat.Debug.PairedAnim.Warp 1` - Warp targets only
- `Combat.Debug.PairedAnim.Partners 1` - Partner connections only
- `Combat.Debug.PairedAnim.Sync 1` - Sync points only

### Phase 5b-5: AI Coordination (PENDING - After Debug)
| Component | Status | Priority |
|-----------|--------|----------|
| `UCombatTokenSubsystem` | ⏳ Pending | P1 |
| Per-target token pools (TMap) | ⏳ Pending | P1 |
| `RequestAttackToken()` / `ReleaseAttackToken()` | ⏳ Pending | P1 |
| StateTree condition `IsTargetInFinisherState` | ⏳ Pending | P2 |
| Token release on attack/finisher end | ⏳ Pending | P2 |

### Phase 5c: Math & Physics Foundation (NEW - Expanded Scope)

**Purpose**: Create foundational data types and utility libraries for sophisticated spatial/skeletal analysis.

**Design Principles**:
- Dedicated files for structs/enums (separation of concerns)
- Generic utility libraries (reusable beyond paired animations)
- UE5.6 best practices (USTRUCT, UENUM with proper specifiers)
- Blueprint-exposed for editor tooling

| Component | File | Status | Description |
|-----------|------|--------|-------------|
| `CombatMathEnums.h` | Math/CombatMathEnums.h | ✅ Created | EDistanceFormula, ESpatialQueryShape, EBoneChainType, EHandedness, EAnatomicalRegion, EContactType, EStabilityState, EIKSolverType |
| `CombatMathTypes.h` | Math/CombatMathTypes.h | ✅ Created | FSkeletalBoneInfo, FSkeletalHierarchy, FBoneChain, FReachQueryResult, FJointConstraint, FCenterOfMassResult, FBoneFrameTransform, FContactPointPrediction, FSpatialQueryResult, FSkeletalDistanceResult |
| `USkeletalAnalysisLibrary` | Utilities/SkeletalAnalysisLibrary.h/.cpp | ⏳ Pending | BuildSkeletalHierarchy(), GetBoneChain(), CalculateReachEnvelope(), CalculateCenterOfMass() |
| `UGeometryMathLibrary` | Utilities/GeometryMathLibrary.h/.cpp | ⏳ Pending | Distance calculations (Euclidean, Manhattan, Chebyshev), bounding volumes, convex hulls |
| `USpatialQueryLibrary` | Utilities/SpatialQueryLibrary.h/.cpp | ⏳ Pending | Sphere/box/cone queries, spatial partitioning helpers, FOV checks |
| `UPhysicsIntegrationLibrary` | Utilities/PhysicsIntegrationLibrary.h/.cpp | ⏳ Pending | Verlet integration, trajectory prediction, collision prediction |

**Enums Defined** (CombatMathEnums.h):
```cpp
EDistanceFormula        // Euclidean, Euclidean2D, Manhattan, Chebyshev, SquaredEuclidean
ESpatialQueryShape      // Sphere, Box, Capsule, Cone, ConvexHull
EBoundingVolumeType     // AABB, OBB, Sphere, Capsule, ConvexHull
EBoneChainType          // None, Spine, LeftArm, RightArm, LeftLeg, RightLeg, Neck, LeftHand, RightHand, Custom
EHandedness             // Left, Right, Either, Both
EAnatomicalRegion       // None, Head, Neck, Chest, Abdomen, Pelvis, UpperArm, LowerArm, Hand, UpperLeg, LowerLeg, Foot
EContactType            // None, WeaponToBody, HandToBody, FootToBody, BodyToBody, WeaponToWeapon, HandToWeapon
EStabilityState         // Stable, Marginal, Unstable, Falling
EIKSolverType           // TwoBone, FABRIK, CCD, FullBody
```

**Structs Defined** (CombatMathTypes.h):
```cpp
FSkeletalBoneInfo       // Bone name, parent, index, depth, length, children
FSkeletalHierarchy      // Root bone, all bones array, lookup map, validation
FBoneChain              // Ordered bones root-to-tip, total length, root/tip names
FReachQueryResult       // Reachable, comfortable reach, distance, extension ratio, direction
FJointConstraint        // Min/max rotation, twist axis, hinge flag, validation
FCenterOfMassResult     // COM location, total mass, stability flag, stability margin
FBoneFrameTransform     // Bone transform at specific animation frame with velocity
FContactPointPrediction // Predicted contact location, normal, time, bones, confidence
FSpatialQueryResult     // Actors/components found, distances, success flag
FSkeletalDistanceResult // Closest bones between two skeletons, points, distance, direction
```

### Phase 5d: Editor Tools & Dashboard (NEW - Expanded Scope)

**Purpose**: Create editor-time analysis tools for montage pairs with full analysis dashboard.

**Design Principles**:
- Generic base class (UMontageAnalyzerTools) subclassable for paired animations
- Full analysis dashboard (Slate UI) for visualizing analysis results
- Pre-calculate values at editor time that can be stored in data assets
- Follow existing AttackDataTools pattern in KatanaCombatEditor module

| Component | File | Status | Description |
|-----------|------|--------|-------------|
| `UMontageAnalyzerTools` | KatanaCombatEditor/MontageAnalyzerTools.h/.cpp | ⏳ Pending | Base class for montage analysis - timing, bone trajectories, contact prediction |
| `UPairedMontageAnalyzer` | KatanaCombatEditor/PairedMontageAnalyzer.h/.cpp | ⏳ Pending | Subclass for paired animation specific analysis |
| `SMontageAnalysisDashboard` | KatanaCombatEditor/Widgets/MontageAnalysisDashboard.h/.cpp | ⏳ Pending | Slate widget for full analysis visualization |
| `FMontageAnalysisResult` | KatanaCombatEditor/MontageAnalysisTypes.h | ⏳ Pending | Analysis result struct with all computed data |
| Editor asset action | KatanaCombatEditor/AssetActions/ | ⏳ Pending | Right-click context menu for PairedAnimationData |

**UMontageAnalyzerTools Functions**:
```cpp
// Timing Analysis
static float GetMontageDuration(UAnimMontage* Montage, FName Section = NAME_None);
static TArray<FAnimNotifyEvent> GetNotifiesInRange(UAnimMontage* Montage, float StartTime, float EndTime);
static float FindSyncPointTime(UAnimMontage* AttackerMontage, UAnimMontage* VictimMontage);

// Bone Trajectory Analysis
static TArray<FBoneFrameTransform> SampleBoneTrajectory(UAnimMontage* Montage, FName BoneName, int32 SampleCount);
static FVector GetBoneVelocityAtTime(UAnimMontage* Montage, FName BoneName, float Time);
static float GetMaxBoneSpeed(UAnimMontage* Montage, FName BoneName);

// Contact Point Prediction
static FContactPointPrediction PredictContactPoint(UAnimMontage* AttackerMontage, FName AttackBone,
                                                    UAnimMontage* VictimMontage, FName VictimBone);
static TArray<FContactPointPrediction> FindAllContactPoints(UAnimMontage* AttackerMontage, UAnimMontage* VictimMontage);

// Reach Analysis
static FReachQueryResult AnalyzeReachRequirement(UAnimMontage* Montage, FName EffectorBone, FVector TargetOffset);
static bool ValidateReachability(const FSkeletalHierarchy& Skeleton, FName ChainTip, FVector Target);

// Validation
static bool ValidatePairedMontages(UAnimMontage* AttackerMontage, UAnimMontage* VictimMontage, TArray<FString>& OutErrors);
static bool ValidateSyncPointAlignment(UPairedAnimationData* Data, TArray<FString>& OutWarnings);
```

**SMontageAnalysisDashboard Features**:
- Timeline view with both montages aligned
- Bone trajectory 3D preview
- Contact point markers with confidence scores
- Sync point alignment indicators
- Reach envelope visualization
- Error/warning list with auto-fix suggestions
- Export analysis to PairedAnimationData fields

### Phase 5e: AnimInstance Integration (NEW - Expanded Scope)

**Purpose**: Extend SamuraiAnimInstance with procedural capabilities for runtime corrections.

**Design Principles**:
- C++ bulk work in SamuraiAnimInstance
- Minimal Blueprint nodes in ABP_SamuraiAnimInstance
- Custom AnimNodes if needed for performance-critical operations
- Integration with existing Control Rigs (CR_Mannequin_BasicFootIK, IK_Mannequin)

| Component | File | Status | Description |
|-----------|------|--------|-------------|
| Paired animation state variables | SamuraiAnimInstance.h | ⏳ Pending | bInPairedAnimation, PairedPartnerRef, CurrentSyncProgress |
| IK target properties | SamuraiAnimInstance.h | ⏳ Pending | ContactPointIKTarget, EffectorBone, IKBlendAlpha |
| Procedural correction interface | SamuraiAnimInstance.h | ⏳ Pending | ApplyContactPointCorrection(), GetIKTargetForBone() |
| Control Rig hookup | ABP_SamuraiAnimInstance | ⏳ Pending | Wire IK targets to existing Control Rig nodes |
| Spine pitch calculation | SamuraiAnimInstance.cpp | ⏳ Pending | CalculateSpinePitch() for height differences (For Honor technique) |

**SamuraiAnimInstance Extensions**:
```cpp
// Paired Animation State (synced from CombatComponent)
UPROPERTY(BlueprintReadOnly, Category = "Paired Animation")
bool bInPairedAnimation = false;

UPROPERTY(BlueprintReadOnly, Category = "Paired Animation")
TWeakObjectPtr<AActor> PairedPartner;

UPROPERTY(BlueprintReadOnly, Category = "Paired Animation")
float CurrentSyncProgress = 0.0f;  // 0-1 through sync point

// IK Targets (set by procedural system, consumed by Control Rig)
UPROPERTY(BlueprintReadOnly, Category = "Procedural IK")
FVector ContactPointIKTarget = FVector::ZeroVector;

UPROPERTY(BlueprintReadOnly, Category = "Procedural IK")
FName EffectorBone = NAME_None;

UPROPERTY(BlueprintReadOnly, Category = "Procedural IK")
float IKBlendAlpha = 0.0f;  // 0 = animation pose, 1 = IK target

// Spine Pitch (For Honor technique for height differences)
UPROPERTY(BlueprintReadOnly, Category = "Procedural IK")
float SpinePitchDegrees = 0.0f;

// Runtime Correction API
UFUNCTION(BlueprintCallable, Category = "Procedural")
void ApplyContactPointCorrection(FName Bone, FVector WorldTarget, float BlendSpeed = 5.0f);

UFUNCTION(BlueprintPure, Category = "Procedural")
FVector GetIKTargetForBone(FName BoneName) const;

UFUNCTION(BlueprintCallable, Category = "Procedural")
void CalculateSpinePitchForPartner(AActor* Partner);
```

**ABP_SamuraiAnimInstance Blueprint Nodes** (minimal):
- Read `ContactPointIKTarget` → Feed to Control Rig IK node
- Read `SpinePitchDegrees` → Apply to spine bones via additive layer
- Read `IKBlendAlpha` → Control blend between animation and IK

### Phase 5f: Awareness Subsystem (NEW - Expanded Scope - LATER)

**Purpose**: Create runtime self-healing subsystem once utility libraries are substantial.

**Design Principles**:
- Graduate from utility libraries once patterns are established
- Centralized coordination of procedural corrections
- Event-driven (not tick-based) for performance
- Debug visualization integration

| Component | File | Status | Description |
|-----------|------|--------|-------------|
| `UCombatAwarenessSubsystem` | Systems/CombatAwarenessSubsystem.h/.cpp | 🔮 Deferred | UWorldSubsystem for runtime awareness coordination |
| Character registration | (in subsystem) | 🔮 Deferred | RegisterCombatCharacter(), UnregisterCombatCharacter() |
| Sync point healing | (in subsystem) | 🔮 Deferred | OnSyncPointApproaching(), ApplySyncCorrection() |
| Contact validation | (in subsystem) | 🔮 Deferred | ValidateContactAtFrame(), SuggestCorrection() |
| Debug integration | (in subsystem) | 🔮 Deferred | Wire to Combat.Debug.Awareness CVar |

**UCombatAwarenessSubsystem Design** (for later implementation):
```cpp
UCLASS()
class KATANACOMBAT_API UCombatAwarenessSubsystem : public UWorldSubsystem
{
    // Registered characters with cached skeletal data
    TMap<TWeakObjectPtr<ACharacter>, FCachedSkeletalData> RegisteredCharacters;

    // Runtime correction requests
    TArray<FPendingCorrection> PendingCorrections;

    // Event handlers
    void OnPairedAnimationStarted(AActor* Attacker, AActor* Victim, UPairedAnimationData* Data);
    void OnSyncPointApproaching(float TimeToSync);
    void OnFrameTick(float DeltaTime);  // Only ticks during active paired animations

    // Correction API
    FVector CalculateCorrectionForContact(AActor* Character, FName Bone, FVector DesiredWorldPos);
    void RequestIKCorrection(AActor* Character, FName Bone, FVector Target, float BlendTime);
    void RequestPositionNudge(AActor* Character, FVector Offset, float BlendTime);
};
```

**Deferred Until**: Phase 5c-5e utilities prove the patterns and provide the building blocks.

---

## COMPLETE GAP COVERAGE MATRIX

This section ensures every single gap identified by the exploration agents has a documented solution. **No gap is left unaddressed.**

### Legend
- ✅ = Implemented | 🔄 = In Progress | ⏳ = Pending | 🔮 = Phase 6 (Deferred)

### 1. AI/ENEMY COORDINATION GAPS

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 1.1 | No Attack Token System | P1 | Create `UCombatTokenSubsystem` (UGameInstanceSubsystem) - per-target token pools, max 3-4 attackers, restrict to 1 during Finishing state | ⏳ |
| 1.2 | No Interrupt Finisher Mechanic | P0 | Add `bInvulnerableDuringPairedAnim` flag to HitReactionComponent | ⏳ |
| 1.3 | AI Awareness of Paired State | P1 | Use existing `ECombatState::Finishing` - add StateTree condition node `IsTargetInFinisherState` | ⏳ |
| 1.4 | No Execution Prevention Window | P2 | Expose safe interrupt times in PairedAnimationData | ⏳ |
| 1.5 | Stacked Finisher Exploitation | P0 | ✅ Victim mutex via `bIsFinisherTarget` flag in HitReactionComponent (Commit 9e67693) | ✅ |

**Research Finding (Agent ac53a6f)**: Existing infrastructure supports token system:
- `ECombatState::Finishing` already exists and is set during finishers
- StateTree plugin enabled for AI decision-making
- Recommended: `UCombatTokenSubsystem` with `TMap<AActor*, FTokenPool>` per-target pools
- During Finishing: Lock token count to 1 (only the executing attacker)

### 2. INPUT HANDLING GAPS

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 2.1 | Player Input Not Blocked | P0 | ✅ `bBlockCombatInput` in CombatComponent, checked in `CanProcessInput()` (Commit 9e67693) | ✅ |
| 2.2 | Camera Input Handling Undefined | P2 | Add `bLookInputLocked` flag to PlayerCharacter, lock during finisher cinematics | ⏳ |
| 2.3 | No Finisher Button Prompt Timing | P2 | Add `FinisherPromptDuration` config, UI system hook | ⏳ |
| 2.4 | Evade/Block Not Disabled | P2 | ✅ Covered by `bBlockCombatInput` - all combat actions gated (Commit 9e67693) | ✅ |
| 2.5 | Menu Input During Cinematics | P3 | Pause finisher on menu open, resume on close | ⏳ |

**Research Finding (Agent a5c78a6)**: Camera infrastructure needs work:
- PlayerCharacter has `IA_Look` bound but no lock mechanism
- `ImpactCameraShake` property exists in PairedAnimationData but NOT wired
- Need: `bLookInputLocked` flag + focus switching for paired animation midpoint
- Note: Template systems (SideScrollingCamera etc.) disregarded - not representative of project

### 3. ANIMATION/TIMING GAPS

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 3.1 | Montage Length Mismatch | P1 | Add `IsDataValid()` check comparing montage lengths vs sync time | ⏳ |
| 3.2 | Playback Rate Desync | P2 | Tie victim playrate to attacker via `SetPlayRateModifier()` | ⏳ |
| 3.3 | Section Playback Not Enforced | **P1** | Add optional `AttackerMontageSection`, `VictimMontageSection` fields with custom editor validated section selector (same as AttackData) - enables multiple finisher variations in single montage | ⏳ |
| 3.4 | Animation Loop/Repeat Behavior | P2 | Add `bExecutedOnce` flag to prevent double notify execution | ⏳ |
| 3.5 | Interrupt Handling Incomplete | P1 | Bind to `OnMontageBlendingOut` to handle forced stops | ⏳ |

### 4. AUDIO SYNCHRONIZATION GAPS

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 4.1 | Sound Effect Sync Points | P2 | Add `ImpactSound` to AttackData/PairedAnimationData, play via sync delegate | ⏳ |
| 4.2 | Voice Line Timing | P3 | Add `VictimReactionSound` field, play at sync point | ⏳ |
| 4.3 | Music Ducking | P3 | Add `MusicDuckingAmount` to PairedAnimationData, hook to audio subsystem | ⏳ |
| 4.4 | Spatial Audio | P3 | Use `VictimContactBone` location from sync notify for 3D positioning | ⏳ |

**Research Finding (Agent aa1802a)**: NO audio playback infrastructure exists currently:
- `VictimContactBone` in sync notify provides spatial position (ready for hookup)
- Delegates already broadcasting: `OnPairedAnimationSyncPoint` is the trigger point
- Recommended: Add audio asset references to AttackData (keeps audio coupled with attacks)
- Implementation: `UGameplayStatics::PlaySoundAtLocation()` at contact point

### 5. UI/HUD GAPS

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 5.1 | Health Bar Visibility | P3 | Add `bHideHealthBarsDuringFinisher` config option | ⏳ |
| 5.2 | Damage Numbers Timing | P3 | Defer damage number display until finisher ends | ⏳ |
| 5.3 | Finisher Prompt Lifecycle | P2 | Add timeout timer, fade-out animation | ⏳ |
| 5.4 | Screen Effects During Slow-Mo | P3 | Add post-process material ref to PairedAnimationData | ⏳ |
| 5.5 | Multi-Victim UI Conflict | P3 | Use mutex (Gap 1.5) to prevent multi-victim scenario | ⏳ |

### 6. ENVIRONMENTAL INTERACTION GAPS

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 6.1 | Ledge Detection Not Integrated | P2 | Add ledge raycast to `ValidatePairedAnimation()` | ⏳ |
| 6.2 | Destructible Environment | P3 | Add `bDisableDestructiblesDuringPairedAnim` flag | 🔮 |
| 6.3 | Damage Volumes | P3 | Add damage immunity or terrain avoidance in ValidatePairedAnimation | ⏳ |
| 6.4 | Moving Platforms | P3 | Track platform velocity, apply offset adjustment | 🔮 |

### 7. STATE TRANSITION GAPS (CRITICAL)

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 7.1 | Attacker Death Mid-Finisher | P0 | ✅ `OnPairedPartnerDeath()` + `CancelPairedAnimation()` in CombatComponent (Commit 9e67693) | ✅ |
| 7.2 | Victim Becomes Invulnerable | P1 | Pre-sync invulnerability check in sync point handler | ⏳ |
| 7.3 | External Damage During Paired | P2 | Add `bFilterExternalDamage` option to PairedAnimationData | ⏳ |
| 7.4 | Montage Fails to Play | P1 | ✅ `TryExecuteFinisher()` validates montage before playing, returns false on failure | ✅ |
| 7.5 | Component Null Reference | P1 | ✅ Null checks in `TryExecuteFinisher()`, warp setup functions (Commit 9e67693) | ✅ |

### 8. PERFORMANCE GAPS

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 8.1 | Multiple Simultaneous Paired Anims | P3 | Limit via attack token system (Gap 1.1) | ⏳ |
| 8.2 | Ragdoll During Paired Animation | P2 | Queue ragdoll activation until paired anim ends | ⏳ |
| 8.3 | Physics Simulation Overhead | P3 | Batch state restoration, cache physics state | 🔮 |

### 9. RECOVERY & CLEANUP GAPS

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 9.1 | State Machine Recovery | P1 | Reset to `ECombatState::Idle` after paired anim | ⏳ |
| 9.2 | Incomplete Cleanup on Interrupt | P1 | Add cleanup in `OnAnyMontageBlendingOut` | ⏳ |
| 9.3 | Capsule Size Mismatch | P2 | Post-warp overlap check, nudge apart if needed | ⏳ |
| 9.4 | Pose Recovery | P3 | Use existing `SavePoseSnapshot()` before paired anim | ⏳ |

### 10. EXTENSIBILITY GAPS (FUTURE)

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 10.1 | Multi-Victim Finishers | P3 | Design TArray<AActor*> victims in PairedAnimationData | 🔮 |
| 10.2 | Environmental Finishers | P3 | Add `IEnvironmentalFinisher` interface | 🔮 |
| 10.3 | Weapon-Type Finishers | P2 | Add `EWeaponType` filter to PairedAnimationData | ⏳ |
| 10.4 | Context-Sensitive Finishers | P2 | Wire `FFinisherTriggerReason` to animation selection | ⏳ |
| 10.5 | Replay/Animation Blending | P3 | Time-dilation aware montage position tracking | 🔮 |

### 11. DELEGATE WIRING GAPS

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 11.1 | Sync Point Not Wiring Damage | P0 | Add damage application in `OnPairedAnimationSyncPoint` handler | ⏳ |
| 11.2 | Slow-Motion Not Triggered | P1 | ✅ `CinematicEffectsUtilityLibrary::ApplySlowMotion()` + `RestoreTimeDilation()` (Commit 9e67693) | ✅ |
| 11.3 | Camera Shake Not Integrated | P1 | ✅ `CinematicEffectsUtilityLibrary::TriggerCameraShake()` (Commit 9e67693) | ✅ |
| 11.4 | Hit Pause Not Implemented | P1 | ✅ `CinematicEffectsUtilityLibrary::ApplyHitstop()` - Sakurai-style (Commit 9e67693) | ✅ |

### 12. ANIMATION INSTANCE GAPS

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 12.1 | Montage Position Sync | P2 | Add `SyncPartnerMontagePosition()` for drift correction | ⏳ |
| 12.2 | Bone Lock for Contact Points | P3 | Continuous bone tracking via sync notify tick | 🔮 |
| 12.3 | Root Motion Blending | P2 | Attacker takes priority, victim root motion disabled | ✅ |

### 13. BUG/CRASH PREVENTION

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 13.1 | Division by Zero in Sync | P1 | Validate sync time < montage length in `IsDataValid()` | ⏳ |
| 13.2 | Null Reference in Warp | P1 | Null-check TargetActor in `CalculateWarpTarget()` | ⏳ |
| 13.3 | Double Ragdoll Activation | P2 | Atomic `bRagdollActivated` check | ⏳ |
| 13.4 | Infinite Loop in History | P2 | Add max iteration guard to history selection | ⏳ |
| 13.5 | Unhandled Montage Interrupted | P1 | Fallback damage application on interrupt | ⏳ |

### 14. POLISH GAPS

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 14.1 | Ragdoll Settling | P3 | Add ragdoll sleep/freeze logic after finisher death | 🔮 |
| 14.2 | Camera Follow During Finisher | P2 | Focus camera on midpoint between characters | ⏳ |
| 14.3 | Screen Shake Stacking | P2 | Add shake priority/blending system | ⏳ |
| 14.4 | Death Animation Loop Prevention | P2 | Disable loop on death reactions | ⏳ |

### 15. VFX/VISUAL EFFECTS GAPS (NEW - Scaffolding)

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 15.1 | Impact VFX at Sync Point | P3 | Add `ImpactVFX` (UNiagaraSystem*) slot to PairedAnimationData, spawn at contact point | ⏳ |
| 15.2 | Blood/Damage Decals | P3 | Add `DecalMaterial` slot to AttackData, project on victim mesh at impact | 🔮 |
| 15.3 | Slow-Motion Post-Process | P3 | Add `SlowMoPostProcess` (UMaterialInterface*) slot to PairedAnimationData | ⏳ |
| 15.4 | Weapon Trail Enhancement | P3 | Add `FinisherTrailVFX` slot for enhanced trail during cinematics | 🔮 |
| 15.5 | Screen Blood Splatter | P3 | Add `ScreenBloodMaterial` slot, activate at high-damage sync points | 🔮 |
| 15.6 | Environment Destruction | P3 | Add `DestructionRadius` config for triggering nearby destructibles | 🔮 |

**VFX Scaffolding Recommendation**:
Your instinct is correct - VFX is "set dressing" that layers on top of working mechanics. However, **scaffolding property slots now** (without implementing the actual VFX) provides:
1. **Zero overhead** - Empty `UNiagaraSystem*` pointers cost nothing at runtime
2. **Clear integration points** - When you add VFX later, you know exactly where they plug in
3. **Designer workflow** - Artists can populate slots as VFX are created
4. **No refactoring** - Avoids revisiting sync point handlers later

**Recommended Approach**: Add property slots in Phase 5b-4 (5 minutes of work), implement actual VFX in Phase 7 after core systems solidified. This matches your "set dressing after core systems" plan while ensuring the scaffolding is in place.

### 16. IMPLEMENTATION GAPS (Earlier Identified)

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 16.1 | Warp Config Struct Inconsistency | P2 | Attacker paired warp uses `FPairedWarpConfig`, regular attack warp uses `FAttackWarpConfig`. Consider unification or clear documentation of when to use each. | ⏳ |
| 16.2 | No Finisher Distance Validation | P2 | ✅ `TryExecuteFinisher()` now validates distance using TargetingSettings (Commit 37faa08) | ✅ |
| 16.3 | Partner Array Not Persisted | P3 | `PairedAnimationPartners` is runtime-only. If serialization needed for network replication, requires UPROPERTY setup. | 🔮 |
| 16.4 | Cinematic Effects Not Auto-Wired | P2 | ✅ Delegates bound in CombatComponent for slow-mo, camera shake, hitstop (Commit 37faa08) | ✅ |
| 16.5 | No Finisher Cancel Animation | P2 | `CancelPairedAnimation()` stops montages but doesn't play recovery animation. Should blend to idle or recovery pose. | ⏳ |

### 17. EDGE CASE GAPS (Earlier Identified)

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 17.1 | Finisher During Hitstop | P2 | If attacker triggers finisher while victim in hitstop, timing may be off. Check and clear hitstop before finisher. | ⏳ |
| 17.2 | Double Finisher Input | P2 | Rapid button presses could queue multiple finisher attempts. Add cooldown or input debounce in `TryExecuteFinisher()`. | ⏳ |
| 17.3 | Victim Movement After Finisher | P2 | Dead victims may slide on slopes after finisher ends. Physics state should be frozen or ragdoll activated. | ⏳ |
| 17.4 | Attacker Blocked During Warp | P2 | If attacker hits obstacle while warping toward victim, warp aborts but finisher continues. Validate clear path. | ⏳ |
| 17.5 | Time Dilation Stacking | P1 | ✅ `ApplySlowMotion()` now prevents stacking - slower dilation takes priority (Commit 37faa08) | ✅ |

### 18. PHASE 5b-4 ANALYSIS GAPS (Agent a45a8ad - 2026-01-30)

**Discovery Method**: Comprehensive code analysis of implementation files post-Phase 5b-4

#### Critical (Blocking) - 5 Issues
| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 18.1 | Finisher Target Cleanup on Failed Execution | P0 | ✅ Added rollback in `TryExecuteFinisher()` - clears flag, partners, effects if montage fails | ✅ |
| 18.2 | Victim Warp Config Not Using Offset | P0 | ✅ Added `RelativeOffset` to `FPairedWarpConfig`, used in `SetupVictimWarp()` | ✅ |
| 18.3 | Attacker Warp Config Not Using Offset | P0 | ✅ `SetupAttackerPairedWarp()` now uses `Config.RelativeOffset` from victim's perspective | ✅ |
| 18.4 | Null MotionWarpingComponent Silent Failure | P1 | Returns false with no warning for characters without MotionWarpingComponent | ⏳ |
| 18.5 | Partner Validity Not Checked During Animation | P1 | No handling for partner destroyed/teleported mid-animation (only death event handled) | ⏳ |

#### Major (Gameplay Breaking) - 5 Issues
| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 18.6 | Finisher Distance Uses Wrong Setting | P1 | Uses `SoftAimRange` instead of `FPairedWarpConfig::MaxWarpDistance` - can start from too far | ⏳ |
| 18.7 | Finisher Target Flag Not Cleared on Interrupt | P0 | ✅ `CancelPairedAnimation()` now clears `bIsFinisherTarget` on all partners | ✅ |
| 18.8 | Partner Validity Check Missing at Sync Point | P1 | Alignment validation accesses partner without null check post-destruction | ⏳ |
| 18.9 | Victim Warp Tracking Continues After Victim Dies | P1 | `OnVictimMotionWarpingPreUpdate` missing bidirectional validity check | ⏳ |
| 18.10 | CancelPairedAnimation Doesn't Clear Victim Warp | P0 | ✅ `CancelPairedAnimation()` now calls `ClearVictimWarp()`/`ClearAttackerPairedWarp()` on partners | ✅ |

#### Edge Cases - 6 Issues
| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 18.11 | Stale WeakObjectPtr in DynamicallyIgnoredActors | P3 | Array accumulates dead references, minor perf impact | ⏳ |
| 18.12 | Capsule Physics Without Movement Restriction | P2 | `bDisableCapsulePhysics=true` + `bDisableMovement=false` allows physics-free movement | ⏳ |
| 18.13 | Hitstop Uses Per-Actor, Slow-Mo Uses World Dilation | P2 | No coordination between `CustomTimeDilation` and `WorldSettings->TimeDilation` | ⏳ |
| 18.14 | Finisher Health Threshold Hardcoded | P2 | `HealthThreshold = 0.25f` hardcoded in HitReactionComponent, not configurable | ⏳ |
| 18.15 | Multiple TODO Comments in CombatComponent | P3 | 5 TODOs indicate incomplete work (checkpoints, framerate, thresholds) | 🔮 |

#### Integration Inconsistencies - 4 Issues
| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 18.16 | Victim Montage Start Offset Calculation | P2 | `VictimStartOffset` negated and clamped - negative values mean wrong timing | ⏳ |
| 18.17 | No Warning When Victim Lacks TargetingComponent | P2 | Warp setup silently fails if victim has no TargetingComponent | ⏳ |
| 18.18 | Asymmetric Collision Settings Between Partners | P2 | Attacker/victim notifies run independently with potentially conflicting settings | ⏳ |
| 18.19 | Duplicate Partner Registration Not Prevented | P2 | If victim lacks CombatComponent, arrays become asymmetric | ⏳ |

---

### 19. GAP AUDIT GAPS (Agent ac9230c + a8642c8 - 2026-01-30)

**Discovery Method**: Comprehensive code audit + verification of completed gaps

#### Critical/Major (P1) - 6 Issues
| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 19.1 | Null MotionWarpingComponent Silent Failure | P1 | Add warning log when MotionWarpingComponent is null in warp setup | ✅ |
| 19.2 | Partner Validity Check Missing at Sync Point | P1 | Add null check before accessing partner location in AnimNotifyState_PairedAnimationSync | ✅ |
| 19.3 | Victim Warp Tracking Continues After Death | P1 | Add bidirectional validity check in OnVictimMotionWarpingPreUpdate | ✅ |
| 19.4 | Finisher Distance Uses SoftAimRange | P3 | **INTENTIONAL**: Finisher-specific detection wasn't working. SoftAimRange is the working solution. Revisit only if dedicated FinisherRange detection fixed. | ✅ Working As Intended |
| 19.6 | Attacker Warp Blocked by Obstacle - No Fallback | P2 | Add path validation before warp execution in TryExecuteFinisher | ⏳ |
| 19.14 | Incomplete Cleanup on Montage Interrupt | P1 | Add safety cleanup in EndPlay() to cancel any active paired animation | ✅ |

#### Moderate (P2) - 7 Issues
| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 19.5 | Finisher Health Threshold Hardcoded (25%) | P2 | Add FinisherHealthThreshold to HitReactionSettings | ⏳ |
| 19.7 | Capsule Physics Without Movement Restriction | P2 | Enforce bDisableMovement when bDisableCapsulePhysics is true | ⏳ |
| 19.8 | Hitstop vs Slow-Mo Dilation Coordination | P2 | Create unified time dilation coordinator tracking both sources | ⏳ |
| 19.10 | Missing Bidirectional Partner Registration | P2 | Add warning when victim lacks CombatComponent for registration | ⏳ |
| 19.11 | Victim Montage Offset Negation Unclear | P2 | Add documentation + validation for VictimStartOffset semantics | ⏳ |
| 19.12 | No Warning When Victim Lacks TargetingComponent | P2 | Add warning log when victim warp setup skipped | ⏳ |
| 19.13 | Asymmetric Collision Settings Between Partners | P2 | Add validation/warning for mismatched collision settings | ⏳ |

#### Polish (P3) - 1 Issue
| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 19.9 | Stale WeakObjectPtr in DynamicallyIgnoredActors | P3 | Add cleanup in RestoreState() to remove invalid weak references | ⏳ |

---

### 20. FINISHER TESTING SESSION GAPS (Log Analysis - 2026-01-30)

**Discovery Method**: Manual testing session with `Combat.Debug.All 1` + comprehensive log analysis

**Context**: Finisher triggered correctly below 20% health, but:
- Enemy didn't stay dead (returned to idle at 24% health)
- Player input blocked permanently after finisher
- Debug showed player still connected to finished enemy

#### Critical (P0) - 3 Issues
| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 20.1 | OnMontageEnded Not Detecting Finisher Completion | P0 | Added detection logic - if montage matches AttackerMontage, call `CompletePairedAnimation()` or `CancelPairedAnimation()` | ✅ |
| 20.2 | No Damage Application at Finisher Completion | P0 | Added `CompletePairedAnimation()` function - applies `BaseDamage * DamageMultiplier`, uses `bIsLethal` for guaranteed kill | ✅ |
| 20.3 | Input Blocking Not Cleared at Finisher End | P0 | `CompletePairedAnimation()` calls `EndPairedAnimation()` which sets `bBlockCombatInput = false` | ✅ |

#### Major (P1) - 3 Issues
| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 20.4 | Guard Flag Prevents Double CompletePairedAnimation | P1 | Added `bCompletingPairedAnimation` flag to prevent re-entry if OnMontageEnded fires multiple times - checked at start, cleared at end and in CancelPairedAnimation/EndPlay | ✅ |
| 20.5 | Sync Point Distance Drift (159.9u > 150u max) | P1 | Characters drifting ~90 units during finisher - increase MaxContactDistance or improve warp tracking frequency | ⏳ |
| 20.6 | Finisher Victim Tracking via CurrentFinisherVictim | P1 | Added `TWeakObjectPtr<AActor> CurrentFinisherVictim` to store victim reference for damage application at completion | ✅ |

#### Moderate (P2) - 3 Issues
| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 20.7 | Paired Animation Cleanup Not Atomic | P2 | Cleanup spread across `CompletePairedAnimation()`, `CancelPairedAnimation()`, `EndPairedAnimation()` - consider consolidation | ⏳ |
| 20.8 | No Recovery Phase After Finisher | P2 | Attacker goes straight to Idle - add optional recovery montage section or blend | ⏳ |
| 20.9 | Weapon State Not Managed During Finisher | P2 | Hit detection state may be inconsistent during paired animation - add explicit weapon state management | ⏳ |

**Implementation Summary (Commit pending)**:
```cpp
// CombatComponent.h - New tracking
TWeakObjectPtr<AActor> CurrentFinisherVictim;
void CompletePairedAnimation();

// CombatComponent.cpp - OnMontageEnded detection
if (IsPairedAnimationActive() && ActivePairedAnimData && Montage == ActivePairedAnimData->AttackerMontage)
{
    if (!bInterrupted) CompletePairedAnimation();  // Success - apply damage
    else CancelPairedAnimation(0.0f);              // Interrupted - no damage
    return;
}

// CombatComponent.cpp - CompletePairedAnimation() (~100 lines)
// 1. Get victim from CurrentFinisherVictim
// 2. Calculate damage: BaseDamage * DamageMultiplier
// 3. If bIsLethal: damage = Max(damage, currentHealth + 1)
// 4. Apply via IDamageableInterface::Execute_ApplyDamage()
// 5. Clear all state: partners, warp tracking, flags, input blocking
```

---

### 21. DEATH ANIMATION FIX GAPS (Session 2026-01-30 Continued)

**Discovery Method**: Manual testing revealed double death animation issue

**Context**: After finisher victim montage completed, enemy went back to idle, THEN played a separate death animation (AM_Deaths), THEN ragdolled. The finisher victim montage IS the death animation - should hold pose or ragdoll directly.

#### Critical (P0) - 1 Issue
| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 21.1 | Double Death Animation After Finisher | P0 | Added `bDeathHandledByPairedAnimation` flag to HitReactionComponent. `SetDeathHandledByPairedAnimation()` called before damage. `PlayDeathReaction()` checks flag → skips AM_Deaths, applies outcome (Ragdoll/FreezeAtCurrentPose) directly. | ✅ |

**Implementation Summary (Commit pending)**:
```cpp
// HitReactionComponent.h - New API
bool bDeathHandledByPairedAnimation = false;
EReactionOutcome PairedAnimationDeathOutcome = EReactionOutcome::Ragdoll;
float PairedAnimationRagdollBlendTime = 0.2f;

void SetDeathHandledByPairedAnimation(EReactionOutcome Outcome, float RagdollBlendTime = 0.2f);
void ClearPairedAnimationDeathHandling();

// HitReactionComponent.cpp - PlayDeathReaction() early exit
if (bDeathHandledByPairedAnimation)
{
    bDeathHandledByPairedAnimation = false;
    switch (PairedAnimationDeathOutcome)
    {
        case EReactionOutcome::Death: FreezeAtCurrentPose(); break;
        case EReactionOutcome::Ragdoll: ActivateRagdoll(PairedAnimationRagdollBlendTime); break;
    }
    return true;  // Don't play AM_Deaths
}

// CombatComponent.cpp - CompletePairedAnimation() sets flag before damage
VictimHitReaction->SetDeathHandledByPairedAnimation(
    ActivePairedAnimData->VictimDeathOutcome,
    ActivePairedAnimData->RagdollBlendTime);
```

---

### GAP COVERAGE SUMMARY (Updated 2026-01-30 - Post Testing Session)

| Category | Total | ✅ Done | 🔄 Progress | ⏳ Pending | 🔮 Deferred |
|----------|-------|---------|-------------|------------|-------------|
| AI Coordination | 5 | 1 | 0 | 4 | 0 |
| Input Handling | 5 | 2 | 0 | 3 | 0 |
| Animation/Timing | 5 | 0 | 0 | 5 | 0 |
| Audio Sync | 4 | 0 | 0 | 4 | 0 |
| UI/HUD | 5 | 0 | 0 | 5 | 0 |
| Environment | 4 | 0 | 0 | 2 | 2 |
| State Transitions | 5 | 3 | 0 | 2 | 0 |
| Performance | 3 | 0 | 0 | 2 | 1 |
| Recovery/Cleanup | 4 | 0 | 0 | 4 | 0 |
| Extensibility | 5 | 0 | 0 | 2 | 3 |
| Delegate Wiring | 4 | 4 | 0 | 0 | 0 |
| Animation Instance | 3 | 1 | 0 | 1 | 1 |
| Bug Prevention | 5 | 0 | 0 | 5 | 0 |
| Polish | 4 | 0 | 0 | 3 | 1 |
| VFX Scaffolding | 6 | 0 | 0 | 2 | 4 |
| **Implementation (16.x)** | **5** | **2** | **0** | **2** | **1** |
| **Edge Cases (17.x)** | **5** | **1** | **0** | **4** | **0** |
| **Phase 5b-4 Analysis (18.x)** | **20** | **5** | **0** | **14** | **1** |
| **Gap Audit (19.x)** | **14** | **5** | **0** | **9** | **0** |
| **Testing Session (20.x)** | **9** | **5** | **0** | **4** | **0** |
| **Death Animation (21.x)** | **1** | **1** | **0** | **0** | **0** |
| **TOTAL** | **121** | **31** | **0** | **76** | **14** |

**Note**: 14 gaps deferred to Phase 6+ (IK, multi-victim, environmental finishers, VFX implementation, network replication, TODOs)
**VFX Strategy**: Property slots scaffolded in Phase 5b-4, actual VFX implementation deferred to Phase 7
**Gap 19.4 Note**: Using SoftAimRange is **INTENTIONAL** - finisher-specific detection had issues. Marked as Working As Intended.
**Death Animation Note**: Gap 21.1 fixed double death animation issue - flag system prevents PlayDeathReaction from playing AM_Deaths after finisher

**⭐ CRITICAL FIXES (Commit pending)**:
- ✅ 21.1: Death animation handled by paired animation flag system (no double death)
- ✅ 20.1: `OnMontageEnded` now detects finisher attacker montage completion
- ✅ 20.2: `CompletePairedAnimation()` applies damage with `bIsLethal` support
- ✅ 20.3: Input blocking cleared via `EndPairedAnimation()` call at completion
- ✅ 20.4: Guard flag prevents double `CompletePairedAnimation()` execution
- ✅ 20.6: `CurrentFinisherVictim` tracks victim for damage application

**P1 Gap Fixes Summary** (Commit pending):
- ✅ 19.1: Warning logs for null MotionWarpingComponent in SetupVictimWarp/SetupAttackerPairedWarp
- ✅ 19.2: Warning log when PairedPartner is null at sync point validation
- ✅ 19.3: World validity check in OnVictimMotionWarpingPreUpdate/OnAttackerPairedWarpPreUpdate
- ✅ 19.14: EndPlay override in CombatComponent for paired animation cleanup

**Phase 5b-4 Completion Summary** (Commit 37faa08):
- ✅ 16.2: Finisher distance validation (`TryExecuteFinisher()`)
- ✅ 16.4: Cinematic effects auto-wired to delegates
- ✅ 17.5: Time dilation stacking prevention
- ✅ Comprehensive test suite (34 tests)
- ✅ Sync point alignment validation with auto-nudge

**Phase 5b-1/2/3 Completion Summary** (Commit 9e67693):
- ✅ 1.5: Victim mutex (`bIsFinisherTarget`)
- ✅ 2.1, 2.4: Input blocking (`bBlockCombatInput`)
- ✅ 7.1, 7.4, 7.5: State transition safety (death handler, null checks)
- ✅ 11.2, 11.3, 11.4: CinematicEffectsUtilityLibrary (slow-mo, camera shake, hitstop)
- ✅ 12.3: Root motion blending (attacker priority)

**REMAINING CRITICAL GAPS** (P1 - Next Priority):
1. **20.5**: Sync point distance drift (characters drifting 90+ units during finisher)
2. **19.6**: No fallback when attacker warp blocked by obstacle (P2)
3. **3.3**: Montage section support for paired finishers (P1)

**RESOLVED THIS SESSION**:
- ~~**19.4**: Finisher uses SoftAimRange~~ → **Working As Intended** (finisher-specific detection had issues)

---

## User Design Requirements

### Core Principles
- **Modular with accessible interfaces** - Easy swap-in testing of animation pairs
- **Environmental awareness** - Terrain, obstacles, walls, ledges
- **Spatial awareness** - Character positioning and gap closing
- **Kinematic awareness** - Weapon/mesh overlap detection
- **Anatomical awareness** - Contact points, character self-overlap
- **Data-driven + Procedural** - Configuration combined with math-based detection
- **Self-correcting/Self-healing** - Runtime procedural adjustments for sync drift, contact alignment
- **Editor-time pre-calculation** - Analyze montage pairs at editor time, populate viable fields mathematically

### Development Philosophy (User Preference)
- **THOROUGH SOLUTIONS OVER QUICK FIXES** - Always prefer the more complete implementation
  - Example: Tracked partner array (supports multi-partner kills) over global pawn collision disable
  - Rationale: Quick fixes create hard-to-debug issues later when forgotten
  - Note: This preference has been added to CLAUDE.md
- **FOUNDATION FIRST WITH AWARENESS OF GREATER PLAN** - Build incrementally but with architecture in mind
  - Example: Create utility libraries first, graduate to subsystem when patterns established
  - This is an expansion/specification of the original plan, not a pivot
- **C++ BULK WORK, MINIMAL BLUEPRINT** - Do heavy lifting in C++, minimal nodes in editor blueprints
  - AnimInstance: Properties and functions in C++, Blueprint just wires to Control Rig
  - Editor tools: Slate UI with C++ backing, minimal Blueprint-only widgets

### Expanded Scope Architecture Decisions (2026-01-30 Consultation)
| Question | Decision | Rationale |
|----------|----------|-----------|
| Tool Structure | UMontageAnalyzerTools (generic base, subclassable) | Reusable beyond paired animations |
| Priority | Contact points & timing first, then full analysis | Foundation for all other features |
| Component Naming | Expand utilities first, PhysicsAwarenessComponent later | Don't create component until patterns proven |
| Editor UI | Full analysis dashboard (Slate) | Comprehensive visualization for artists |
| AnimInstance | Extend SamuraiAnimInstance in C++ | Matches existing project pattern |
| Phasing | Foundation first with greater plan awareness | Expansion, not pivot |

### Target Flow (AC3-style)
1. Enemy attacks player → UI alert appears
2. Player presses block in time → **Parry sequence** (synced, slowdown)
3. Enemy's unsynced attack blends/warps into closest paired parry animation
4. Slo-mo window for player input
5. Player attacks during window → **Counter sequence** (synced, flows from parry)
6. Follow-up attack → **Paired finisher sequence**
7. Low health enemies → Vulnerable state → **Cinematic execution**

### User Design Decisions (Finalized)
- **Attack Blend**: Hybrid slow-mo blend - time slows during parry, enemy blends to victim pose, paired animation plays
- **Contact Detection**: Socket-based + procedural traces - data-driven sockets with runtime verification
- **Environmental Finishers**: Architecture only - build interfaces now, implement standard finishers first
- **Slow Motion**: Separate system with hooks - `OnPairedAnimationStarted` delegate, modular TimeManager
- **Finisher Triggers** (multiple):
  - Low health threshold (enemy below X% health)
  - Guard break state (posture depleted)
  - Stun state (from heavy attack hitstun)
  - Design goal: Easy to use and flashy (AC3 feel)

---

## Lessons Learned (Phase 5b-1/2/3 Implementation)

This section documents critical insights gained during implementation that should inform future development.

### 1. THOROUGH SOLUTIONS OVER QUICK FIXES (Core Principle)

**Discovery**: When implementing `TryExecuteFinisher()`, the quick approach was to directly access `MotionWarpingComponent` and set warp targets. The thorough approach was to create `SetupAttackerPairedWarp()` that mirrors the existing `SetupVictimWarp()` API.

**Why Thorough Wins**:
| Aspect | Quick Fix | Thorough Solution |
|--------|-----------|-------------------|
| **Collision Handling** | Manual registration | Auto-registers partner in `PairedAnimationPartners` |
| **Cleanup** | Easy to forget | `EndPlay()` auto-cleanup, `ClearMotionWarp()` integration |
| **Continuous Tracking** | Snapshot-only | Frame-by-frame via `OnAttackerPairedWarpPreUpdate` |
| **Debugging** | Scattered logic | Single source of truth in TargetingComponent |
| **Future Features** | Requires refactoring | Supports multi-partner kills out of the box |

**Lesson**: Quick fixes accumulate technical debt that becomes hard to debug. The "extra hour" for the thorough solution saves days of debugging later.

### 2. SYMMETRIC SYSTEMS ARE MORE ROBUST

**Discovery**: The victim warp system (`SetupVictimWarp`) already existed, but attacker-side paired warp was missing. Once we implemented `SetupAttackerPairedWarp()` as a mirror, the entire system became more intuitive.

**Pattern Identified**:
```
Attacker warps TOWARD victim (closing gap)
Victim warps FROM attacker (maintaining offset)
Both track each other via OnPreUpdate delegate
Both auto-register as PairedAnimationPartners
```

**Why Symmetry Matters**:
- Same mental model for both roles
- Same cleanup patterns
- Same debugging approach
- Reduces special-case code

**Lesson**: When implementing paired interactions, design both sides symmetrically from the start, even if only one side is immediately needed.

### 3. CONTINUOUS TRACKING VS SNAPSHOT POSITIONING

**Discovery**: Initial paired animation designs calculated victim position ONCE at animation start. This caused drift when attacker moved (via root motion or warp).

**Problem Scenario**:
1. Attacker starts at position A, victim calculated for position B
2. Attacker warps 50 units toward victim during animation
3. At sync point, attacker is at A+50, victim still at B
4. Impact misses, animation looks wrong

**Solution**: Both characters continuously track each other via `OnMotionWarpingPreUpdate`:
```cpp
void OnAttackerPairedWarpPreUpdate(UMotionWarpingComponent* MWC)
{
    // Recalculate warp target based on victim's CURRENT position
    FVector CurrentVictimLoc = TrackedVictim->GetActorLocation();
    // ... apply terrain adjustment, update warp target
}
```

**Lesson**: Any system where two actors need to stay synchronized requires continuous position updates, not initial-position snapshots.

### 4. PARTNER ARRAY > GLOBAL COLLISION DISABLE

**Discovery**: The naive approach to collision during paired animations is `SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore)`. This affects ALL pawns, causing unrelated characters to clip through.

**Better Approach**: `PairedAnimationPartners` array + `IgnoreActorWhenMoving()`:
- Only ignores specific tracked partners
- Supports multi-partner scenarios (double takedowns)
- Clear ownership and cleanup
- Doesn't affect unrelated actors

**Lesson**: Targeted solutions that maintain explicit state are more debuggable and extensible than global toggles.

### 5. DELEGATE CLEANUP PATTERNS

**Discovery**: When binding to `OnPreUpdate` delegates for continuous tracking, forgetting to unbind causes:
- Crashes after actor destruction
- Performance overhead from stale callbacks
- Hard-to-trace bugs

**Pattern Established**:
```cpp
// Setup
void SetupAttackerPairedWarp(AActor* Victim, const FPairedWarpConfig& Config)
{
    // Store weak reference (handles destruction)
    TrackedVictim = Victim;
    // Bind delegate
    MotionWarpingComponent->OnPreUpdate.AddDynamic(this, &OnAttackerPairedWarpPreUpdate);
    bIsTrackingAsAttacker = true;
}

// Cleanup - called from multiple paths
void StopAttackerPairedWarpTracking()
{
    if (bIsTrackingAsAttacker && MotionWarpingComponent)
    {
        MotionWarpingComponent->OnPreUpdate.RemoveDynamic(...);
    }
    TrackedVictim.Reset();
    bIsTrackingAsAttacker = false;
}

// EndPlay - guaranteed cleanup
void EndPlay(...)
{
    StopAttackerPairedWarpTracking();  // Also stop victim tracking
    Super::EndPlay(...);
}
```

**Lesson**: Every delegate binding needs a corresponding unbind, called from:
1. Explicit clear function
2. `EndPlay()`
3. Any interrupt/cancel handler

### 6. FINISHER VULNERABILITY PRIORITY ORDER

**Discovery**: Multiple conditions can make an enemy vulnerable to finishers (guard break, stun, low health). The order of checking matters for game feel.

**Priority Order (Commit 9e67693)**:
1. **Guard Break** (highest) - Player earned this through sustained offense
2. **Stunned** - Temporary window from heavy attack
3. **Low Health** - Fallback for accessibility/pacing

**Rationale**: Guard break represents player skill (depleting posture), stun is situational, low health is "mercy kill" accessibility. Communicating the REASON via `GetFinisherTriggerReason()` enables context-aware UI and animation selection.

**Lesson**: When multiple triggers enable the same action, establish clear priority and expose the reason for downstream systems.

---

## Newly Identified Gaps (Phase 5b Implementation)

These gaps were discovered during implementation and should be added to tracking:

### 16. IMPLEMENTATION GAPS (NEW CATEGORY)

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 16.1 | Warp Config Struct Inconsistency | P2 | Attacker paired warp uses `FPairedWarpConfig`, regular attack warp uses `FAttackWarpConfig`. Consider unification or clear documentation of when to use each. | ⏳ |
| 16.2 | No Finisher Distance Validation | P2 | `TryExecuteFinisher()` doesn't verify target is in range before executing. Add range check using TargetingSettings.MaxTargetingRange. | ⏳ |
| 16.3 | Partner Array Not Persisted | P3 | `PairedAnimationPartners` is runtime-only. If serialization needed for network replication, requires UPROPERTY setup. | 🔮 |
| 16.4 | Cinematic Effects Not Auto-Wired | P2 | `CinematicEffectsUtilityLibrary` provides functions but nothing auto-triggers them. Need delegate binding in CombatComponent. | ⏳ |
| 16.5 | No Finisher Cancel Animation | P2 | `CancelPairedAnimation()` stops montages but doesn't play recovery animation. Should blend to idle or recovery pose. | ⏳ |

### 17. EDGE CASE GAPS (NEW CATEGORY)

| Gap ID | Description | Priority | Solution | Status |
|--------|-------------|----------|----------|--------|
| 17.1 | Finisher During Hitstop | P2 | If attacker triggers finisher while victim in hitstop, timing may be off. Check and clear hitstop before finisher. | ⏳ |
| 17.2 | Double Finisher Input | P2 | Rapid button presses could queue multiple finisher attempts. Add cooldown or input debounce in `TryExecuteFinisher()`. | ⏳ |
| 17.3 | Victim Movement After Finisher | P2 | Dead victims may slide on slopes after finisher ends. Physics state should be frozen or ragdoll activated. | ⏳ |
| 17.4 | Attacker Blocked During Warp | P2 | If attacker hits obstacle while warping toward victim, warp aborts but finisher continues. Validate clear path. | ⏳ |
| 17.5 | Time Dilation Stacking | P1 | Multiple slow-mo sources (finisher + parry) could stack. `CinematicEffectsUtilityLibrary` should track active dilation source. | ⏳ |

---

## Updated Risk Mitigation (Post-Implementation)

| Risk | Mitigation | Status | Validation |
|------|-----------|--------|------------|
| Animation clipping | Use obstacle validation before triggering | ✅ Complete | `ValidatePairedAnimation()` |
| Terrain issues | Leverage existing ground sampling | ✅ Complete | `AdjustLocationToGround()` |
| Sync drift | Continuous warp tracking for BOTH attacker AND victim | ✅ Complete | `SetupAttackerPairedWarp()` + `SetupVictimWarp()` |
| Root motion conflicts | Movement disabled via `AnimNotifyState_PairedAnimationCollision` | ✅ Complete | `bDisableMovement` flag |
| Capsule collision | Targeted ignore via `PairedAnimationPartners` | ✅ Complete | `IgnoreActorWhenMoving()` |
| Mesh penetration | Distance-based prevention + collision disable | ✅ Complete | Validation + collision disable |
| Permanent slow-mo | Timer-based restoration with safeguard | ✅ Complete | `RestoreTimeDilation()` + timer |
| Input during cinematic | `bBlockCombatInput` checked in `CanProcessInput()` | ✅ Complete | Commit 9e67693 |
| Attacker death | `OnPairedPartnerDeath()` cancels victim animation | ✅ Complete | Delegate binding |
| Stacked finishers | `bIsFinisherTarget` mutex prevents double-execution | ✅ Complete | HitReactionComponent flag |
| **Time dilation stacking** | Track active dilation source in utility library | ⏳ Pending | Gap 17.5 |
| **Distance validation** | Add range check in `TryExecuteFinisher()` | ⏳ Pending | Gap 16.2 |
| Network replication | Design for future but defer implementation | 🔮 Deferred | Phase 7+ |
| Performance | Event-driven (no tick), timer-based updates | ✅ Architecture | OnPreUpdate callback |

---

## Research Synthesis

### Key Patterns from Reference Games

| Game | Key Technique | Applicable Pattern |
|------|--------------|-------------------|
| **AC3** | Combat Manager AI | Enemies coordinate attacks, don't overlap |
| **AC3** | Kill Streaks | Directional targeting during death animation |
| **AC3** | 3200 fight animations | Massive paired animation library |
| **Batman** | Combat Token System | Max 3 simultaneous attackers (DOOM-style) |
| **Batman** | IK Contact Points | Limbs blend toward contact targets |
| **Batman** | Freeflow Arena | UE5 recreation with 60+ animations |
| **GoT** | Lethality Contract | Weapons feel lethal, instant kills |
| **GoT** | Stance-based selection | Animation varies by equipped stance |
| **GoT** | Perfect Parry Window | Tight timing, slow-mo reward |
| **Gears 4** | Warp Points | Generalized motion warping system |
| **Gears 4** | Inertialization | 60% cheaper animation transitions |
| **For Honor** | Motion Matching | Best pose from thousands of animations |
| **For Honor** | Spine Pitching | Handles height differences on slopes |

### Technical Implementation Patterns

**1. Paired Animation Sync (AC3/Batman)**
- Both characters play animations simultaneously from sync point
- Position interpolation (sliding/warping) handles alignment
- Environment must be clear of obstructions
- Flat ground preferred (or use procedural foot IK)

**2. Motion Warping (Gears 4 → UE5)**
- Named warp targets set before montage play
- AnimNotifyState_MotionWarping reads targets during playback
- Dynamic updates via `OnMotionWarpingPreUpdate` callback
- Terrain adjustment via ground sampling

**3. Token-Based AI Coordination (DOOM/Batman)**
- AI requests attack token before initiating
- Limited simultaneous attackers (default 3)
- Queue system for waiting attackers
- Cooldowns prevent rapid sequential attacks

**4. Contact Point Alignment (Batman IK)**
- IK blends limbs toward expected contact locations
- Simple blocking animation as base
- Eliminates need for huge shields or complex AI
- Works with minimal animation variants

---

## Critical Gaps Identified (Agent Exploration Results)

### Gap 1: Collision During Paired Sequences
**Source**: Agent a6547bd exploration

**Current State**:
- ✅ Ragdoll collision management exists (disables capsule, enables mesh physics)
- ✅ Hit actor ignore list in WeaponComponent (weapon traces)
- ✅ I-frames are software-based damage blocking
- ❌ **NO collision-disabling mechanism for paired animations**

**Problem**: Characters will push each other apart during close paired animations.

**Solution**: Create `AnimNotifyState_PairedAnimationCollision`
```cpp
UCLASS()
class UAnimNotifyState_PairedAnimationCollision : public UAnimNotifyState
{
    // Disables character-to-character collision on NotifyBegin
    // Restores collision on NotifyEnd
    // Saves/restores collision profile to handle interruption

    UPROPERTY(EditAnywhere)
    bool bDisableCharacterCollision = true;

    UPROPERTY(EditAnywhere)
    bool bDisablePawnOverlap = true;
};
```

**Implementation Location**: Reference `HitReactionComponent.cpp:683-729` ragdoll collision pattern.

### Gap 2: Camera & Slow-Motion Not Triggered
**Source**: Agent ab1a168 exploration

**Existing Infrastructure**:
- ✅ `OnPairedAnimationStarted` delegate - already broadcasting
- ✅ `OnPairedAnimationSyncPoint` delegate - already broadcasting
- ✅ `OnPairedAnimationEnded` delegate - already broadcasting
- ✅ `ImpactCameraShake` property in PairedAnimationData - **NEVER TRIGGERED**
- ✅ `bApplySlowMotion`, `SlowMotionScale`, `SlowMotionDuration` in PairedAnimationData - **NEVER TRIGGERED**
- ❌ No `GetPlayerCameraManager()->PlayCameraShake()` calls
- ❌ No `SetWorldTimeDilation()` calls

**Missing Components**:
| Component | Purpose |
|-----------|---------|
| Custom PlayerCameraManager | Paired animation framing, focus switching |
| Time dilation system | Slow-mo with auto-restore timer |
| Camera shake trigger | Play shake at sync point |
| Letterboxing (optional) | Cinematic framing |

**Solution**: Wire existing delegates to camera/time systems
```cpp
// In CombatComponent or new CinematicEffectsComponent
void OnPairedAnimationTriggered(EPairedReactionType Type, bool bIsCriticalMoment)
{
    if (bIsCriticalMoment && PairedAnimData->bApplySlowMotion)
    {
        GetWorld()->GetWorldSettings()->SetTimeDilation(PairedAnimData->SlowMotionScale);
        GetWorldTimerManager().SetTimer(SlowMoHandle, this,
            &RestoreTimeDilation, PairedAnimData->SlowMotionDuration, false);
    }
}

void OnPairedAnimationSyncPointReached(EPairedReactionType Type, FName SyncName)
{
    if (PairedAnimData->ImpactCameraShake)
    {
        GetPlayerController()->PlayerCameraManager->StartCameraShake(
            PairedAnimData->ImpactCameraShake);
    }
}
```

### Gap 3: IK/Procedural Animation (In Scope, Deferred)
**Source**: Agent a7f2b67 exploration

**Current State (70% Ready)**:
- ✅ Control Rigs exist in content (CR_Mannequin_BasicFootIK, IK_Mannequin)
- ✅ Contact point calculation fully implemented (GetBoneWorldLocation, CalculateContactPoint)
- ✅ Motion Warping enabled and integrated
- ✅ Sync point events broadcasting with AttackHand/VictimContactBone parameters
- ❌ No runtime IK chain adjustment
- ❌ No procedural pose blending

**Feasibility Assessment**:
| Feature | Effort | Impact |
|---------|--------|--------|
| Contact point IK (Batman-style) | 2-3 weeks | HIGH - production quality |
| Spine pitch (For Honor-style) | 1 week | MEDIUM - slope handling |
| Full Control Rig integration | 2-3 weeks | HIGH - maximum flexibility |

**Recommendation**: Defer to Phase 6 after core paired animation flow works.

### Gap 4: Root Motion Conflicts (CRITICAL)
**Source**: Agent a7c852f exploration

**Problem**: When two characters have active root motion simultaneously (attacker montage + victim reaction), four motion sources compete with NO priority system:
1. Attacker's montage root motion (forward)
2. Attacker's motion warp (toward victim)
3. Victim's montage root motion (often backward)
4. Both CharacterMovementComponents (still processing input)

**Current State**:
- ✅ Attacker-side warp tracking is solid (`OnMotionWarpingPreUpdate`)
- ❌ **Victim-side warp NOT implemented** - position calculated once, not continuously updated
- ❌ **CharacterMovement keeps running** - `bOrientRotationToMovement = true` in PlayerCharacter.cpp
- ❌ **No RootMotionMode configuration** - doesn't disable movement during paired animations
- ❌ **No sync point validation** - can't detect if characters are actually aligned at impact

**Risk Scenarios**:
| Scenario | Risk | Cause |
|----------|------|-------|
| Both characters have root motion | CRITICAL | Async drift, sync point missed |
| Attacker warping + victim moving | HIGH | Victim displacement causes misalignment |
| Attacker hits obstacle while warping | MEDIUM | Collision response overwrites warp |
| Victim on uneven terrain | MEDIUM | Z-adjustment doesn't track victim motion |

**Solution - Movement Disabling**:
```cpp
// At paired animation start
void UPairedAnimationManager::BeginPairedSequence()
{
    // Disable CharacterMovement input during paired animations
    if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
    {
        CMC->DisableMovement();  // Or set custom mode
        CMC->Velocity = FVector::ZeroVector;
        SavedMovementMode = CMC->MovementMode;
    }
}

// At paired animation end
void UPairedAnimationManager::EndPairedSequence()
{
    if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
    {
        CMC->SetMovementMode(SavedMovementMode);
    }
}
```

**Solution - Victim Warp Tracking** (mirror attacker system):
```cpp
// In TargetingComponent or new PairedAnimationManager
void UTargetingComponent::SetupVictimWarp(AActor* Attacker, const FPairedWarpConfig& Config)
{
    TrackedAttacker = Attacker;
    bIsTrackingAsVictim = true;
    MotionWarpingComponent->OnPreUpdate.AddDynamic(this, &OnVictimMotionWarpingPreUpdate);
}

void UTargetingComponent::OnVictimMotionWarpingPreUpdate(UMotionWarpingComponent* MWC)
{
    // Update victim's position relative to attacker's ACTUAL current location
    FVector AttackerLoc = TrackedAttacker->GetActorLocation();
    FTransform VictimTarget = CalculateVictimTransform(AttackerLoc, VictimWarpConfig);
    MWC->AddOrUpdateWarpTargetFromLocationAndRotation(VictimWarpConfig.WarpTargetName, VictimTarget);
}
```

### Gap 5: Capsule Collision & Mesh Penetration
**Source**: Agent a3cc744 exploration

**Problem**: During close paired animations, character capsules overlap with NO handling - characters clip through each other.

**Current State**:
- ✅ "SoftCollision" channel defined in DefaultEngine.ini - **BUT NEVER USED**
- ✅ Ragdoll collision handling exists (for death) - reference pattern available
- ❌ **No capsule-to-capsule overlap handling** - no depenetration logic
- ❌ **No soft collision system** - no OnComponentBeginOverlap bindings
- ❌ **Distance-based prevention only** - ValidatePairedAnimation() checks space at START only
- ❌ **No collision disable during paired animations** - capsules push each other apart
- ❌ **No tracked partner system** - can't target specific actors for collision ignore

**Solution - Tracked Partner Array + Targeted Collision Disable**:

**1. Add tracked partner array to CombatComponent** (supports multi-partner kills like double takedowns):
```cpp
// In CombatComponent.h
// Actors currently participating in paired animation with this character
UPROPERTY(BlueprintReadOnly, Category = "Combat|Paired Animation")
TArray<TWeakObjectPtr<AActor>> PairedAnimationPartners;

// API for managing partners
UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
void AddPairedPartner(AActor* Partner);

UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
void RemovePairedPartner(AActor* Partner);

UFUNCTION(BlueprintCallable, Category = "Combat|Paired Animation")
void ClearPairedPartners();

UFUNCTION(BlueprintPure, Category = "Combat|Paired Animation")
bool IsPairedPartner(AActor* Actor) const;
```

**2. AnimNotifyState_PairedAnimationCollision** (targeted collision only):
```cpp
UCLASS()
class UAnimNotifyState_PairedAnimationCollision : public UAnimNotifyState
{
    UPROPERTY(EditAnywhere)
    bool bDisablePawnCollision = true;  // Disable collision with partners only

    UPROPERTY(EditAnywhere)
    bool bDisableMovement = true;  // Disable CharacterMovement during paired anim

    virtual void NotifyBegin(...) override
    {
        // Get partners from CombatComponent->PairedAnimationPartners
        // Use MoveIgnoreActors API to ignore ONLY tracked partners
        // Save state for restoration
    }

    virtual void NotifyEnd(...) override
    {
        // Restore collision with partners
        // Re-enable movement
    }
};
```

**3. Use MoveIgnoreActors (preferred over global pawn ignore)**:
```cpp
// Targeted collision ignore (doesn't affect unrelated pawns)
UPrimitiveComponent* Capsule = Character->GetCapsuleComponent();
for (AActor* Partner : PairedAnimationPartners)
{
    Capsule->IgnoreActorWhenMoving(Partner, true);  // Ignore specific partner
}

// Restoration
for (AActor* Partner : PairedAnimationPartners)
{
    Capsule->IgnoreActorWhenMoving(Partner, false);  // Re-enable collision
}
```

**Benefits of Tracked Partner Approach**:
- Supports multi-partner kills (double takedowns, group finishers)
- Doesn't affect collision with unrelated pawns (allies, environmental characters)
- Easier to debug (clear partner list vs global pawn ignore)
- Sets up infrastructure for future features (partner-specific effects, damage sharing)

### Gap 6: Gameplay Tags Not Used at Runtime
**Source**: Agent afc089b exploration

**Problem**: Gameplay Tags exist in AttackData and HitReactionData but are **ONLY used for editor validation**, not runtime animation selection.

**Current State**:
- ✅ `AttackTags` and `RequiredContextTags` declared in AttackData.h (lines 285-309)
- ✅ `ReactionTags` and `RequiredContextTags` declared in HitReactionData.h (lines 150-164)
- ✅ Tags used for cycle detection in editor validation
- ❌ **TODO comment never implemented**: `// TODO: Check RequiredContextTags against ActiveContext` (MontageUtilityLibrary.cpp:1167)
- ❌ **No context accumulation** - no system tracks current combat state tags
- ❌ **No tag-based animation selection** - selection uses enum-based lookup only

**Opportunity**: Expand tags for context-aware paired animation selection.

**Solution - Tag-Based Animation Selection** (Phase 6 expansion):
```cpp
// New: Context resolver for paired animations
struct FPairedAnimationContext
{
    FGameplayTagContainer AttackerCapabilities;  // From AttackData.AttackTags
    FGameplayTagContainer VictimState;           // Guard broken, stunned, low health
    FGameplayTagContainer ActiveCombatContext;   // Combo active, holding, phase
    EFinisherTriggerReason TriggerReason;
    float TargetDistance;
};

// Extension to HitReactionSettings
UPairedAnimationData* UHitReactionSettings::SelectPairedAnimationByContext(
    EPairedReactionType Type,
    const FPairedAnimationContext& Context)
{
    // Filter available animations by RequiredContextTags
    // Score by tag match quality
    // Return best matching animation
}

// Context accumulation in CombatComponent
FGameplayTagContainer UCombatComponent::BuildCurrentCombatContext()
{
    FGameplayTagContainer Context;
    if (bHasComboActive) Context.AddTag(FGameplayTag::RequestGameplayTag("Context.Combo"));
    if (bIsHoldingAttack) Context.AddTag(FGameplayTag::RequestGameplayTag("Context.Holding"));
    if (CurrentPhase == EAttackPhase::Active) Context.AddTag(FGameplayTag::RequestGameplayTag("Context.Phase.Active"));
    // ... accumulate from current combat state
    return Context;
}
```

**Recommendation**: Tag expansion is Phase 6 work. Current paired animation system should work without it, but infrastructure prepares for context-aware selection.

### Gap 7: Hit Stop/Hit Pause (Sakurai Technique) - CRITICAL
**Source**: Web research on Masahiro Sakurai's game design principles

**What is Hitstop?**
From Sakurai's Famitsu column and YouTube channel "Masahiro Sakurai on Creating Games":
- Brief freeze when attack connects - both attacker AND victim freeze
- Emphasizes impact power and gives players' eyes time to process
- Different from hitstun (which only affects victim and enables combos)

**Sakurai's Implementation Principles:**
1. **Both parties freeze** - attacker and victim freeze for identical duration
2. **Damage-proportional duration** - more damage = longer freeze (0.03-0.1s typical)
3. **Per-attack customization** - not universal; Marth's sword tip gets more hitstop than blade edge
4. **Character vibration** - characters vibrate during hitstop (grounded = horizontal, airborne = vertical)
5. **Selective freezing** - ONLY hit participants freeze; background, particles, other players continue
6. **Smooth transition** - Characters transition from flinch to hurt animation over 4 frames DURING hitstop

**Paired Animation Relevance:**
- Hitstop is CRITICAL for sync points in finishers/counters
- Creates dramatic "impact moment" when weapon connects
- Sync point is perfect trigger for hitstop

**Current State:**
- ✅ `AnimNotifyState_PairedAnimationSync` has `bApplyHitPause` and `HitPauseDuration` properties
- ❌ **NO hitstop system implemented** - properties exist but never trigger

**UE5 Implementation Options:**
1. **Montage Pause** - `UAnimInstance::Montage_Pause()` pauses animation playback
2. **Custom Time Dilation** - `SetCustomTimeDilation(0.0f)` freezes specific actors
3. **Animation Playrate** - Set playrate to 0.0 for duration
4. **Cronus Hitstop plugin** - Commercial solution with mesh shake, audio ducking

**Recommended Implementation:**
```cpp
// In CombatComponent or new HitstopComponent
void ApplyHitstop(AActor* Attacker, AActor* Victim, float Duration)
{
    // Freeze both participants
    Attacker->CustomTimeDilation = 0.0f;
    Victim->CustomTimeDilation = 0.0f;

    // Add mesh vibration effect
    ApplyMeshVibration(Attacker, Duration);
    ApplyMeshVibration(Victim, Duration);

    // Schedule restoration
    GetWorldTimerManager().SetTimer(HitstopHandle, [=]()
    {
        Attacker->CustomTimeDilation = 1.0f;
        Victim->CustomTimeDilation = 1.0f;
    }, Duration, false);
}
```

**Priority**: P1 (after core collision/warp, but before camera effects - hitstop makes finishers feel impactful)

### Gap 8: AI/Enemy Coordination (Attack Token System)
**Source**: Comprehensive gap exploration agent + Research Agent ac53a6f

**Problem**: Multiple enemies can attack simultaneously during paired animations, guaranteeing hits on locked player.

**Existing Infrastructure (Agent ac53a6f findings)**:
- ✅ `ECombatState::Finishing` already exists in CombatTypes.h - set during finishers
- ✅ StateTree plugin enabled - can add condition nodes for AI decisions
- ✅ CombatComponent accessible from AI controllers
- ❌ No token management system
- ❌ AI doesn't query target combat state before attacking

**Solution - UCombatTokenSubsystem (UGameInstanceSubsystem)**:
```cpp
UCLASS()
class UCombatTokenSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    // Per-target token pools (Batman/DOOM pattern)
    // Each target has its own pool of attackers
    TMap<TWeakObjectPtr<AActor>, FTokenPool> TargetTokenPools;

    // Request token to attack a specific target
    // Returns false if pool exhausted or target in Finishing state
    UFUNCTION(BlueprintCallable)
    bool RequestAttackToken(AActor* Requester, AActor* Target);

    UFUNCTION(BlueprintCallable)
    void ReleaseAttackToken(AActor* Requester, AActor* Target);

    // Query target state before attacking
    UFUNCTION(BlueprintPure)
    bool CanAttackTarget(AActor* Target) const;

    // Configuration
    int32 MaxTokensPerTarget = 3;          // Normal combat: 3 attackers max
    int32 MaxTokensDuringFinisher = 1;     // During finisher: only executor attacks
};

// StateTree condition node for AI
USTRUCT()
struct FStateTreeCondition_CanAttackTarget : public FStateTreeConditionBase
{
    // Checks: token available AND target not in Finishing state
    bool TestCondition(FStateTreeExecutionContext& Context) const;
};
```

**Integration Points**:
1. AI BehaviorTree/StateTree: Check `CanAttackTarget()` before attack decision
2. CombatComponent: Call `ReleaseAttackToken()` when attack ends
3. Finisher start: Automatically restrict tokens to 1 for that target
4. Finisher end: Restore normal token limit

**Priority**: P2 (can work without, but causes unfair multi-hit scenarios)

### Gap 9: Input Handling During Paired Animations
**Source**: Comprehensive gap exploration agent

**Problem**: Input buffering still captures button presses during finishers, leading to unintended actions.

**Missing Handling:**
- Input not explicitly blocked during finisher cinematics
- Camera look input undefined (should lock?)
- Evade/block buttons still functional during paired animation
- Pause menu behavior during finishers undefined

**Solution:**
```cpp
// In PlayerController or CombatComponent
void OnPairedAnimationStarted(EPairedReactionType Type, bool bIsCritical)
{
    if (Type == EPairedReactionType::Finisher)
    {
        // Block combat input, allow only camera
        SetInputMode(EInputMode::CameraOnly);
        bBlockCombatInput = true;
    }
}

void OnPairedAnimationEnded(EPairedReactionType Type)
{
    SetInputMode(EInputMode::Full);
    bBlockCombatInput = false;
}
```

**Priority**: P2 (prevents accidental input during cinematics)

### Gap 10: Audio Synchronization
**Source**: Comprehensive gap exploration agent

**Missing Audio Systems:**
- Impact sounds not triggered at sync points
- Voice lines/grunts not synced with victim reactions
- No music ducking during finisher slow-motion
- Contact point not used for spatial audio positioning

**Solution - Audio Event Trigger at Sync Point:**
```cpp
// In AnimNotifyState_PairedAnimationSync
UPROPERTY(EditAnywhere, Category = "Audio")
USoundBase* ImpactSound;

UPROPERTY(EditAnywhere, Category = "Audio")
float MusicDuckingAmount = -6.0f;  // dB reduction

void NotifyTick(...)
{
    if (bAtSyncPoint && ImpactSound)
    {
        FVector ContactPoint = UPairedAnimationUtilityLibrary::CalculateContactPoint(...);
        UGameplayStatics::PlaySoundAtLocation(World, ImpactSound, ContactPoint);
    }
}
```

**Priority**: P3 (polish, but significantly improves feel)

### Gap 11: State Transitions & Edge Cases
**Source**: Comprehensive gap exploration agent

**Critical Edge Cases Identified:**
| Scenario | Risk | Missing Handler |
|----------|------|-----------------|
| Attacker dies mid-finisher | Victim locked in animation | Finisher cancellation callback |
| Victim becomes invulnerable | Damage sync point fails | Pre-sync invulnerability check |
| External DoT during paired anim | Unexpected damage/death | Damage filtering flag |
| Animation montage fails to play | Attacker plays, victim doesn't | Graceful degradation fallback |
| Component destroyed mid-animation | Null reference crash | Safe component checking |

**Solution - Interrupt Handler:**
```cpp
// Bind to attacker death
AttackerHealthComp->OnDeath.AddDynamic(this, &OnAttackerDiedDuringFinisher);

void OnAttackerDiedDuringFinisher()
{
    // Cancel victim's animation, restore state
    VictimAnimInstance->Montage_Stop(0.1f, CurrentVictimMontage);
    VictimCombatComp->ClearPairedPartners();
    VictimCombatComp->SetCombatState(ECombatState::Idle);

    // Apply partial damage or skip
    // Trigger ragdoll or recovery animation
}
```

**Priority**: P1 (prevents soft-locks and crashes)

### Gap 12: Pose Recovery Integration
**Source**: Pose recovery exploration agent

**Existing Infrastructure:**
- ✅ `HitReactionComponent::SavePoseSnapshot()` - saves pose for death/recovery
- ✅ `bPauseAnims = true` technique for freezing at current pose
- ✅ Standard 0.1s blend time for montage transitions
- ❌ No inertialization (UE5.6 supports it, not implemented)

**For Paired Animations:**
- May need pose snapshot before entering paired animation (for interruption recovery)
- `bPauseAnims` can freeze at current pose during hitstop
- Standard blend handles most recovery cases

**Implementation if needed:**
```cpp
void BeginPairedAnimation()
{
    // Optionally snapshot pose in case of interruption
    AnimInstance->SavePoseSnapshot(TEXT("PrePairedAnimation"));
}

void OnPairedAnimationInterrupted()
{
    // Blend back to saved pose over 0.2s
    AnimInstance->Montage_Stop(0.2f);
    // AnimBP can use saved snapshot for smoother recovery
}
```

**Priority**: P3 (existing blend system handles most cases, snapshot is extra polish)

---

## Existing Infrastructure (75% Ready)

### Already Implemented
| Component | Location | Status |
|-----------|----------|--------|
| `EPairedReactionType` enum | CombatTypes.h:154-162 | ✅ Complete |
| Paired fields in HitReactionData | HitReactionData.h:115-136 | ✅ Complete |
| `PlayPairedReaction()` API | HitReactionComponent.h:200-207 | ✅ Complete |
| Counter/Finisher TMap lookup | HitReactionSettings.h:95-155 | ✅ Complete |
| Attacker continuous warp tracking | TargetingComponent.cpp | ✅ Complete |
| Ground sampling utilities | DebugUtils.h:14-38 | ✅ Complete |
| Phase/window event system | AnimNotifyState_ActionWindow_Base | ✅ Complete |
| Finishing combat state | CombatTypes.h:22-36 | ✅ Complete |

### Needs Implementation (Updated 2026-01-30 - Post Phase 5b-3)
| Component | Priority | Complexity | Status |
|-----------|----------|------------|--------|
| AttackData paired fields | P0 | Low | ✅ DONE (Phase 5a) |
| Victim-side warp setup | P0 | Medium | ✅ DONE - `SetupVictimWarp()` |
| AnimNotifyState_PairedSync | P0 | Medium | ✅ DONE (Phase 5a) |
| PairedAnimationPartners array | P0 | Low | ✅ DONE - Full API implemented |
| AnimNotifyState_PairedAnimationCollision | P0 | Medium | ✅ DONE - Uses tracked partners |
| Movement disabling during paired anims | P0 | Low | ✅ DONE - In AnimNotifyState |
| Victim continuous warp tracking | P0 | Medium | ✅ DONE - `OnVictimMotionWarpingPreUpdate` |
| Attacker paired warp tracking | P0 | Medium | ✅ DONE - `SetupAttackerPairedWarp()` |
| CinematicEffectsUtilityLibrary | P1 | Low | ✅ DONE - Slow-mo, hitstop, camera shake |
| Hit Stop/Hit Pause (Sakurai) | P1 | Medium | ✅ DONE - `ApplyHitstop()` |
| State transition edge case handlers | P1 | Medium | ✅ DONE - `OnPairedPartnerDeath()`, `CancelPairedAnimation()` |
| Sync point delegate/events | P1 | Low | ✅ DONE (Phase 5a) |
| Finisher execution flow | P1 | Medium | ✅ DONE - `TryExecuteFinisher()` |
| Finisher vulnerability query | P1 | Low | ✅ DONE - `IsVulnerableToFinisher()`, `GetFinisherTriggerReason()` |
| Input blocking during paired anims | P2 | Low | ✅ DONE - `bBlockCombatInput` |
| Obstacle validation | P2 | Medium | ✅ DONE - `ValidatePairedAnimation()` |
| **Sync point alignment validation** | **P2** | **Low** | ⏳ Pending - Phase 5b-4 |
| **AI attack token coordination** | **P2** | **Medium** | ⏳ Pending - Phase 5b-5 |
| **Wire CinematicEffects to delegates** | **P2** | **Low** | ⏳ Pending - Gap 16.4 |
| Impact normal extraction | P2 | Low | ⏳ Pending |
| **Audio sync at impact points** | **P3** | **Low** | ⏳ Pending - Slots scaffolded |
| **VFX spawn at sync points** | **P3** | **Low** | ⏳ Pending - Slots scaffolded |
| Pose recovery for interruption | P3 | Low | 🔮 Optional (blend handles most) |
| IK contact point adjustment | P3 | High | 🔮 Deferred to Phase 6 |
| Tag-based animation selection | P3 | High | 🔮 Deferred to Phase 6 |

---

## Implementation Architecture

### Phase 5a: Finisher System (Foundation)

#### New Files to Create

**1. `PairedAnimationUtilityLibrary.h/.cpp`**
Static utility functions for paired animation math:
```cpp
UCLASS()
class KATANACOMBAT_API UPairedAnimationUtilityLibrary : public UBlueprintFunctionLibrary
{
    // Position calculation
    UFUNCTION(BlueprintPure)
    static FTransform CalculateVictimPosition(AActor* Attacker, AActor* Victim, const FPairedAnimationData& Data);

    // Contact point alignment
    UFUNCTION(BlueprintPure)
    static FVector GetBestContactPoint(USkeletalMeshComponent* Mesh, FName BoneName, FVector TargetLocation);

    // Obstacle validation
    UFUNCTION(BlueprintPure)
    static bool IsPositionClearForPairedAnimation(UWorld* World, const FTransform& AttackerTransform, const FTransform& VictimTransform, float ClearanceRadius);

    // Terrain alignment
    UFUNCTION(BlueprintPure)
    static FTransform AdjustTransformToTerrain(UWorld* World, const FTransform& Transform, float CapsuleHalfHeight, AActor* IgnoreActor);
};
```

**2. `PairedAnimationData.h` (Data Asset)**
```cpp
UCLASS(BlueprintType)
class KATANACOMBAT_API UPairedAnimationData : public UPrimaryDataAsset
{
    // Animation references
    UPROPERTY(EditAnywhere, Category = "Animation")
    TObjectPtr<UAnimMontage> AttackerMontage;

    UPROPERTY(EditAnywhere, Category = "Animation")
    TObjectPtr<UAnimMontage> VictimMontage;

    // Sync configuration
    UPROPERTY(EditAnywhere, Category = "Sync")
    float SyncPointTime = 0.0f;  // When damage/effect triggers

    UPROPERTY(EditAnywhere, Category = "Sync")
    float AttackerBlendIn = 0.1f;

    UPROPERTY(EditAnywhere, Category = "Sync")
    float VictimBlendIn = 0.1f;

    // Positioning
    UPROPERTY(EditAnywhere, Category = "Positioning")
    FVector VictimOffset = FVector(100.f, 0.f, 0.f);  // Relative to attacker

    UPROPERTY(EditAnywhere, Category = "Positioning")
    bool bVictimFacesAttacker = true;

    // Warp configuration
    UPROPERTY(EditAnywhere, Category = "Warping")
    FPairedWarpConfig AttackerWarp;

    UPROPERTY(EditAnywhere, Category = "Warping")
    FPairedWarpConfig VictimWarp;

    // Effects
    UPROPERTY(EditAnywhere, Category = "Effects")
    bool bApplySlowMotion = false;

    UPROPERTY(EditAnywhere, Category = "Effects", meta = (EditCondition = "bApplySlowMotion"))
    float SlowMotionScale = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Effects", meta = (EditCondition = "bApplySlowMotion"))
    float SlowMotionDuration = 0.5f;
};
```

**3. `AnimNotifyState_PairedAnimationSync.h/.cpp`**
```cpp
UCLASS()
class KATANACOMBAT_API UAnimNotifyState_PairedAnimationSync : public UAnimNotifyState
{
    UPROPERTY(EditAnywhere, Category = "Sync")
    FName SyncPointName = "FinisherSync";

    // Called when sync point is reached
    virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
};
```

#### Extensions to Existing Files

**AttackData.h** - Add paired animation fields:
```cpp
// Paired Animation Configuration
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paired Animation")
FName FinisherAnimationName = NAME_None;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paired Animation")
TObjectPtr<UPairedAnimationData> FinisherData;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paired Animation")
FName CounterAnimationName = NAME_None;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paired Animation")
TObjectPtr<UPairedAnimationData> CounterData;
```

**TargetingComponent.h** - Add victim warp API:
```cpp
UFUNCTION(BlueprintCallable, Category = "Paired Animation")
bool SetupVictimWarp(AActor* Attacker, const FPairedWarpConfig& Config);

UFUNCTION(BlueprintCallable, Category = "Paired Animation")
void ClearVictimWarp();
```

**CombatTypes.h** - Add victim warp config:
```cpp
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FPairedWarpConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName WarpTargetName = "PairedTarget";

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxWarpDistance = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bWarpTranslation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bWarpRotation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAdjustToTerrain = true;
};
```

### Finisher Trigger System

**Multiple trigger conditions (AC3-style accessibility):**
```cpp
USTRUCT(BlueprintType)
struct FFinisherTriggerConfig
{
    // Low health trigger
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bTriggerOnLowHealth = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bTriggerOnLowHealth"))
    float HealthThreshold = 0.25f;  // Below 25% health

    // Guard break trigger
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bTriggerOnGuardBreak = true;

    // Stun trigger (from heavy attacks)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bTriggerOnStun = true;

    // Visual feedback
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bShowFinisherPrompt = true;  // UI indicator when finisher available
};
```

**Query function in HitReactionComponent:**
```cpp
UFUNCTION(BlueprintPure, Category = "Finisher")
bool IsVulnerableToFinisher() const;

UFUNCTION(BlueprintPure, Category = "Finisher")
EFinisherTriggerReason GetFinisherTriggerReason() const;
```

### Slow-Motion System (Separate Module)

**Delegate-driven design:**
```cpp
// In CombatTypes.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPairedAnimationStarted, EPairedReactionType, Type, bool, bIsCriticalMoment);

// In separate TimeManager or CinematicEffectsComponent
UFUNCTION()
void OnPairedAnimationTriggered(EPairedReactionType Type, bool bIsCriticalMoment);
```

**Configurable per-context:**
- Parry success → optional slow-mo (configurable)
- Counter execution → brief slow-mo
- Finisher → dramatic slow-mo with camera
- Can be disabled globally for performance testing

### Phase 5b: Parry/Counter System (Builds on 5a)

Uses same infrastructure with additional:
- Parry window detection (already exists)
- Counter window state on enemy
- Role reversal (parrier becomes attacker)

---

## File Modifications Summary

### Already Created (Phase 5a - COMPLETE)
| File | Purpose | Status |
|------|---------|--------|
| `Public/Utilities/PairedAnimationUtilityLibrary.h` | Static utility functions | ✅ |
| `Private/Utilities/PairedAnimationUtilityLibrary.cpp` | Implementation | ✅ |
| `Public/Data/PairedAnimationData.h` | Data asset for paired anims | ✅ |
| `Private/Data/PairedAnimationData.cpp` | Implementation | ✅ |
| `Public/Data/PairedAnimationTypes.h` | Structs (FPairedWarpConfig, etc.) | ✅ |
| `Public/Animation/AnimNotifyState_PairedAnimationSync.h` | Sync point notify | ✅ |
| `Private/Animation/AnimNotifyState_PairedAnimationSync.cpp` | Implementation | ✅ |

### Already Modified (Phase 5a - COMPLETE)
| File | Changes | Status |
|------|---------|--------|
| `CombatTypes.h` | Paired animation delegates | ✅ |
| `AttackData.h` | FinisherData, CounterData fields | ✅ |

### New Files to Create (Phase 5b)
| File | Purpose | Priority |
|------|---------|----------|
| `Public/Animation/AnimNotifyState_PairedAnimationCollision.h` | Collision disable during paired | P0 |
| `Private/Animation/AnimNotifyState_PairedAnimationCollision.cpp` | Implementation | P0 |

### Files to Modify (Phase 5b)
| File | Changes | Priority |
|------|---------|----------|
| `TargetingComponent.h/.cpp` | Add `SetupVictimWarp()`, `ClearVictimWarp()` | P0 |
| `HitReactionComponent.h/.cpp` | Add `IsVulnerableToFinisher()`, `GetFinisherTriggerReason()` | P0 |
| `CombatComponent.h/.cpp` | Wire camera shake + slow-mo to delegates, finisher execution flow | P1 |
| `CombatSettings.h` | Add `FFinisherTriggerConfig` defaults | P1 |

---

## Verification Plan

### Build Verification
```powershell
cd "C:\Program Files\Epic Games\UE_5.6\Engine\Source"
dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" KatanaCombatEditor Win64 Development "-Project=D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -WaitMutex
```

### Test Verification
```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.PairedAnimation" -unattended -nopause -NullRHI -nosplash -stdout
```

### Manual Testing Checklist

**Phase 5b-1: Collision & Warp**
- [ ] Characters don't push apart during close paired animations
- [ ] Collision restores correctly after animation completes
- [ ] Collision restores on animation interruption
- [ ] Victim warps to correct position relative to attacker

**Phase 5b-2: Camera & Effects**
- [ ] Slow-motion activates when `bApplySlowMotion = true`
- [ ] Slow-motion duration respects `SlowMotionDuration`
- [ ] Time dilation restores to 1.0 after animation
- [ ] Camera shake plays at sync point
- [ ] No permanent slow-motion on edge cases (interruption, death)
- [ ] Hitstop freezes BOTH attacker and victim at sync point
- [ ] Hitstop duration proportional to damage (configurable)
- [ ] Mesh vibration during hitstop (grounded = horizontal, airborne = vertical)
- [ ] Background and particles continue during hitstop (selective freeze)

**Phase 5b-2.5: Edge Case Handling**
- [ ] Attacker death mid-finisher cancels victim animation
- [ ] Victim invulnerability checked before damage sync point
- [ ] External damage filtered during paired animation (optional)
- [ ] Graceful fallback if montage fails to play

**Phase 5b-3: Finisher Flow**
- [ ] `IsVulnerableToFinisher()` returns true below health threshold
- [ ] `IsVulnerableToFinisher()` returns true on guard break
- [ ] `IsVulnerableToFinisher()` returns true on stun
- [ ] Finisher triggers from attack input during recovery
- [ ] Both characters animate in sync
- [ ] Damage applies at sync point

**Environment Testing**
- [ ] Works on flat ground
- [ ] Works on slopes (terrain adjustment)
- [ ] Blocked by obstacles (validation)
- [ ] Handles height differences (victim adjustment)

---

## Implementation Order

### ~~Week 1: Foundation~~ ✅ COMPLETE (Commit e7c8354)
1. ~~Create `FPairedWarpConfig` struct~~ ✅
2. ~~Create `UPairedAnimationData` data asset~~ ✅
3. ~~Add paired fields to AttackData~~ ✅
4. ~~Create `UPairedAnimationUtilityLibrary`~~ ✅
5. ~~Create `AnimNotifyState_PairedAnimationSync`~~ ✅
6. ~~Add paired animation delegates to CombatTypes.h~~ ✅

### Phase 5b-1: Collision, Root Motion & Warp (CURRENT PRIORITY)
7. **Add PairedAnimationPartners array to CombatComponent** - Partner Tracking
   - Add `TArray<TWeakObjectPtr<AActor>> PairedAnimationPartners`
   - Add `AddPairedPartner()`, `RemovePairedPartner()`, `ClearPairedPartners()`, `IsPairedPartner()` API
   - Supports multi-partner kills (double takedowns, group finishers)
   - Enables targeted collision ignore (vs global pawn ignore)

8. **Update `AnimNotifyState_PairedAnimationCollision`** - Targeted Collision Fix
   - Get partners from CombatComponent->PairedAnimationPartners
   - Use `Capsule->IgnoreActorWhenMoving()` to ignore ONLY tracked partners
   - Restore collision with partners on NotifyEnd
   - Include movement disabling option (bDisableMovement flag)
   - Save/restore state for interruption handling

9. **Add `SetupVictimWarp()` to TargetingComponent** - Core Warp Infrastructure
   - Mirror existing `SetupAttackWarp()` API
   - Accept `FPairedWarpConfig` for victim positioning
   - Store TrackedAttacker reference for continuous updates
   - Automatically add/remove from PairedAnimationPartners

10. **Implement victim continuous warp tracking** - Root Motion Gap Fix
    - Add `OnVictimMotionWarpingPreUpdate()` callback
    - Calculate victim position relative to attacker's ACTUAL current location (not initial)
    - Apply terrain adjustment to prevent floating
    - Update warp target every frame during paired animation

### Phase 5b-2: Hit Stop, Camera & Effects
11. **Implement Sakurai-style Hitstop System** - Game Feel Critical
    - Create `ApplyHitstop(AActor* Attacker, AActor* Victim, float Duration)` in CombatComponent
    - Use `CustomTimeDilation = 0.0f` to freeze both participants
    - Add mesh vibration during freeze (grounded = horizontal, airborne = vertical)
    - Ensure background/particles continue (selective freeze)
    - Wire to `OnPairedAnimationSyncPoint` when `bApplyHitPause = true`
12. Wire `OnPairedAnimationStarted` → slow-motion in CombatComponent
13. Wire `OnPairedAnimationSyncPoint` → camera shake trigger + hitstop trigger
14. Wire `OnPairedAnimationEnded` → restore time dilation
15. Add time dilation restore timer with safeguard (prevent permanent slow-mo on interruption)

### Phase 5b-2.5: State Transition Edge Cases
16. **Add finisher interrupt handler** - Prevents Soft-Locks
    - Bind to attacker death event during paired animation
    - Cancel victim montage, restore state on attacker death
    - Add graceful fallback if victim montage fails to play
17. **Add input blocking during paired animations** - Polish
    - Block combat input (attacks, evades) during finisher cinematics
    - Allow camera input only
    - Restore full input on `OnPairedAnimationEnded`

### Phase 5b-3: Finisher Flow
18. Add `IsVulnerableToFinisher()` to HitReactionComponent
19. Add `GetFinisherTriggerReason()` to HitReactionComponent
20. Implement finisher execution flow in CombatComponent
21. Add finisher input detection (during attack recovery)

### Phase 5b-4: Validation & Polish (DETAILED)

**22. Sync Point Alignment Validation** (Gap 16.2, Root Motion)
```cpp
// In AnimNotifyState_PairedAnimationSync::NotifyTick
UPROPERTY(EditAnywhere, Category = "Validation")
float MaxContactDistance = 150.0f;  // Units

UPROPERTY(EditAnywhere, Category = "Validation")
bool bLogMisalignment = true;

UPROPERTY(EditAnywhere, Category = "Validation")
bool bNudgeOnMinorMisalignment = true;  // Auto-correct if < 50 units off

float NudgeThreshold = 50.0f;
```
- At sync point time, measure actual distance between attacker and victim
- If `Distance > MaxContactDistance`: Log warning, apply damage anyway (graceful degradation)
- If `Distance < NudgeThreshold && bNudgeOnMinorMisalignment`: Teleport victim to correct offset
- Track misalignment frequency in debug stats for animation tuning

**23. Comprehensive Test Suite** (Files: `Source/KatanaCombatTest/Private/PairedAnimation/`)
```
PairedAnimationSyncTests.cpp
├── TestSyncPointDelegatesFire
├── TestSyncPointTimingAccuracy
├── TestMisalignmentDetection
└── TestMisalignmentNudge

PairedAnimationWarpTests.cpp
├── TestAttackerWarpTracking
├── TestVictimWarpTracking
├── TestSymmetricPartnerRegistration
├── TestWarpCleanupOnInterrupt
└── TestTerrainAdjustment

PairedAnimationCollisionTests.cpp
├── TestPartnerCollisionDisabled
├── TestUnrelatedPawnCollisionPreserved
├── TestCollisionRestoredAfterAnimation
└── TestCollisionRestoredOnInterrupt

FinisherFlowTests.cpp
├── TestFinisherTriggersOnGuardBreak
├── TestFinisherTriggersOnStun
├── TestFinisherTriggersOnLowHealth
├── TestFinisherPriorityOrder
├── TestFinisherVictimMutex
├── TestFinisherInputBlocking
└── TestFinisherCancellation

CinematicEffectsTests.cpp
├── TestSlowMotionActivation
├── TestSlowMotionRestore
├── TestHitstopFreeze
├── TestHitstopRestoreOnInterrupt
└── TestCameraShakeTrigger
```

**24. Example PairedAnimationData Asset**
Create `Content/ProjectFiles/Data/Combat/PairedAnimations/DA_Finisher_Katana_Front.uasset`:
```
AttackerMontage: AM_Finisher_Katana_Front_Attacker
VictimMontage: AM_Finisher_Katana_Front_Victim
SyncPointTime: 0.8f
AttackerBlendIn: 0.15f
VictimBlendIn: 0.1f
VictimOffset: (X=100, Y=0, Z=0)
bVictimFacesAttacker: true
AttackerWarpConfig:
  WarpTargetName: "FinisherWarp"
  MaxWarpDistance: 300.0f
  bAdjustToTerrain: true
VictimWarpConfig:
  WarpTargetName: "VictimWarp"
  MaxWarpDistance: 100.0f
  bAdjustToTerrain: true
bApplySlowMotion: true
SlowMotionScale: 0.3f
SlowMotionDuration: 0.5f
ImpactCameraShake: CS_MediumImpact
```

**25. Manual Testing Matrix**
| Environment | Test Case | Pass Criteria |
|-------------|-----------|---------------|
| Flat ground | Execute finisher | Both characters aligned, sync point hit |
| 10° slope | Execute finisher uphill | Victim Z-adjusted, no floating |
| 10° slope | Execute finisher downhill | Attacker terrain-adjusted |
| Near wall | Execute finisher toward wall | Validation rejects, no finisher |
| Moving target | Execute finisher on walking enemy | Warp tracks, sync achieved |
| Ledge | Execute finisher at ledge edge | Validation rejects or safe positioning |
| During hitstop | Trigger finisher on hitstop victim | Hitstop clears, finisher executes |
| Rapid input | Spam finisher button | Only one execution, debounced |

**26. API Documentation Update** (ATTACK_CREATION.md)
Add new section "Paired Animation Setup":
- How to create PairedAnimationData asset
- Required AnimNotifyStates for montages
- Warp target naming conventions
- Finisher trigger configuration
- Testing checklist for new paired animations

### Phase 5b-5: AI Coordination & Safety (DETAILED)

**27. UCombatTokenSubsystem Implementation** (Gap 1.1, 8.1)

File: `Source/KatanaCombat/Public/Systems/CombatTokenSubsystem.h`
```cpp
UCLASS()
class KATANACOMBAT_API UCombatTokenSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // --- Token Management ---

    /** Request permission to attack a target. Returns false if max attackers reached or target in finisher. */
    UFUNCTION(BlueprintCallable, Category = "Combat|AI")
    bool RequestAttackToken(AActor* Requester, AActor* Target);

    /** Release attack token when attack ends or is interrupted. */
    UFUNCTION(BlueprintCallable, Category = "Combat|AI")
    void ReleaseAttackToken(AActor* Requester, AActor* Target);

    /** Check if target can be attacked (has available tokens AND not in finisher state). */
    UFUNCTION(BlueprintPure, Category = "Combat|AI")
    bool CanAttackTarget(AActor* Target) const;

    /** Get number of current attackers for a target. */
    UFUNCTION(BlueprintPure, Category = "Combat|AI")
    int32 GetActiveAttackerCount(AActor* Target) const;

    // --- Configuration ---

    /** Max simultaneous attackers per target in normal combat (default: 3). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxTokensPerTarget = 3;

    /** Max attackers when target is in finisher state (default: 1, the executor). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxTokensDuringFinisher = 1;

    /** Cooldown before same attacker can request token again (prevents rapid re-attacks). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float TokenCooldown = 1.0f;

private:
    // Per-target token pools
    struct FTokenPool
    {
        TArray<TWeakObjectPtr<AActor>> ActiveAttackers;
        TMap<TWeakObjectPtr<AActor>, float> CooldownTimers;  // Requester -> CooldownEndTime
    };

    TMap<TWeakObjectPtr<AActor>, FTokenPool> TargetTokenPools;

    // Internal helpers
    bool IsTargetInFinisherState(AActor* Target) const;
    int32 GetEffectiveMaxTokens(AActor* Target) const;
};
```

StateTree Integration (File: `Source/KatanaCombat/Public/AI/StateTreeConditions/StateTreeCondition_CanAttackTarget.h`):
```cpp
USTRUCT()
struct KATANACOMBAT_API FStateTreeCondition_CanAttackTarget : public FStateTreeConditionBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Input")
    TStateTreeExternalDataHandle<AActor> TargetHandle;

    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
```

Integration Points:
1. **AI StateTree**: `CanAttackTarget` condition before "Approach to Attack" state
2. **CombatComponent**: Call `ReleaseAttackToken()` in `OnAttackEnded()` delegate handler
3. **TryExecuteFinisher**: Finisher start → tokens auto-restricted to 1 for that target
4. **OnPairedAnimationEnded**: Restore normal token limit

**28. Victim Mutex Flag** (Gap 1.5) - ✅ COMPLETED
Already implemented in HitReactionComponent (Commit 9e67693):
```cpp
UPROPERTY(BlueprintReadOnly, Category = "Combat|Finisher")
bool bIsFinisherTarget = false;

// Set in TryExecuteFinisher():
TargetHitReaction->bIsFinisherTarget = true;

// Cleared in OnPairedAnimationEnded or CancelPairedAnimation():
TargetHitReaction->bIsFinisherTarget = false;
```

**29. Wire CinematicEffects to Delegates** (Gap 16.4)

The `CinematicEffectsUtilityLibrary` provides static functions but they're not auto-triggered.
Add delegate bindings in CombatComponent:

```cpp
// In CombatComponent::BeginPlay or initialization
void UCombatComponent::SetupPairedAnimationEffects()
{
    // Bind to sync point delegate
    OnPairedAnimationSyncPoint.AddDynamic(this, &UCombatComponent::HandlePairedAnimationSyncPoint);
    OnPairedAnimationStarted.AddDynamic(this, &UCombatComponent::HandlePairedAnimationStarted);
    OnPairedAnimationEnded.AddDynamic(this, &UCombatComponent::HandlePairedAnimationEnded);
}

void UCombatComponent::HandlePairedAnimationStarted(EPairedReactionType Type, bool bIsCritical)
{
    if (ActivePairedAnimData && ActivePairedAnimData->bApplySlowMotion)
    {
        UCinematicEffectsUtilityLibrary::ApplySlowMotion(
            GetWorld(),
            ActivePairedAnimData->SlowMotionScale,
            ActivePairedAnimData->SlowMotionDuration
        );
    }
}

void UCombatComponent::HandlePairedAnimationSyncPoint(EPairedReactionType Type, FName SyncName)
{
    if (ActivePairedAnimData)
    {
        // Camera shake
        if (ActivePairedAnimData->ImpactCameraShake)
        {
            UCinematicEffectsUtilityLibrary::TriggerCameraShake(
                GetWorld(),
                ActivePairedAnimData->ImpactCameraShake
            );
        }

        // Hitstop
        if (ActivePairedAnimData->bApplyHitPause)
        {
            UCinematicEffectsUtilityLibrary::ApplyHitstop(
                GetOwner(),
                CurrentPairedPartner,  // Victim
                ActivePairedAnimData->HitPauseDuration
            );
        }
    }
}
```

**30. Camera Midpoint Focus** (Gap 14.2)

For cinematic framing during finishers, calculate focus point:
```cpp
// In PlayerCameraManager or CinematicEffectsUtilityLibrary
UFUNCTION(BlueprintCallable, Category = "Cinematic")
static FVector CalculatePairedAnimationFocusPoint(AActor* Attacker, AActor* Victim)
{
    FVector AttackerLoc = Attacker->GetActorLocation();
    FVector VictimLoc = Victim->GetActorLocation();

    // Weight toward victim (they're the spectacle)
    return FMath::Lerp(AttackerLoc, VictimLoc, 0.6f);
}
```

### Phase 5b-6: Audio & VFX Scaffolding (NEW)
30. **Add audio property slots** - Gaps 4.1-4.4
    - `ImpactSound` to AttackData/PairedAnimationData
    - `VictimReactionSound` for voice lines
    - Wire to `OnPairedAnimationSyncPoint` with `PlaySoundAtLocation()`
31. **Add VFX property slots** - Gaps 15.1, 15.3
    - `ImpactVFX` (UNiagaraSystem*) to PairedAnimationData
    - `SlowMoPostProcess` (UMaterialInterface*) slot
    - (Implementation deferred, slots scaffolded)

### Phase 6: IK & Context Enhancement (FUTURE)
32. Contact point IK for hands/weapons (Batman Arkham style)
33. Spine pitch for height differences (For Honor style)
34. Control Rig integration (optional)
35. **Tag-based animation selection** - Gameplay Tags Gap Fix
    - Implement `FPairedAnimationContext` for context accumulation
    - Add `SelectPairedAnimationByContext()` to HitReactionSettings
    - Wire RequiredContextTags evaluation at runtime
    - Enable context-aware paired animation branching

### Phase 7: VFX Implementation (FUTURE)
36. Implement `ImpactVFX` spawning at contact point
37. Implement slow-motion post-process activation
38. Blood decals on victim mesh
39. Enhanced weapon trails during finishers
40. Screen blood splatter on high-damage hits

---

## API Verification Status

### UE5.6 APIs (Verified)
- [x] `UMotionWarpingComponent` - Available in UE5.6
- [x] `AddOrUpdateWarpTargetFromLocationAndRotation()` - Standard API
- [x] `UAnimNotifyState` - Core animation notify
- [x] `UPrimaryDataAsset` - Data asset base class
- [x] `UBlueprintFunctionLibrary` - Static function library

### Project APIs (Verified via Codebase Exploration)
- [x] `EPairedReactionType` - CombatTypes.h:154-162
- [x] `PlayPairedReaction()` - HitReactionComponent.h:200-207
- [x] `SetupAttackWarp()` - TargetingComponent.h:224
- [x] `OnMotionWarpingPreUpdate()` - TargetingComponent.h:269
- [x] `FGroundSampleResult` - DebugUtils.h:14-38
- [x] `AdjustLocationToGround()` - DebugUtils.h:314

---

## Risk Mitigation (Final Status - Post Phase 5b-3)

| Risk | Mitigation | Status | Implementation |
|------|-----------|--------|----------------|
| Animation clipping | Obstacle validation before triggering | ✅ Complete | `ValidatePairedAnimation()` |
| Terrain issues | Ground sampling utilities | ✅ Complete | `AdjustLocationToGround()` |
| Sync drift | Continuous warp tracking for BOTH actors | ✅ Complete | `SetupAttackerPairedWarp()` + `SetupVictimWarp()` |
| Root motion conflicts | Movement disabled via AnimNotifyState | ✅ Complete | `bDisableMovement` in collision notify |
| Capsule collision | Targeted ignore via partner array | ✅ Complete | `PairedAnimationPartners` + `IgnoreActorWhenMoving()` |
| Mesh penetration | Distance prevention + collision disable | ✅ Complete | Validation + targeted ignore |
| Permanent slow-mo | Timer-based restoration with safeguard | ✅ Complete | `RestoreTimeDilation()` + timer |
| Input during cinematic | Block combat input flag | ✅ Complete | `bBlockCombatInput` in `CanProcessInput()` |
| Attacker death | Cancel victim animation | ✅ Complete | `OnPairedPartnerDeath()` |
| Stacked finishers | Victim mutex flag | ✅ Complete | `bIsFinisherTarget` |
| Time dilation stacking | Track active dilation source | ⏳ Pending | Gap 17.5 - Phase 5b-4 |
| Distance validation | Range check before finisher | ⏳ Pending | Gap 16.2 - Phase 5b-4 |
| Network replication | Design for future, defer implementation | 🔮 Deferred | Phase 7+ |
| Performance | Event-driven, no tick overhead | ✅ Complete | OnPreUpdate callbacks |

---

## Sources Referenced

### Game Design Reference
- [GDC: Animating The 3rd Assassin](https://gdcvault.com/play/1017635/Animation-Bootcamp-Animating-The-3rd) - AC3 3200 animations
- [GDC: Motion Warping in Gears of War 4](https://www.gdcvault.com/play/1024219/Motion-Warping-in-Gears-of) - Warp points
- [GDC: Master of the Katana](https://gdcvault.com/play/1027194/Master-of-the-Katana-Melee) - GoT combat
- [Freeflow Arena UE5](https://discover.therookies.co/2025/10/28/building-a-cinematic-combat-system-in-unreal-engine-5/) - Modern recreation
- [Contextual Animation Plugin](https://vorixo.github.io/devtricks/contextual-anim/) - UE5.3+ paired anims
- [Motion Matching For Honor](https://www.gameanim.com/2016/05/03/motion-matching-ubisofts-honor/) - Sync techniques

### Sakurai Hitstop Technique
- [Source Gaming: Thinking About Hitstop](https://sourcegaming.info/2015/11/11/thoughts-on-hitstop-sakurais-famitsu-column-vol-490-1/) - Sakurai's Famitsu column on hitstop principles
- [GoNintendo: Sakurai's 8 Hit Stop Techniques](https://www.gonintendo.com/contents/13581-sakurai-s-latest-game-dev-video-features-8-hit-stop-techniques) - YouTube video breakdown
- [Infil Fighting Game Glossary: Hitstop](https://glossary.infil.net/?t=Hitstop) - Technical definition
- [Epic Forums: Hitstop with Montage Sections](https://forums.unrealengine.com/t/how-can-i-create-hitstop-system-base-on-different-section-of-the-montage/298007) - UE implementation discussion
- [Cronus Hitstop Plugin](https://www.unrealengine.com/marketplace/en-US/product/cronus-hitstop) - Commercial reference implementation

---

## Quick Reference: Critical Files

### Files Created in Phase 5a (Reference for Patterns)
| File | Key Functions/Patterns |
|------|------------------------|
| `PairedAnimationUtilityLibrary.cpp` | `CalculateVictimTransform()`, `ValidatePairedAnimation()`, `IsPathClear()`, `GetBoneWorldLocation()`, `CalculateContactPoint()` |
| `PairedAnimationData.h` | Data asset structure, warp configs, effects settings |
| `AnimNotifyState_PairedAnimationSync.cpp` | Sync point broadcasting pattern, delegate invocation |
| `PairedAnimationTypes.h` | `FPairedWarpConfig`, `FFinisherTriggerConfig`, `FPairedAnimationValidation` |

### Files to Modify in Phase 5b
| File | Changes Needed | Reference Pattern |
|------|----------------|-------------------|
| `HitReactionComponent.cpp:683-729` | Reference for collision handling pattern | Ragdoll collision management |
| `TargetingComponent.cpp:OnMotionWarpingPreUpdate()` | Pattern for victim warp tracking | Attacker continuous warp |
| `CombatComponent.cpp` | Add paired animation state management, movement disabling, camera/slow-mo wiring | Existing delegate binding |
| `PlayerCharacter.cpp:22-25` | Reference for CharacterMovement setup | Movement mode to disable |

### New Files to Create in Phase 5b
| File | Purpose | Priority |
|------|---------|----------|
| `AnimNotifyState_PairedAnimationCollision.h/.cpp` | Disable collision during paired animations | P0 |

### Key APIs for Root Motion Handling (UE5.6)
```cpp
// Movement Disabling
CharacterMovement->DisableMovement();       // Stops all movement
CharacterMovement->SetMovementMode(MOVE_None); // Alternative
CharacterMovement->Velocity = FVector::ZeroVector; // Clear momentum

// Collision Management (from HitReactionComponent ragdoll pattern)
Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

// Time Dilation
GetWorld()->GetWorldSettings()->SetTimeDilation(float Scale);
GetWorldTimerManager().SetTimer(Handle, Callback, Duration, false);

// Camera Shake
PlayerCameraManager->StartCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass);
```
| `CombatComponent.cpp` | Wire delegates to effects | Existing delegate binding |
| `CombatTypes.h:130-140` | Delegates already declared | Use existing delegates |

### Key APIs (UE 5.6)
```cpp
// Time Dilation
GetWorld()->GetWorldSettings()->SetTimeDilation(float Scale);
GetWorld()->GetWorldSettings()->TimeDilation; // Read current

// Camera Shake
APlayerCameraManager* PCM = GetPlayerController()->PlayerCameraManager;
PCM->StartCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass);

// Collision Management (from HitReactionComponent pattern)
Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

// Motion Warping
MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
    FName TargetName, FVector Location, FRotator Rotation);
```

---

## IK System Design (Phase 6 - Deferred)

### Recommended Architecture
Based on exploration agent a7f2b67 findings:

**Priority 1**: Contact Point IK (2-3 weeks)
- New `UProceduralIKComponent`
- Listen to `OnPairedAnimationSyncPoint` delegate
- Calculate delta from animation pose to contact target
- Apply via animation layering in Animation Blueprint

**Priority 2**: Spine Pitch (1 week)
- For Honor technique for height differences
- Simple calculation: `SpinePitchDegrees = HeightDelta / 100.0f`
- Apply to spine_02/spine_03 bones

**Priority 3**: Full Control Rig (2-3 weeks, optional)
- Runtime instantiation of existing Control Rigs
- Maximum flexibility but higher overhead
- Only if Phase 1-2 insufficient

### Existing Infrastructure Ready for IK
- ✅ `GetBoneWorldLocation()` in PairedAnimationUtilityLibrary
- ✅ `CalculateContactPoint()` for midpoint calculation
- ✅ AttackHand, VictimContactBone parameters in sync notify
- ✅ Control Rig assets in Content (CR_Mannequin_BasicFootIK, IK_Mannequin)
- ✅ Motion Warping plugin enabled

---

## EXECUTION PLAN: Documentation Sync

**Purpose**: Update CLAUDE.md and docs to reflect current project state accurately.

### Step 1: Update CLAUDE.md Active Development Section

Replace the existing "Active Development" section in `CLAUDE.md` with the comprehensive update from the "DOCUMENTATION UPDATE" section above. This includes:
- Detailed paired animation system status tables
- Clear distinction between Implemented / Scaffolded / Planned
- Key design decisions with rationale
- Entry points for finisher flow
- Core combat system status
- Deferred systems

**File**: `D:\UnrealProjects\5.6\KatanaCombat\CLAUDE.md`
**Section**: Lines 253-271 (Active Development)

### Step 2: Verify Implementation Files Match Plan

Quick verification that recent fixes are in place:
- `HitReactionComponent.h`: `bDeathHandledByPairedAnimation`, `SetDeathHandledByPairedAnimation()`
- `HitReactionComponent.cpp`: Flag check in `PlayDeathReaction()`
- `CombatComponent.cpp`: `CompletePairedAnimation()` calls `SetDeathHandledByPairedAnimation()`

### Step 3: Test Verification (After Rebuild)

1. Close Unreal Editor
2. Rebuild project
3. Launch editor and play
4. Test finisher:
   - Attack enemy until health < 20%
   - Execute finisher (should be TryExecuteFinisher via attack input)
   - Verify: Victim ragdolls/holds pose directly (NO double death animation)
   - Verify: Player input unblocked after finisher
   - Verify: Debug HUD shows correct state transitions

### Files to Modify

| File | Change |
|------|--------|
| `CLAUDE.md` | Replace Active Development section with comprehensive status |

### Verification

After applying changes:
1. Read CLAUDE.md to confirm update applied
2. Check git status shows only CLAUDE.md modified
3. Optionally commit with message: "Update CLAUDE.md with comprehensive paired animation system status"

---

## EXECUTION PLAN: Create Detailed Spec File

**Purpose**: Create a standalone specification document that distills the plan into a clear outcome specification.

**Target File**: `D:\UnrealProjects\5.6\KatanaCombat\docs\specs\PAIRED_ANIMATION_SPEC.md`

### Spec File Content

```markdown
# Paired Animation System - Technical Specification

> **Version**: 1.0 | **Date**: 2026-01-30 | **Status**: Implementation In Progress
> **Reference Plan**: `.claude/plans/synthetic-painting-ritchie.md`

---

## 1. SYSTEM OVERVIEW

### 1.1 Purpose
A production-quality paired animation system enabling synchronized two-character combat sequences including:
- **Finishers**: Cinematic kill animations triggered by low health, guard break, or stun
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

    // Audio (Scaffolded)
    USoundBase* ImpactSound;
    USoundBase* VictimReactionSound;
    USoundBase* AttackerVoiceLine;
    float MusicDuckingDB;

    // VFX (Scaffolded)
    UNiagaraSystem* ImpactVFX;
    UMaterialInterface* SlowMoPostProcessMaterial;
    UMaterialInterface* ScreenBloodMaterial;
    bool bSpawnBloodDecals;
};
```

### 2.2 Math Foundation Types (CombatMathTypes.h)

| Type | Purpose |
|------|---------|
| FSkeletalHierarchy | Complete bone tree with parent-child relationships |
| FBoneChain | Ordered bone sequence from root to tip with total length |
| FReachQueryResult | Reachability analysis (distance, extension ratio) |
| FJointConstraint | Anatomical rotation limits per joint |
| FContactPointPrediction | Predicted contact location with confidence score |
| FBoneFrameTransform | Bone transform at specific animation frame |

### 2.3 Math Foundation Enums (CombatMathEnums.h)

| Enum | Values |
|------|--------|
| EDistanceFormula | Euclidean, Euclidean2D, Manhattan, Chebyshev, SquaredEuclidean |
| ESpatialQueryShape | Sphere, Box, Capsule, Cone, ConvexHull |
| EBoneChainType | Spine, LeftArm, RightArm, LeftLeg, RightLeg, Neck, etc. |
| EAnatomicalRegion | Head, Neck, Chest, Abdomen, UpperArm, LowerArm, Hand, etc. |
| EContactType | WeaponToBody, HandToBody, FootToBody, WeaponToWeapon, etc. |
| EIKSolverType | TwoBone, FABRIK, CCD, FullBody |

---

## 3. RUNTIME FLOW

### 3.1 Finisher Execution Flow

```
Player Input (Attack during finisher opportunity)
    │
    ▼
CombatComponent::TryExecuteFinisher()
    ├─ Validate: Target in range (SoftAimRange)
    ├─ Validate: Target vulnerable (IsVulnerableToFinisher)
    ├─ Validate: Path clear (ValidatePairedAnimation)
    ├─ Set: bBlockCombatInput = true
    ├─ Set: bIsFinisherTarget = true (mutex)
    ├─ Store: CurrentFinisherVictim reference
    │
    ▼
Setup Warp Tracking (TargetingComponent)
    ├─ SetupAttackerPairedWarp() → Continuous tracking toward victim
    ├─ SetupVictimWarp() → Continuous tracking relative to attacker
    └─ AddPairedPartner() → Enable collision ignore
    │
    ▼
Play Montages (Both Characters)
    ├─ Attacker: AttackerMontage at configured section
    └─ Victim: VictimMontage with timing offset
    │
    ▼
AnimNotifyState_PairedAnimationSync (Sync Point Reached)
    ├─ Validate alignment (warn if > MaxContactDistance)
    ├─ Apply optional nudge correction
    ├─ Trigger hitstop (FreezeActors)
    ├─ Trigger slow motion (ApplySlowMotion)
    ├─ Trigger camera shake (TriggerCameraShake)
    └─ Broadcast OnPairedAnimationSyncPoint delegate
    │
    ▼
OnMontageEnded → CompletePairedAnimation()
    ├─ Set: bDeathHandledByPairedAnimation = true (victim)
    ├─ Calculate: damage = Max(BaseDamage * Multiplier, currentHealth + 1)
    ├─ Apply: ApplyDamage() → triggers Die() → PlayDeathReaction()
    │         └─ PlayDeathReaction checks flag → skips AM_Deaths
    ├─ Clear: warp tracking, partner registration, flags
    └─ Restore: input, time dilation, collision
```

### 3.2 Vulnerability Triggers (Priority Order)

1. **Guard Broken** (Highest) - Posture depleted
2. **Stunned** - Heavy attack hitstun active
3. **Low Health** (Lowest) - Below 25% threshold (configurable)

---

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
// Paired Animation State (synced from CombatComponent)
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

### 6.2 New Libraries (Create)

| Library | Purpose |
|---------|---------|
| SkeletalAnalysisLibrary | BuildSkeletalHierarchy, GetBoneChain, CalculateReachEnvelope |
| GeometryMathLibrary | Distance formulas, bounding volumes, convex hulls |
| SpatialQueryLibrary | Sphere/box/cone queries, FOV checks |
| PhysicsIntegrationLibrary | Trajectory prediction, collision prediction |

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
╚═══════════════════════════════════════╝
```

---

## 8. VERIFICATION CRITERIA

### 8.1 Functional Requirements

| Requirement | Test Criteria |
|-------------|---------------|
| Finisher triggers | Below 25% health, guard broken, or stunned |
| Victim dies correctly | No double death animation, direct ragdoll/freeze |
| Input blocking | Player cannot attack/evade during finisher |
| Warp tracking | Both characters track each other continuously |
| Sync point | Distance < 150u at impact, auto-nudge if < 50u |
| Time dilation | Slow-mo applies and restores without stacking |
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
| CombatComponent.cpp | TryExecuteFinisher, CompletePairedAnimation |
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
| CombatMathEnums.h | Distance formulas, bone chains, contact types |
| CombatMathTypes.h | Skeletal hierarchy, reach results, contact predictions |

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
| 5b | Core finisher flow, effects, safety | ~95% Complete |
| 5c | Math & physics utility libraries | ⏳ Pending |
| 5d | Editor tools & analysis dashboard | ⏳ Pending |
| 5e | AnimInstance procedural integration | ⏳ Pending |
| 5f | Awareness subsystem (runtime healing) | 🔮 Deferred |

---

**Document Maintained By**: Claude Code
**Last Updated**: 2026-01-30
```

---

## EXECUTION PLAN: Comprehensive CLAUDE.md Update

**Purpose**: Update CLAUDE.md with Claude CLI best practices and ensure proper documentation hierarchy.

**Target File**: `D:\UnrealProjects\5.6\KatanaCombat\CLAUDE.md`

### Changes Required

#### 1. Add Documentation Hierarchy Section

After the existing Documentation section, add a clear hierarchy:

```markdown
### Documentation Hierarchy

**Level 1 - CLAUDE.md (Working Memory)**
Essential rules, patterns, and quick references that must be in context for every interaction.

**Level 2 - Specification Files (`docs/specs/`)**
Detailed technical specifications for major systems. Read when working on that system.

**Level 3 - Architecture Docs (`docs/`)**
Deep dives into component design. Read when understanding or modifying architecture.

**Level 4 - Implementation Plans (`.claude/plans/`)**
Active development plans with gap tracking. Read when continuing phased work.

| Need | Start Here |
|------|------------|
| Quick combat system rules | CLAUDE.md (this file) |
| Paired animation spec | `docs/specs/PAIRED_ANIMATION_SPEC.md` |
| Component architecture | `docs/ARCHITECTURE.md` |
| API details | `docs/API_REFERENCE.md` |
| Active plan status | `.claude/plans/synthetic-painting-ritchie.md` |
| Troubleshooting | `docs/TROUBLESHOOTING.md` |
```

#### 2. Add Best Practices Section

```markdown
## Claude CLI Best Practices

### Session Continuity
- Plan files persist across sessions - check `.claude/plans/` for active work
- CLAUDE.md provides working memory context loaded automatically
- Use `docs/specs/` for detailed specs too large for CLAUDE.md

### Exploration Before Implementation
- ALWAYS use Explore agents before modifying unfamiliar code
- Verify actual API signatures - don't assume method names or parameters
- Check existing patterns in similar components

### Code Quality Standards
- Thorough solutions over quick fixes (even at time expense)
- Event-driven over tick-based where possible
- Blueprint exposure only for intentional public API
- Null checks on all weak references and component accesses

### Documentation Updates
- Update CLAUDE.md when design decisions change
- Update specs when implementation deviates from spec
- Archive completed plans to `docs/plans/archive/`
```

#### 3. Update File Structure Section

Add the new Math/ and specs/ folders to the file structure:

```markdown
├── Math/
│   ├── CombatMathEnums.h      ← Distance formulas, bone chains, contact types
│   └── CombatMathTypes.h      ← Skeletal hierarchy, reach, contact predictions
```

#### 4. Add Spec File References

In the Documentation table, add:

```markdown
| Paired animation spec | `docs/specs/PAIRED_ANIMATION_SPEC.md` |
```

### Implementation Steps

1. Create `docs/specs/` directory
2. Write `docs/specs/PAIRED_ANIMATION_SPEC.md` with content above
3. Update CLAUDE.md Documentation section with hierarchy
4. Update CLAUDE.md File Structure with new folders
5. Add Claude CLI Best Practices section
6. Verify all cross-references are correct

### Verification

1. Read CLAUDE.md after updates
2. Verify all referenced files exist
3. Check documentation hierarchy is clear
4. Ensure spec file is comprehensive but scannable

---

## Phase 5d: Paired Animation Preview Tool Enhancements (NEW - HIGH PRIORITY)

> **Added**: 2026-01-31 | **Status**: Planning
> **Priority**: HIGH - This tool enables efficient iteration on all paired animation work

The SPairedAnimationPreview editor tool needs significant enhancements to properly visualize the holistic analysis and provide actionable insights for paired animation authoring.

### Current Issues Identified

#### 1. Optimization Inconsistency Bug (P0)
**Problem**: When pressing "Run Full Optimization" while animation is playing, the optimal distance/rotation values change at different frames. This contradicts the purpose of holistic optimization which should find the SINGLE BEST configuration across all weighted frames.

**Root Cause**: The optimization may be using current frame state instead of the cached holistic analysis.

**Solution**:
- Ensure optimization functions use EvaluateConfigurationHolistic() exclusively
- Cache the holistic analysis result and use it consistently
- Disable frame-dependent calculations during optimization

#### 2. Spatial Relationship Context Problem (P0)
**Problem**: The optimizer has no concept of intended spatial relationship between characters. It cannot distinguish between:
- **Facing**: Characters meant to face each other (front attacks)
- **Behind**: Attacker meant to be behind victim (backstabs)
- **Side**: Attacker approaching from left/right
- **Custom**: Any arbitrary relationship

**Analysis - Is Intent Inferable from Animation Data?**

Potentially inferable signals:
1. **Sync Point Contact Analysis**: At impact moment, analyze where attackers contact bones are relative to victims body
   - Contact on victims spine_03, spine_02 -> attacker behind
   - Contact on victims chest, clavicle -> attacker in front
   - Contact on victims left side bones -> attacker on left

2. **Victim Bone Facing at Sync Point**: Check which direction victims pelvis/spine faces relative to attacker
   - If victims forward faces attacker -> facing relationship
   - If victims back faces attacker -> behind relationship

3. **Root Motion Direction Analysis**: Attackers root motion direction relative to victim starting position
   - Forward motion toward victim -> likely facing
   - Lateral motion -> likely side approach

**Recommended Solution - Hybrid Approach**:
1. **Infer Suggested Relationship** from animation data (sync point contact location + victim facing)
2. **Display as Dropdown** with inferred value pre-selected
3. **Allow Explicit Override** (Facing, Behind, Left Side, Right Side, Custom)
4. **Constrain Optimization Search Space** based on relationship:
   - Facing: Victim rotation ~180 deg from attacker (+-30 deg)
   - Behind: Victim rotation ~0 deg from attacker (+-30 deg)
   - Side: Victim rotation ~90 deg from attacker (+-30 deg)

#### 3. Lack of Trajectory Visualization (P1)
**Problem**: The tool samples the full animation but does not visually communicate this. Users cannot see how bones will move over time without scrubbing through manually.

**Solution - Bone Chain Visualization**:
- Visualize predefined bone chains as connected trajectory ribbons
- Chains: Left Arm, Right Arm, Left Leg, Right Leg, Spine
- Show movement from t=0 to t=end with color gradient (start=blue, end=red)
- Less configuration overhead than individual bones

#### 4. No Joint Constraint Visualization (P2)
**Problem**: No way to see if animations cause over-extension or over-compression of joints, which is critical for procedural IK planning.

**Solution**:
- Visualize joint angles at key frames
- Highlight joints exceeding anatomical limits (red warning)
- Show pole vector positions for key joints (elbows, knees)
- This data feeds into future procedural IK system

---

### Phase 5d-1: Fix Optimization Consistency

**Goal**: Ensure Run Full Optimization produces stable, frame-independent results

| Task | Description | Priority |
|------|-------------|----------|
| Cache holistic result | Store FHolisticTimelineAnalysis and reuse during optimization | P0 |
| Decouple from current frame | Optimization should not read CurrentTime | P0 |
| Lock during optimization | Disable timeline scrubbing while optimization runs | P1 |
| Show optimization scope | Display Analyzing X frames with Y contact points | P2 |

### Phase 5d-2: Spatial Relationship System

**Goal**: Infer and configure intended character spatial relationships

| Task | Description | Priority |
|------|-------------|----------|
| Infer relationship from sync point | Analyze contact bone location on victim at sync time | P0 |
| Add relationship dropdown | ESpatialRelationship enum (Facing, Behind, LeftSide, RightSide, Custom) | P0 |
| Constrain rotation optimization | Limit search space based on relationship type | P0 |
| Show inferred vs configured | Visual indicator when user overrides inference | P1 |
| Relationship confidence score | Display confidence in inference (0-100%) | P2 |

**ESpatialRelationship Enum**:
- Inferred (Auto-Detect)
- Facing (Facing Each Other)
- Behind (Attacker Behind)
- LeftSide (Attacker on Left)
- RightSide (Attacker on Right)
- Custom (No Constraints)

**Rotation Constraints by Relationship**:
| Relationship | Victim Rotation (relative to attacker forward) | Tolerance |
|--------------|-----------------------------------------------|-----------|
| Facing | 180 deg (facing attacker) | +-30 deg |
| Behind | 0 deg (back to attacker) | +-30 deg |
| LeftSide | 90 deg (left side to attacker) | +-30 deg |
| RightSide | -90 deg (right side to attacker) | +-30 deg |
| Custom | Any | N/A |

### Phase 5d-3: Bone Chain Trajectory Visualization

**Goal**: Visualize how bone chains move through the full animation timeline

| Task | Description | Priority |
|------|-------------|----------|
| Define standard bone chains | Left/Right Arm, Left/Right Leg, Spine, Neck | P0 |
| Sample chain trajectories | Extract positions for all chain bones at all sample times | P0 |
| Draw trajectory ribbons | Connect bone positions over time as ribbons/lines | P0 |
| Color gradient over time | Blue (t=0) to Red (t=end) for temporal understanding | P0 |
| Chain selection UI | Checkboxes for each chain (global enable + per-chain) | P1 |
| Per-character toggle | Separate enable for attacker vs victim chains | P1 |
| Transparency/fade options | Adjust visibility to not obscure mesh | P2 |

**Bone Chain Definitions**:
- LeftArm: clavicle_l -> upperarm_l -> lowerarm_l -> hand_l
- RightArm: clavicle_r -> upperarm_r -> lowerarm_r -> hand_r
- LeftLeg: thigh_l -> calf_l -> foot_l -> ball_l
- RightLeg: thigh_r -> calf_r -> foot_r -> ball_r
- Spine: pelvis -> spine_01 -> spine_02 -> spine_03 -> neck_01 -> head

### Phase 5d-4: Individual Bone Trajectory Visualization

**Goal**: Allow fine-grained bone selection for detailed analysis

| Task | Description | Priority |
|------|-------------|----------|
| Skeletal hierarchy dropdown | Expandable tree view of skeleton for each character | P1 |
| Bone checkbox selection | Check individual bones for trajectory display | P1 |
| Bone trajectory rendering | Same ribbon/gradient style as chains | P1 |
| Bone velocity display | Show speed at cursor position along trajectory | P2 |
| Bone distance to partner | Show closest approach distance to partner bones | P2 |

### Phase 5d-5: Joint Constraint Visualization

**Goal**: Show joint angle limits and detect over-extension/compression

| Task | Description | Priority |
|------|-------------|----------|
| Define joint limits | Anatomical rotation limits for key joints | P1 |
| Calculate joint angles | Extract current angle for elbow, knee, shoulder, etc | P1 |
| Limit violation detection | Flag when angle exceeds limit (red highlight) | P1 |
| Pole vector display | Show IK pole positions for two-bone chains | P2 |
| Joint stress heatmap | Color joints by how close to limits (green to yellow to red) | P2 |
| Export constraint data | Generate data for procedural IK correction | P2 |

**Key Joints to Monitor**:
| Joint | Flexion Min | Flexion Max | Notes |
|-------|-------------|-------------|-------|
| Elbow | 0 deg | 145 deg | Over-extension = less than 0, Over-compression = more than 145 |
| Knee | 0 deg | 140 deg | Similar to elbow |
| Shoulder | -60 deg | 180 deg | Complex, simplified to single axis |
| Wrist | -70 deg | 70 deg | Flexion/extension |
| Neck | -40 deg | 40 deg | Lateral bend limits |

---

### Implementation Files

| File | Purpose | Changes |
|------|---------|---------|
| PairedAnimationPreview.h | Header | Add ESpatialRelationship, chain visualization state, joint analysis structs |
| PairedAnimationPreview.cpp | Implementation | Add relationship inference, chain visualization, joint analysis |
| MontageAnalysisTypes.h | Types | Add FBoneChainTrajectory, FJointConstraintAnalysis |

---

### Preview Tool Gap Summary

| Gap ID | Description | Priority | Status |
|--------|-------------|----------|--------|
| PT-1 | Optimization changes with frame (should be holistic) | P0 | Pending |
| PT-2 | No spatial relationship context for optimization | P0 | Pending |
| PT-3 | No visualization of full trajectory over time | P1 | Pending |
| PT-4 | No bone chain visualization | P1 | Pending |
| PT-5 | No individual bone trajectory selection | P1 | Pending |
| PT-6 | No joint constraint analysis | P2 | Pending |
| PT-7 | No pole vector visualization | P2 | Pending |
