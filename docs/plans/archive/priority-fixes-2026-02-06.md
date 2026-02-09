# Combat System Priority Fixes

## CURRENT STATUS (2026-02-06)

| Phase | Task | Status |
|-------|------|--------|
| **BUG-1** | Movement disabled after finisher | ✅ DONE |
| **BUG-2** | Procedural blend system | ✅ DONE - "massive difference", "buttery smooth" |
| **BUG-3** | Combo chain fix | ✅ DONE |
| **BUG-4** | Input pairing during blends | 🔴 Deferred |
| **CAM-1** | Camera collision with enemies | 🔴 **CRITICAL - NEW** |
| **HIT-1** | Hit detection robustness | 🔴 **CRITICAL - NEW** |
| Counter System | AC3/Arkham counter-kills | 🔴 Deferred until CAM-1/HIT-1 resolved |

---

## CRITICAL PRIORITY 0: Input/Combo Resolution (INPUT-1)

### Problem Statement
When spamming light attack rapidly, the combo system flip-flops:
- Light 1 plays partially → blends to Light 2
- Light 2 plays partially → starts Light 3
- Then jumps BACK to Light 1 instead of completing the chain

This is NOT a data asset configuration issue.

### Suspected Root Causes

**1. Combo Resolution Race Condition**
The procedural blend system or rapid input handling may be:
- Processing new input before previous attack state is fully committed
- Using stale `CurrentAttackData` when resolving next attack
- Blending out before combo chain state is properly updated

**2. Attack State Machine Desync**
The `FAttackStateMachine` may have:
- Generation counter not incrementing properly
- Section tracking getting confused with rapid transitions
- Grace period (150ms) being bypassed during blend

**3. Montage Callback Timing**
Multiple `OnMontageEnded` or `OnMontageBlendingOut` callbacks may fire:
- During blend transition, stale callbacks processed
- `ShouldProcessMontageEnd()` filter not catching all cases

### ROOT CAUSE IDENTIFIED ✅

**The Race Condition** (16-50ms window):

```
T+0ms:   PlayAttackMontage() starts
T+1ms:   StopAllMontages(0.0f) called (line 1458)
T+5ms:   OnMontageEnded fires IMMEDIATELY (engine callback)
T+10ms:  SetPhase(None) clears CurrentAttackData = nullptr (line 2553)
T+15ms:  PlayAttackMontage returns
T+20ms:  ExecuteAction sets CurrentAttackData = new attack (line 994) - TOO LATE!
T+25ms:  Next input arrives: bComboWindowActive=true, CurrentAttackData=nullptr
T+30ms:  GetAttackForInput hits BUG-3 FIX (line 3387): bShouldCombo = false
         → Returns default attack 1 instead of continuing combo!
```

**Critical Code Locations**:
| Issue | File | Line |
|-------|------|------|
| StopAllMontages triggers callback | CombatComponent.cpp | 1458 |
| CurrentAttackData set TOO LATE | CombatComponent.cpp | 994 |
| SetPhase(None) clears data | CombatComponent.cpp | 2553 |
| BUG-3 FIX forces reset | CombatComponent.cpp | 3387-3395 |

**Fix Strategy**: Set `CurrentAttackData` BEFORE calling `PlayAttackMontage()`, not after.

```cpp
// BEFORE (ExecuteAction line 983-994):
if (PlayAttackMontage(Action.AttackData))
{
    SetPhase(EAttackPhase::Windup);
    DiscoverCheckpoints();
    CurrentAttackData = Action.AttackData;  // TOO LATE!
}

// AFTER (Fix):
CurrentAttackData = Action.AttackData;  // Set FIRST
if (PlayAttackMontage(Action.AttackData))
{
    SetPhase(EAttackPhase::Windup);
    DiscoverCheckpoints();
}
else
{
    CurrentAttackData = nullptr;  // Revert on failure
}
```

### Test-Driven Development Approach

**Write tests FIRST, then fix**. This is automatable:

### Required Tests (TDD - Write FIRST)

**File**: `Source/KatanaCombatTest/Private/InputResolutionTests.cpp` (NEW)

| Test | Description | Expected | Automation |
|------|-------------|----------|------------|
| INPUT-1a | Spam light 20x from idle | Smooth 1→2→3→4→5→6→1→2... cycle | ✅ Automatable |
| INPUT-1b | Rapid input at <50ms intervals | Every input respected in order | ✅ Automatable |
| INPUT-1c | Full attack plays to Recovery | Each attack reaches Recovery before next blend | ✅ Automatable |
| INPUT-1d | No blend during Windup/Active | Blend only in Recovery or ComboWindow | ✅ Automatable |
| INPUT-1e | CurrentAttackData set before montage | Never null during combo window | ✅ Automatable |
| INPUT-1f | StopAllMontages doesn't clear combo state | CurrentAttackData persists through callback | ✅ Automatable |
| INPUT-1g | 100 rapid inputs stress test | No combo resets, consistent progression | ✅ Automatable |
| INPUT-1h | Alternating Light/Heavy spam | Correct attack type resolution each time | ✅ Automatable |

**Test Template**:
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputResolution_RapidSpamMaintainsCombo,
    "KatanaCombat.Input.RapidSpamMaintainsCombo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputResolution_RapidSpamMaintainsCombo::RunTest(const FString& Parameters)
{
    // Setup: Create character with combat component
    // Input: Simulate 20 rapid light attack inputs at 30ms intervals
    // Verify: Attack progression is 1→2→3→4→5→6→1→2→3... (never 1→2→1)
    // Verify: CurrentAttackData is NEVER null when bComboWindowActive=true
    return true;
}
```

### Design Goals

**Bulletproof Input System Requirements**:
1. **Every input queued**: No input ever lost, regardless of timing
2. **Attack completes to Recovery**: Cannot blend out of Active phase
3. **Combo chain always progresses**: 1→2→3→... never 1→2→1
4. **State consistency**: CurrentAttackData always matches playing montage
5. **Predictable timing**: Same input sequence = same animation result

---

## CRITICAL PRIORITY 1: Camera Collision (CAM-1)

### Problem Statement
Camera is pushed inside the player character when moving between enemies. Spring arm collision detection treats enemy pawns as obstacles, causing jarring camera behavior.

**NOTE**: Issue is **worst during root motion** - the character's rapid position changes during attack animations cause camera spring arm to collide with nearby enemies more aggressively.

### Root Cause Analysis

**Current Implementation** (from exploration):
- Uses `USpringArmComponent` with default collision settings
- NO explicit `bDoCollisionTest` configuration
- NO ignored actors list defined
- NO custom collision channel for camera
- Spring arm auto-retracts when ANY pawn blocks line of sight

**Files Involved**:
| File | Lines | Purpose |
|------|-------|---------|
| `KatanaCombatCharacter.cpp` | 38-47 | Base character camera boom setup |
| `CombatCharacter.cpp` | 32-44 | Combat variant camera boom |
| `DefaultEngine.ini` | 79-135 | Collision profiles (no camera-specific) |

### Proposed Fixes

**Option A: Ignore Enemy Pawns (Recommended)**
```cpp
// In character constructor or BeginPlay
CameraBoom->bDoCollisionTest = true;  // Explicit enable
CameraBoom->ProbeChannel = ECC_Camera;  // Use camera channel
CameraBoom->ProbeSize = 12.0f;  // Standard probe size

// Add enemies to ignore list dynamically
void ABaseCombatCharacter::OnEnemyEnteredView(AActor* Enemy)
{
    if (CameraBoom)
    {
        CameraBoom->AddIgnoredActor(Enemy);
    }
}
```

**Option B: Custom Collision Channel**
```ini
; DefaultEngine.ini
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel2,DefaultResponse=ECR_Block,bTraceType=False,bStaticObject=False,Name="CameraProbe")
```
Configure enemy pawns to ignore this channel.

**Option C: Soft Collision Camera (Most Complex)**
Implement custom camera collision that:
- Detects obstacles but doesn't hard-clip
- Uses smooth blend to avoid penetration
- Maintains minimum distance from player

### Implementation Steps

1. Add `CameraBoom->ProbeChannel = ECC_Camera` to character constructors
2. Create helper to add/remove ignored actors for camera
3. In enemy spawn logic, add enemy to player's camera ignore list
4. In enemy death/despawn, remove from ignore list
5. Test with multiple enemies surrounding player

### Verification
- Walk between 3+ enemies - camera maintains distance
- Finisher execution - camera doesn't clip through victim
- Fast camera rotation near enemies - no jerking

---

## CRITICAL PRIORITY 2: Hit Detection Robustness (HIT-1)

### Problem Statement
Hits are being dropped. The current swept trace system is inadequate for:
1. Fast-moving targets
2. Providing detailed hit analytics
3. Predicting hit outcomes
4. Broadcasting rich hit event data

### Root Cause Analysis (from exploration)

**Current Implementation**:
- **Trace Method**: `SweepMultiByChannel()` with capsule shape
- **Trace Frequency**: Every tick when hit detection enabled
- **Shape**: Capsule aligned weapon start→end sockets
- **Channel**: `ECC_Pawn`
- **Filtering**: Already-hit actors, dead/dying, I-frames

**Where Hits Get Dropped**:
| Location | Issue | Severity |
|----------|-------|----------|
| `WeaponComponent.cpp:182-186` | Socket not found → falls back to character center | **HIGH** |
| `WeaponComponent.cpp:298` | Already-hit filtering (silent skip) | Low (by design) |
| `WeaponComponent.cpp:304-309` | Dead/dying filter (no logging) | Medium |
| `BaseCombatCharacter.cpp:476` | Null AttackData → early return (no warning) | **HIGH** |
| `WeaponComponent.cpp:216-222` | First frame intentionally skipped | Medium |

**Missing Hit Information**:
| Field | Current State | Need |
|-------|---------------|------|
| Animation frame/time | NOT captured | Required for VFX sync |
| Weapon velocity | NOT captured | Required for knockback direction |
| Contact surface type | Scaffolded (ECombatSurfaceType) | Required for surface FX |
| Attack phase | NOT captured | Required for damage scaling |
| Distance to target | NOT captured | Required for range-based effects |
| bWasCounter | Always false | Required for counter damage |

### Proposed Fixes

**Phase 1: Logging & Diagnostics**
Add debug logging to all silent failure points:
```cpp
// WeaponComponent.cpp - After socket fallback
if (!bStartSocketFound || !bEndSocketFound)
{
    UE_LOG(LogCombat, Warning, TEXT("[HIT] Socket fallback: Start=%s End=%s"),
        bStartSocketFound ? TEXT("OK") : TEXT("FALLBACK"),
        bEndSocketFound ? TEXT("OK") : TEXT("FALLBACK"));
}
```

**Phase 2: Enhanced FHitReactionInfo**
```cpp
// Add to FHitReactionInfo in CombatTypes.h
UPROPERTY(BlueprintReadOnly)
float AnimationTime = 0.0f;  // Position in attack montage

UPROPERTY(BlueprintReadOnly)
FVector WeaponVelocity = FVector::ZeroVector;  // For knockback

UPROPERTY(BlueprintReadOnly)
ECombatSurfaceType SurfaceType = ECombatSurfaceType::Default;

UPROPERTY(BlueprintReadOnly)
EAttackPhase PhaseWhenHit = EAttackPhase::None;

UPROPERTY(BlueprintReadOnly)
float DistanceToTarget = 0.0f;
```

**Phase 3: Trajectory-Based Detection**
Integrate `PhysicsIntegrationLibrary` (already exists, 15 functions):
- `PredictTrajectory()` for fast-moving targets
- `VerletIntegrate()` for physics prediction
- Sub-frame interpolation for high-speed weapons

**Phase 4: Robust Socket Handling**
```cpp
// WeaponComponent.cpp - Before trace
if (!GetSocketLocation(WeaponStartSocket, StartLocation))
{
    // LOG the error, don't silently fall back
    UE_LOG(LogCombat, Error, TEXT("[HIT] Critical: Start socket '%s' not found!"), *WeaponStartSocket.ToString());
    return;  // Fail loudly, not silently
}
```

### Implementation Priority

| Task | Priority | Complexity |
|------|----------|------------|
| Add logging to all failure points | P0 | Low |
| Fix socket fallback to fail loudly | P0 | Low |
| Populate bWasCounter field | P1 | Low |
| Add AnimationTime to FHitReactionInfo | P1 | Medium |
| Enable bReturnPhysicalMaterial for surface FX | P2 | Low |
| Add WeaponVelocity calculation | P2 | Medium |
| Integrate trajectory prediction | P3 | High |

### Files to Modify

| File | Changes |
|------|---------|
| `WeaponComponent.cpp` | Socket validation, logging, velocity capture |
| `CombatTypes.h` | Enhanced FHitReactionInfo fields |
| `BaseCombatCharacter.cpp` | Populate new hit info fields |
| `WeaponComponent.h` | Store previous frame weapon positions for velocity |

---

## VERIFICATION PLAN

### CAM-1 Verification
1. **Basic Test**: Walk player between 3+ enemies - camera should NOT push into player
2. **Rotation Test**: Fast camera rotation near enemies - no jerking or snapping
3. **Finisher Test**: During finisher execution - camera doesn't clip through victim mesh
4. **Edge Case**: Enemy dies while blocking camera - smooth transition, no pop

### HIT-1 Verification
1. **Logging Test**: Enable debug draw, verify all hit detection events are logged
2. **Socket Test**: Remove weapon mesh → verify loud error instead of silent fallback
3. **Fast Target**: High-speed enemy movement → verify no dropped hits
4. **Hit Info**: On hit, verify FHitReactionInfo contains all new fields populated
5. **Counter Flag**: During counter attack → verify bWasCounter = true

### Post-Implementation
- All 345 existing tests should still pass
- New tests for camera ignore list management
- New tests for enhanced hit info population
- Manual combat testing: 10 minutes of uninterrupted combat with no camera issues or dropped hits

---

## GAP TRACKER UPDATES NEEDED

**Note**: These gaps need to be added to `docs/plans/gap-tracker.md` after exiting plan mode:

### New Section 23: Camera & Collision (4 gaps)

| ID | Description | Priority | Status |
|----|----|----------|--------|
| 23.1 | Camera pushed inside player when between enemies | P1 | Pending |
| 23.2 | Spring arm ignores enemy pawns (no exclusion list) | P1 | Pending |
| 23.3 | Camera collision during finisher cinematics | P2 | Pending |
| 23.4 | No smooth blend for collision avoidance | P3 | Pending |

### New Section 24: Hit Detection Robustness (7 gaps)

| ID | Description | Priority | Status |
|----|----|----------|--------|
| 24.1 | Hits dropped on fast-moving targets | P1 | Pending |
| 24.2 | Silent socket fallback causes misaligned traces | P1 | Pending |
| 24.3 | FHitReactionInfo missing animation context | P1 | Pending |
| 24.4 | bWasCounter always false (never populated) | P1 | Pending |
| 24.5 | No weapon velocity in hit info | P2 | Pending |
| 24.6 | Surface type detection not wired | P2 | Pending |
| 24.7 | No trajectory prediction for high-speed weapons | P3 | Pending |

---

## COMMIT PLAN

Before starting CAM-1 and HIT-1 implementation:

```bash
# Commit BUG-2 procedural blend implementation
git add Source/KatanaCombat/Public/Core/CombatComponent.h
git add Source/KatanaCombat/Private/Core/CombatComponent.cpp
git commit -m "Implement procedural blend system (BUG-2)

- Add FProceduralBlendConfig to CombatComponent
- Wire UProceduralAnimationLibrary::CalculateProceduralBlend() into PlayAttackMontage()
- Blend time now calculated from animation progress (0% = slow, 100% = fast)
- Debug logging shows progress %, blend times, and strategy
- Replaces preset ComboBlendInTime/ComboBlendOutTime (kept but ignored)

User feedback: 'buttery smooth' animation transitions"
```

---

## DEFERRED: Previous Bug Fixes

Before implementing the counter system, these bugs must be addressed:

### Bug 1: Movement Disabled After Finisher

**Symptom**: After executing a finisher, player cannot move but can still attack.

**Root Cause Analysis**:
- `bMovementCurrentlyDisabled` flag at `CombatComponent.h:906`
- `UpdateMovementFromMontageState()` is the ONLY place controlling movement (`CombatComponent.cpp:2802-2873`)
- **The bug**: `CompletePairedAnimation()` → `ClearHoldState()` → `UpdateMovementFromMontageState()`
- BUT finishers don't use hold mechanics, so `HoldState.IsHolding() = false` and `HoldState.bIsEasing = false`
- The logic at lines 2824-2848 checks these flags to determine `bShouldLockMovement`
- If neither is true, `bShouldLockMovement = false`, BUT `bMovementCurrentlyDisabled` may still be `true` from somewhere else

**Files**:
- `CombatComponent.h:906` - `bMovementCurrentlyDisabled` flag
- `CombatComponent.cpp:2802-2873` - `UpdateMovementFromMontageState()`
- `CombatComponent.cpp:2854-2855` - `DisableMovement()` and flag set
- `CombatComponent.cpp:4333-4336` - `CompletePairedAnimation()` cleanup

**Fix**: Add explicit `bMovementCurrentlyDisabled = false` and `SetMovementMode(MOVE_Walking)` in `EndPairedAnimation()` or `CompletePairedAnimation()` before calling `SetPhase(None)`.

### Bug 2: Blend Configs Carrying Over Between Animations

**Symptom**: Blend settings from one attack affect subsequent animations unexpectedly.

**Root Cause Analysis**:
- `ComboBlendInTime` and `ComboBlendOutTime` configured per-attack in AttackData
- `FAlphaBlendArgs` used at `CombatComponent.cpp:1504`
- Critical playrate restoration fix at lines 2902-2917 addresses one symptom
- Line 2903-2904 comment: "Without this, new montage starts with wrong playrate (e.g., 0.75) causing 'partial blend' visual issues"
- **The bug**: Blend state may persist across attacks via AnimInstance montage blend weights

**Files**:
- `AttackData.h` - `ComboBlendInTime`, `ComboBlendOutTime` properties
- `CombatComponent.cpp:1416-1528` - Blend logic in `PlayAttackMontage()`
- `CombatComponent.cpp:2902-2917` - Playrate restoration fix

**Potential Fix**: Force-stop all montages with instant blend before starting new attack during rapid input, or implement procedural blend system.

### Bug 3: Input Down/Release Pairing Issues During Blends

**Symptom**: Inputs missed or duplicated during blend transitions.

**Root Cause Analysis**:
- `ProcessInputPair()` at `CombatComponent.cpp:962` handles press/release matching
- `FAttackStateMachine` tracks `ComboBlendEndTime` for callback filtering
- Line 1423-1444: "CRITICAL FIX: Detect if we're still in a blend transition (rapid input during blend)"
- **The bug**: If inputs arrive during blend window, they may be processed with stale attack context

**Files**:
- `CombatComponent.h:962` - `ProcessInputPair()`
- `CombatComponent.cpp:1423-1444` - Rapid input during blend fix
- `CombatTypes.h:1332` - `ComboBlendEndTime` in FAttackStateMachine

**Potential Fix**: Create procedural blend system that calculates blend dynamically based on animation positions.

### Proposed Solution: Procedural Blend System (APPROVED)

**User Decision**: Replace preset blend configs with procedural system following established architecture patterns.

---

## DETAILED PROCEDURAL BLEND IMPLEMENTATION

### Architecture Overview (Three-Layer Pattern)

| Layer | File | Responsibility |
|-------|------|----------------|
| **Types** | `Data/ProceduralAnimationTypes.h` (NEW) | Enums, configs, result structs for all procedural animation |
| **Library** | `Utilities/ProceduralAnimationLibrary.h/.cpp` (NEW) | Stateless calculation functions |
| **Component** | `CombatComponent.h/.cpp` | Routes data, owns state, calls library |

---

### STEP 1: Types Layer ✅ COMPLETE

**Location**: `Source/KatanaCombat/Public/Data/ProceduralAnimationTypes.h`

**Status**: Created and compiled successfully.

This file provides types for ALL procedural animation calculations - blend timing, IK targets, pose matching, etc.

```cpp
// ProceduralAnimationTypes.h
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralAnimationTypes.generated.h"

// ============================================================================
// INTERPOLATION STRATEGIES
// ============================================================================

/**
 * Strategy for calculating procedural values.
 * Different algorithms for different use cases.
 */
UENUM(BlueprintType)
enum class EProceduralStrategy : uint8
{
    /** Linear interpolation (Progress → MinBlend to MaxBlend) */
    Linear                  UMETA(DisplayName = "Linear"),

    /** Quadratic ease-out (fast start, slow end) */
    EaseOut                 UMETA(DisplayName = "Ease Out (Quadratic)"),

    /** Quadratic ease-in (slow start, fast end) */
    EaseIn                  UMETA(DisplayName = "Ease In (Quadratic)"),

    /** Cubic ease-in-out (smooth S-curve) */
    EaseInOut               UMETA(DisplayName = "Ease In-Out (Cubic)"),

    /** Step function (instant at threshold) */
    Step                    UMETA(DisplayName = "Step (Threshold-based)"),

    /** Custom curve (requires UCurveFloat reference) */
    CustomCurve             UMETA(DisplayName = "Custom Curve")
};

/**
 * Mode for blend behavior during rapid input.
 * Controls how system handles mashing during transitions.
 */
UENUM(BlueprintType)
enum class ERapidInputBlendMode : uint8
{
    /** Force instant blend (clear everything, start fresh) */
    ForceInstant            UMETA(DisplayName = "Force Instant"),

    /** Continue current blend (ignore rapid input) */
    ContinueCurrent         UMETA(DisplayName = "Continue Current"),

    /** Queue for later execution */
    QueueUntilComplete      UMETA(DisplayName = "Queue Until Complete"),

    /** Blend faster than normal (accelerate) */
    Accelerate              UMETA(DisplayName = "Accelerate Blend")
};

// ============================================================================
// PROCEDURAL BLEND CONFIGURATION
// ============================================================================

/**
 * Configuration for procedural blend time calculation.
 * Multiple strategies available for different feel/performance trade-offs.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FProceduralBlendConfig
{
    GENERATED_BODY()

    /** Strategy for calculating blend time from animation progress */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Strategy")
    EProceduralStrategy Strategy = EProceduralStrategy::Linear;

    /** How to handle rapid input during blend transitions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Strategy")
    ERapidInputBlendMode RapidInputMode = ERapidInputBlendMode::ForceInstant;

    /** Minimum blend time in seconds (used near animation end) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Timing",
              meta = (ClampMin = "0.01", ClampMax = "1.0", EditCondition = "Strategy != EProceduralStrategy::CustomCurve"))
    float MinBlendTime = 0.05f;

    /** Maximum blend time in seconds (used mid-animation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Timing",
              meta = (ClampMin = "0.01", ClampMax = "1.0", EditCondition = "Strategy != EProceduralStrategy::CustomCurve"))
    float MaxBlendTime = 0.2f;

    /** Progress threshold for instant blend (Step strategy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Timing",
              meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "Strategy == EProceduralStrategy::Step"))
    float InstantBlendThreshold = 0.95f;

    /** Custom blend curve (CustomCurve strategy only) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|Curve",
              meta = (EditCondition = "Strategy == EProceduralStrategy::CustomCurve"))
    TObjectPtr<UCurveFloat> CustomBlendCurve = nullptr;

    /** Multiplier for accelerated blend mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend|RapidInput",
              meta = (ClampMin = "1.0", ClampMax = "5.0", EditCondition = "RapidInputMode == ERapidInputBlendMode::Accelerate"))
    float AccelerationMultiplier = 2.0f;

    /** Default constructor */
    FProceduralBlendConfig()
        : Strategy(EProceduralStrategy::Linear)
        , RapidInputMode(ERapidInputBlendMode::ForceInstant)
        , MinBlendTime(0.05f)
        , MaxBlendTime(0.2f)
        , InstantBlendThreshold(0.95f)
        , CustomBlendCurve(nullptr)
        , AccelerationMultiplier(2.0f)
    {
    }

    /** Create default combat blend config (Linear strategy) */
    static FProceduralBlendConfig CreateDefault()
    {
        return FProceduralBlendConfig();
    }

    /** Create smooth combo config (EaseInOut strategy) */
    static FProceduralBlendConfig CreateSmoothCombo()
    {
        FProceduralBlendConfig Config;
        Config.Strategy = EProceduralStrategy::EaseInOut;
        Config.MinBlendTime = 0.08f;
        Config.MaxBlendTime = 0.25f;
        return Config;
    }

    /** Create snappy config (Step strategy) */
    static FProceduralBlendConfig CreateSnappy()
    {
        FProceduralBlendConfig Config;
        Config.Strategy = EProceduralStrategy::Step;
        Config.MinBlendTime = 0.03f;
        Config.InstantBlendThreshold = 0.8f;
        return Config;
    }
};

/**
 * Result of procedural blend calculation.
 * Rich return type for debugging, analytics, and flexibility.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FProceduralBlendResult
{
    GENERATED_BODY()

    /** Calculated blend-in time for new montage */
    UPROPERTY(BlueprintReadOnly, Category = "Blend")
    float BlendInTime = 0.1f;

    /** Calculated blend-out time for current montage */
    UPROPERTY(BlueprintReadOnly, Category = "Blend")
    float BlendOutTime = 0.1f;

    /** Current animation progress (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Blend")
    float AnimationProgress = 0.0f;

    /** Raw interpolation alpha before clamping (for debug) */
    UPROPERTY(BlueprintReadOnly, Category = "Blend|Debug")
    float RawInterpolationAlpha = 0.0f;

    /** Strategy that was used for calculation */
    UPROPERTY(BlueprintReadOnly, Category = "Blend|Debug")
    EProceduralStrategy UsedStrategy = EProceduralStrategy::Linear;

    /** Should use instant blend (rapid input or near end) */
    UPROPERTY(BlueprintReadOnly, Category = "Blend")
    bool bUseInstantBlend = false;

    /** Was this a fresh attack (no previous montage) */
    UPROPERTY(BlueprintReadOnly, Category = "Blend")
    bool bIsFreshAttack = false;

    /** Was rapid input detected? */
    UPROPERTY(BlueprintReadOnly, Category = "Blend")
    bool bRapidInputDetected = false;

    /** Default constructor */
    FProceduralBlendResult()
        : BlendInTime(0.1f)
        , BlendOutTime(0.1f)
        , AnimationProgress(0.0f)
        , RawInterpolationAlpha(0.0f)
        , UsedStrategy(EProceduralStrategy::Linear)
        , bUseInstantBlend(false)
        , bIsFreshAttack(false)
        , bRapidInputDetected(false)
    {
    }

    /** Is blend valid? */
    bool IsValid() const { return BlendInTime >= 0.0f && BlendOutTime >= 0.0f; }

    /** Get effective blend duration (max of in/out) */
    float GetEffectiveDuration() const { return FMath::Max(BlendInTime, BlendOutTime); }

    /** Get debug string for logging */
    FString ToDebugString() const
    {
        return FString::Printf(TEXT("BlendIn=%.3f, BlendOut=%.3f, Progress=%.1f%%, Strategy=%d, Instant=%s, Fresh=%s, Rapid=%s"),
            BlendInTime, BlendOutTime, AnimationProgress * 100.0f,
            static_cast<int32>(UsedStrategy),
            bUseInstantBlend ? TEXT("Y") : TEXT("N"),
            bIsFreshAttack ? TEXT("Y") : TEXT("N"),
            bRapidInputDetected ? TEXT("Y") : TEXT("N"));
    }
};

// ============================================================================
// PROCEDURAL TIMING TYPES (For future expansion)
// ============================================================================

/**
 * Configuration for procedural hit reaction timing.
 * Can extend procedural system beyond just blend timing.
 */
USTRUCT(BlueprintType)
struct KATANACOMBAT_API FProceduralTimingConfig
{
    GENERATED_BODY()

    /** Strategy for timing calculation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    EProceduralStrategy Strategy = EProceduralStrategy::Linear;

    /** Minimum duration (fast case) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
    float MinDuration = 0.1f;

    /** Maximum duration (slow case) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
    float MaxDuration = 0.5f;

    /** Custom curve for timing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
              meta = (EditCondition = "Strategy == EProceduralStrategy::CustomCurve"))
    TObjectPtr<UCurveFloat> CustomCurve = nullptr;

    FProceduralTimingConfig()
        : Strategy(EProceduralStrategy::Linear)
        , MinDuration(0.1f)
        , MaxDuration(0.5f)
        , CustomCurve(nullptr)
    {
    }
};

// ============================================================================
// FORWARD DECLARATIONS FOR CURVE
// ============================================================================

class UCurveFloat;
```

---

### STEP 2: Library Layer ✅ COMPLETE

**Location**: `Source/KatanaCombat/Public/Utilities/ProceduralAnimationLibrary.h`

**Status**: Created and compiled successfully. All 207 tests pass.

Dedicated library for procedural animation calculations. Separates this from MontageUtilityLibrary for cleaner organization.

```cpp
// ProceduralAnimationLibrary.h
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/ProceduralAnimationTypes.h"
#include "ProceduralAnimationLibrary.generated.h"

/**
 * Procedural Animation Library
 *
 * Stateless utility functions for procedural animation calculations.
 * All functions are pure (no side effects) and take primitives.
 *
 * Features:
 * - Multiple interpolation strategies (Linear, EaseIn, EaseOut, EaseInOut, Step, Custom)
 * - Blend time calculation based on animation progress
 * - Timing calculation for hit reactions, recovery, etc.
 * - Extension point for future procedural systems
 */
UCLASS()
class KATANACOMBAT_API UProceduralAnimationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ============================================================================
    // CORE INTERPOLATION (Strategy-agnostic primitives)
    // ============================================================================

    /**
     * Apply interpolation strategy to normalize progress (0-1) → alpha (0-1).
     * Core primitive used by all higher-level functions.
     *
     * @param Progress Normalized progress (0-1)
     * @param Strategy Interpolation strategy to apply
     * @param CustomCurve Optional curve for CustomCurve strategy
     * @return Interpolated alpha (0-1)
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation",
              meta = (DisplayName = "Apply Interpolation Strategy"))
    static float ApplyStrategy(
        float Progress,
        EProceduralStrategy Strategy,
        UCurveFloat* CustomCurve = nullptr);

    /**
     * Linear interpolation with optional easing.
     * Maps progress → value using Min/Max and strategy.
     *
     * @param Progress Normalized progress (0-1)
     * @param MinValue Value at progress=1 (end of animation)
     * @param MaxValue Value at progress=0 (start of animation)
     * @param Strategy Interpolation strategy
     * @param CustomCurve Optional curve for CustomCurve strategy
     * @return Interpolated value
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation",
              meta = (DisplayName = "Interpolate With Strategy"))
    static float InterpolateWithStrategy(
        float Progress,
        float MinValue,
        float MaxValue,
        EProceduralStrategy Strategy,
        UCurveFloat* CustomCurve = nullptr);

    // ============================================================================
    // BLEND TIME CALCULATION
    // ============================================================================

    /**
     * Calculate procedural blend times based on animation state.
     * STATELESS - takes primitives, returns result struct.
     *
     * @param CurrentPosition Current position in source montage (seconds)
     * @param MontageLength Total length of source montage (seconds)
     * @param Config Blend configuration parameters
     * @param bIsRapidInput Was this triggered during an existing blend?
     * @return FProceduralBlendResult with calculated blend times
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Blend",
              meta = (DisplayName = "Calculate Procedural Blend"))
    static FProceduralBlendResult CalculateProceduralBlend(
        float CurrentPosition,
        float MontageLength,
        const FProceduralBlendConfig& Config,
        bool bIsRapidInput = false);

    /**
     * Simple blend time calculation (returns float only).
     * Uses Linear strategy for backwards compatibility.
     *
     * @param Progress Animation progress (0-1)
     * @param MinBlend Minimum blend time (at progress=1)
     * @param MaxBlend Maximum blend time (at progress=0)
     * @return Calculated blend time
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Blend",
              meta = (DisplayName = "Calculate Blend Time (Simple)"))
    static float CalculateBlendTimeSimple(
        float Progress,
        float MinBlend = 0.05f,
        float MaxBlend = 0.2f);

    /**
     * Calculate blend time with specified strategy.
     * More control than Simple, less than full Config.
     *
     * @param Progress Animation progress (0-1)
     * @param MinBlend Minimum blend time
     * @param MaxBlend Maximum blend time
     * @param Strategy Interpolation strategy
     * @return Calculated blend time
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Blend",
              meta = (DisplayName = "Calculate Blend Time With Strategy"))
    static float CalculateBlendTimeWithStrategy(
        float Progress,
        float MinBlend,
        float MaxBlend,
        EProceduralStrategy Strategy);

    // ============================================================================
    // TIMING CALCULATION (Extensible for hit reactions, recovery, etc.)
    // ============================================================================

    /**
     * Calculate procedural timing value from normalized input.
     * Generic function usable for any timing-based procedural system.
     *
     * @param InputValue Normalized input (0-1), meaning depends on context
     * @param Config Timing configuration
     * @return Calculated duration in seconds
     */
    UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Timing",
              meta = (DisplayName = "Calculate Procedural Timing"))
    static float CalculateProceduralTiming(
        float InputValue,
        const FProceduralTimingConfig& Config);

    // ============================================================================
    // EASING FUNCTIONS (Individual strategies exposed for flexibility)
    // ============================================================================

    /** Linear interpolation (no easing) */
    UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
    static float EaseLinear(float T) { return T; }

    /** Quadratic ease-in (slow start, fast end) */
    UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
    static float EaseInQuad(float T) { return T * T; }

    /** Quadratic ease-out (fast start, slow end) */
    UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
    static float EaseOutQuad(float T) { return 1.0f - (1.0f - T) * (1.0f - T); }

    /** Cubic ease-in-out (S-curve) */
    UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
    static float EaseInOutCubic(float T)
    {
        return T < 0.5f
            ? 4.0f * T * T * T
            : 1.0f - FMath::Pow(-2.0f * T + 2.0f, 3.0f) / 2.0f;
    }

    /** Exponential ease-out (very fast start, gradual end) */
    UFUNCTION(BlueprintPure, Category = "Combat|Procedural Animation|Easing")
    static float EaseOutExpo(float T)
    {
        return T >= 1.0f ? 1.0f : 1.0f - FMath::Pow(2.0f, -10.0f * T);
    }
};
```

**Implementation**: `Source/KatanaCombat/Private/Utilities/ProceduralAnimationLibrary.cpp`

```cpp
// ProceduralAnimationLibrary.cpp

#include "Utilities/ProceduralAnimationLibrary.h"
#include "Curves/CurveFloat.h"

float UProceduralAnimationLibrary::ApplyStrategy(
    float Progress,
    EProceduralStrategy Strategy,
    UCurveFloat* CustomCurve)
{
    const float T = FMath::Clamp(Progress, 0.0f, 1.0f);

    switch (Strategy)
    {
    case EProceduralStrategy::Linear:
        return EaseLinear(T);

    case EProceduralStrategy::EaseOut:
        return EaseOutQuad(T);

    case EProceduralStrategy::EaseIn:
        return EaseInQuad(T);

    case EProceduralStrategy::EaseInOut:
        return EaseInOutCubic(T);

    case EProceduralStrategy::Step:
        // Step returns 1.0 for any non-zero progress (handled by caller)
        return T > 0.0f ? 1.0f : 0.0f;

    case EProceduralStrategy::CustomCurve:
        if (CustomCurve)
        {
            return CustomCurve->GetFloatValue(T);
        }
        // Fallback to linear if no curve
        return EaseLinear(T);

    default:
        return EaseLinear(T);
    }
}

float UProceduralAnimationLibrary::InterpolateWithStrategy(
    float Progress,
    float MinValue,
    float MaxValue,
    EProceduralStrategy Strategy,
    UCurveFloat* CustomCurve)
{
    const float Alpha = ApplyStrategy(Progress, Strategy, CustomCurve);
    // Lerp from Max to Min (progress increases → value decreases)
    return FMath::Lerp(MaxValue, MinValue, Alpha);
}

FProceduralBlendResult UProceduralAnimationLibrary::CalculateProceduralBlend(
    float CurrentPosition,
    float MontageLength,
    const FProceduralBlendConfig& Config,
    bool bIsRapidInput)
{
    FProceduralBlendResult Result;
    Result.UsedStrategy = Config.Strategy;
    Result.bRapidInputDetected = bIsRapidInput;

    // Fresh attack (no previous montage)
    if (MontageLength <= 0.0f)
    {
        Result.bIsFreshAttack = true;
        Result.bUseInstantBlend = true;
        Result.BlendInTime = Config.MinBlendTime;
        Result.BlendOutTime = 0.0f;
        Result.AnimationProgress = 0.0f;
        return Result;
    }

    // Calculate progress
    Result.AnimationProgress = FMath::Clamp(CurrentPosition / MontageLength, 0.0f, 1.0f);

    // Handle rapid input based on mode
    if (bIsRapidInput)
    {
        switch (Config.RapidInputMode)
        {
        case ERapidInputBlendMode::ForceInstant:
            Result.bUseInstantBlend = true;
            Result.BlendInTime = 0.0f;
            Result.BlendOutTime = 0.0f;
            return Result;

        case ERapidInputBlendMode::ContinueCurrent:
            // Caller handles this - we just mark and continue
            break;

        case ERapidInputBlendMode::QueueUntilComplete:
            // Caller handles this - return invalid result
            Result.BlendInTime = -1.0f; // Signal to queue
            return Result;

        case ERapidInputBlendMode::Accelerate:
            // Calculate normal then multiply
            break;
        }
    }

    // Step strategy: instant if above threshold
    if (Config.Strategy == EProceduralStrategy::Step)
    {
        if (Result.AnimationProgress >= Config.InstantBlendThreshold)
        {
            Result.bUseInstantBlend = true;
            Result.BlendInTime = Config.MinBlendTime;
            Result.BlendOutTime = Config.MinBlendTime;
        }
        else
        {
            Result.BlendInTime = Config.MaxBlendTime;
            Result.BlendOutTime = Config.MaxBlendTime;
        }
        return Result;
    }

    // Apply interpolation strategy
    Result.RawInterpolationAlpha = ApplyStrategy(Result.AnimationProgress, Config.Strategy, Config.CustomBlendCurve);
    float BlendTime = FMath::Lerp(Config.MaxBlendTime, Config.MinBlendTime, Result.RawInterpolationAlpha);

    // Apply acceleration if rapid input in Accelerate mode
    if (bIsRapidInput && Config.RapidInputMode == ERapidInputBlendMode::Accelerate)
    {
        BlendTime /= Config.AccelerationMultiplier;
    }

    Result.BlendInTime = BlendTime;
    Result.BlendOutTime = BlendTime;
    Result.bUseInstantBlend = false;

    return Result;
}

float UProceduralAnimationLibrary::CalculateBlendTimeSimple(
    float Progress,
    float MinBlend,
    float MaxBlend)
{
    return InterpolateWithStrategy(Progress, MinBlend, MaxBlend, EProceduralStrategy::Linear, nullptr);
}

float UProceduralAnimationLibrary::CalculateBlendTimeWithStrategy(
    float Progress,
    float MinBlend,
    float MaxBlend,
    EProceduralStrategy Strategy)
{
    return InterpolateWithStrategy(Progress, MinBlend, MaxBlend, Strategy, nullptr);
}

float UProceduralAnimationLibrary::CalculateProceduralTiming(
    float InputValue,
    const FProceduralTimingConfig& Config)
{
    return InterpolateWithStrategy(
        InputValue,
        Config.MinDuration,
        Config.MaxDuration,
        Config.Strategy,
        Config.CustomCurve);
}
```

---

### STEP 3: Component Layer (CombatComponent.h/.cpp)

**Header Changes**:

1. **Add config member** (after `bInComboBlend` ~line 925):
```cpp
/** Procedural blend configuration */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation")
FProceduralBlendConfig ProceduralBlendConfig;
```

2. **Remove duplicate declaration** at line 242 (already added one earlier at line 100 - need to remove one):
   - Keep the declaration at line 100
   - Delete lines 227-242 (the duplicate I accidentally added)

**Implementation Changes** (CombatComponent.cpp PlayAttackMontage):

**Line 1420-1421**: Replace preset blend time retrieval with procedural calculation:

```cpp
// BEFORE (lines 1420-1421):
float BlendOutTime = 0.0f;
float BlendInTime = AttackData->ComboBlendInTime;

// AFTER:
FProceduralBlendResult BlendResult;
UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();

if (CurrentMontage)
{
    const float CurrentPosition = AnimInstance->Montage_GetPosition(CurrentMontage);
    const float MontageLength = CurrentMontage->GetPlayLength();
    BlendResult = UProceduralAnimationLibrary::CalculateProceduralBlend(
        CurrentPosition, MontageLength, ProceduralBlendConfig, bStillInBlendTransition);
}
else
{
    // Fresh attack - no current montage
    BlendResult.bIsFreshAttack = true;
    BlendResult.bUseInstantBlend = true;
    BlendResult.BlendInTime = ProceduralBlendConfig.MinBlendTime;
    BlendResult.BlendOutTime = 0.0f;
}

float BlendOutTime = BlendResult.BlendOutTime;
float BlendInTime = BlendResult.BlendInTime;
```

**Line 1446-1457**: Replace `CurrentAttackData->ComboBlendOutTime` lookup:

```cpp
// BEFORE (lines 1446-1457):
else if (CurrentAttackData)
{
    BlendOutTime = CurrentAttackData->ComboBlendOutTime;
    // debug log...
}

// AFTER:
else if (CurrentAttackData)
{
    // Blend times already calculated procedurally above
    if (GetDebugDraw() && (BlendOutTime > 0.0f || BlendInTime > 0.0f))
    {
        UE_LOG(LogCombat, Log, TEXT("[BLEND] Procedural combo transition: %s → %s (Progress: %.1f%%, BlendIn: %.2fs, BlendOut: %.2fs)"),
            *CurrentAttackData->GetName(),
            *AttackData->GetName(),
            BlendResult.AnimationProgress * 100.0f,
            BlendInTime,
            BlendOutTime);
    }
}
```

**Line 1428-1444**: Update rapid input handling to use procedural result:

```cpp
// BEFORE (lines 1428-1444):
if (bStillInBlendTransition)
{
    // ... force instant stop ...
    BlendOutTime = 0.0f;
    BlendInTime = 0.0f;
}

// AFTER:
if (bStillInBlendTransition)
{
    if (GetDebugDraw())
    {
        UE_LOG(LogCombat, Warning, TEXT("[BLEND] Rapid input during blend! Forcing instant cleanup."));
    }
    AnimInstance->StopAllMontages(0.0f);
    bInComboBlend = false;
    BlendTransitionEndTime = 0.0f;

    // Override procedural result with instant blend
    BlendResult.bUseInstantBlend = true;
    BlendOutTime = 0.0f;
    BlendInTime = 0.0f;
}
```

---

### STEP 4: Remove Duplicate Declaration

The earlier edit accidentally added a duplicate `CalculateProceduralBlendTime` declaration. Need to remove one.

**CombatComponent.h line 227-242**: DELETE these lines (keep the one at line 100):
```cpp
// DELETE THIS BLOCK:
/**
 * Calculate procedural blend time for animation transitions
 * ... (duplicate documentation) ...
 */
UFUNCTION(BlueprintPure, Category = "Combat|Animation")
float CalculateProceduralBlendTime(UAnimMontage* FromMontage, UAnimMontage* ToMontage) const;
```

**CombatComponent.h line 88-100**: KEEP this declaration (but update to match new API):
```cpp
// UPDATE to use the library function internally:
/**
 * Calculate procedural blend time based on animation position (BUG-2 FIX).
 * Wrapper around UMontageUtilityLibrary::CalculateProceduralBlend().
 *
 * @param FromMontage The montage being transitioned FROM (can be nullptr)
 * @param ToMontage The montage being transitioned TO (unused, for future)
 * @return Calculated blend time in seconds
 */
UFUNCTION(BlueprintPure, Category = "Combat|Animation")
float CalculateProceduralBlendTime(UAnimMontage* FromMontage, UAnimMontage* ToMontage) const;
```

**CombatComponent.cpp**: Add simple wrapper implementation:
```cpp
float UCombatComponent::CalculateProceduralBlendTime(UAnimMontage* FromMontage, UAnimMontage* ToMontage) const
{
    if (!FromMontage)
    {
        return ProceduralBlendConfig.MinBlendTime;
    }

    if (UAnimInstance* AnimInstance = GetAnimInstance())
    {
        const float CurrentPosition = AnimInstance->Montage_GetPosition(FromMontage);
        const float MontageLength = FromMontage->GetPlayLength();

        FProceduralBlendResult Result = UProceduralAnimationLibrary::CalculateProceduralBlend(
            CurrentPosition, MontageLength, ProceduralBlendConfig, false);

        return Result.BlendInTime;
    }

    return ProceduralBlendConfig.MinBlendTime;
}
```

---

### STEP 5: Include Updates

**CombatComponent.h**: Add include at top:
```cpp
#include "Data/ProceduralAnimationTypes.h"  // For FProceduralBlendConfig
```

**CombatComponent.cpp**: Add include:
```cpp
#include "Utilities/ProceduralAnimationLibrary.h"  // For blend calculation
```

**Update calls from MontageUtilityLibrary to ProceduralAnimationLibrary**:
```cpp
// BEFORE:
UMontageUtilityLibrary::CalculateProceduralBlend(...)

// AFTER:
UProceduralAnimationLibrary::CalculateProceduralBlend(...)
```

---

### STEP 6: Verification

1. **Build**: Compile and verify no errors
2. **Test**: Run all 207 tests, verify no regressions
3. **Manual Test**:
   - Light combo 1→2→3 from idle - verify smooth blends
   - Rapid double-tap during Active phase - verify no "partial blend"
   - Interrupt mid-attack with new attack - verify slower blend
   - Attack near end of recovery - verify faster blend
4. **Debug Log Check**: Enable `Combat.Debug.All 1`, look for `[BLEND] Procedural` messages showing calculated blend times

---

### Benefits

- **Types Layer**: Clean data definition, Blueprint-exposed for designer tuning
- **Library Layer**: Stateless, testable, reusable across systems
- **Component Layer**: Owns state, routes data, maintains existing API
- **No preset carryover**: Blend calculated fresh for each transition
- **Debug visibility**: Rich result struct exposes animation progress
- **Backwards compatible**: Old AttackData fields ignored, not deleted

### Deprecations

- `AttackData.ComboBlendInTime` - Keep property, ignore in code
- `AttackData.ComboBlendOutTime` - Keep property, ignore in code

---

## Implementation Priority Order (UPDATED 2026-02-06)

**User Decision**: Camera collision and hit detection are CRITICAL. Counter system deferred.

| Phase | Task | Status | Notes |
|-------|------|--------|-------|
| **BUG-1** | Fix movement disabled after finisher | ✅ DONE | Fixed in `EndPairedAnimation()` |
| **BUG-2** | Implement procedural blend system | ✅ DONE | "Buttery smooth" - user confirmed |
| **BUG-3** | Fix combo chain broken | ✅ DONE | Guard in `GetAttackForInput()` |
| **INPUT-1** | Combo flip-flop during rapid spam | 🔴 **CRITICAL** | Spam light → 1,2,partial 3,back to 1 |
| **CAM-1** | Camera collision with enemies | 🔴 **CRITICAL** | Camera pushed inside player near enemies |
| **HIT-1** | Hit detection robustness | 🔴 **CRITICAL** | Hits being dropped, insufficient hit info |
| **BUG-4** | Fix input pairing during blends | ⏸️ Deferred | Lower priority after BUG-2 fix |
| **DATA** | Assign DefaultHeavyAttack in CombatSettings | ⏸️ Deferred | Data asset fix |
| Counter System | AC3/Arkham counter-kills | ⏸️ Deferred | After critical bugs resolved |

---

## Overview

A single, focused counter system implementation aligned with Batman Arkham/AC3 design philosophy. One button during enemy attack = instant counter-kill. Player dominance, natural flow, no stat whittling.

| Input | Result | Feel |
|-------|--------|------|
| Counter during enemy attack | Instant counter-kill | "I'm a badass" |
| Timing in last 20% of window | Perfect counter (bonus effects) | "I'm a *perfect* badass" |

---

## Design Philosophy (User Constraints)

**These constraints are NON-NEGOTIABLE based on user feedback:**

| ✅ DO | ❌ DON'T |
|-------|----------|
| Natural flow, player dominance | Rigid frame data or frame-perfect timing |
| Backend procedural/contextual animation selection | Multiple stat pools (posture, stamina, etc.) |
| Generous timing windows (400-600ms) | "Whittling away" at enemy bars |
| Single-player, player-first emphasis | Fighting game complexity |
| Player "in control of the arena exerting their will" | Tedious resource management |

### Core Feeling
> "The player should be in control of the arena exerting their will."

This means:
- **Counter = Kill** (on basic enemies) - not "counter = some damage"
- **Flow state** - chain counter-kills across multiple enemies without interruption
- **Contextual spectacle** - system picks the right animation, player just presses the button
- **Forgiving inputs** - timing windows are generous, prediction over reaction

---

## Phase 0: Enemy AI Prerequisites (EXISTING - Already Implemented)

**Status**: Already scaffolded in codebase from previous work. Files exist.

**Goal**: Enemies that attack in readable, counterable patterns. Without this, there's nothing to counter.

### 0.1 Combat Token System (EXISTS)

**File**: `Source/KatanaCombat/Public/AI/CombatTokenSubsystem.h`
**Purpose**: Prevents enemy spam-attacks, ensures player has time to read and react.

```cpp
// Already implemented - UCombatTokenSubsystem
int32 MaxConcurrentAttackers = 2;  // AC3 uses 1-2
bool RequestAttackToken(AActor* Requester);
void ReleaseAttackToken(AActor* Holder);
bool HasAttackToken(AActor* Actor) const;
```

### 0.2 Base Enemy State Machine

**File**: `Source/KatanaCombat/Public/AI/EnemyStateTypes.h` (NEW)

```cpp
UENUM(BlueprintType)
enum class EEnemyAIState : uint8
{
    Idle,           // Standing, not engaged
    Circling,       // Moving around player, waiting for token
    Approaching,    // Has token, moving into attack range
    Attacking,      // Executing attack (has counter window)
    Recovering,     // Post-attack recovery
    Staggered,      // Hit/parried, vulnerable
    Dying           // Death sequence
};
```

### 0.3 Enemy Combat AI Component

**File**: `Source/KatanaCombat/Public/AI/EnemyCombatAIComponent.h` (NEW)

```cpp
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class UEnemyCombatAIComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // Current AI state
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    EEnemyAIState CurrentState = EEnemyAIState::Idle;

    // Attack decision making
    UFUNCTION(BlueprintCallable, Category = "AI")
    bool TryInitiateAttack();  // Requests token, transitions to Approaching

    UFUNCTION(BlueprintCallable, Category = "AI")
    void ExecuteAttack();  // Plays attack montage with CounterWindow

    // Response to player actions
    UFUNCTION(BlueprintCallable, Category = "AI")
    void OnCountered();  // Player countered this enemy

    UFUNCTION(BlueprintCallable, Category = "AI")
    void OnParried();  // Player parried this enemy

    // Circling behavior
    UPROPERTY(EditAnywhere, Category = "AI|Circling")
    float CircleRadius = 400.0f;

    UPROPERTY(EditAnywhere, Category = "AI|Circling")
    float CircleSpeed = 200.0f;
};
```

### 0.4 Behavior Tree Tasks

**File**: `Source/KatanaCombat/Public/AI/BTTask_RequestAttackToken.h` (NEW)
**File**: `Source/KatanaCombat/Public/AI/BTTask_ExecuteAttack.h` (NEW)
**File**: `Source/KatanaCombat/Public/AI/BTTask_CirclePlayer.h` (NEW)
**File**: `Source/KatanaCombat/Public/AI/BTDecorator_HasAttackToken.h` (NEW)

### 0.5 Basic Enemy Behavior Tree

```
Root
├─ Selector
│   ├─ Sequence [HasAttackToken]
│   │   ├─ BTTask_ApproachPlayer (until in range)
│   │   ├─ BTTask_ExecuteAttack (plays montage with CounterWindow)
│   │   └─ BTTask_ReleaseToken
│   │
│   └─ Sequence [!HasAttackToken]
│       ├─ BTTask_RequestAttackToken
│       └─ BTTask_CirclePlayer (while waiting)
```

### 0.6 Enemy Attack Configuration

**File**: `EnemyAttackData.h` (NEW or extend AttackData)

Each enemy attack needs:
- Attack montage with `AnimNotifyState_CounterWindow`
- Swing direction (ESwingDirection) for pose-matching
- Counter response data (UPairedAnimationData)
- Attack range and timing

---

## Phase 1: Shared Foundation

### 1.1 Counter Window Detection

**File**: `Source/KatanaCombat/Public/Animation/AnimNotifyState_CounterWindow.h` (NEW)

```cpp
UCLASS(meta = (DisplayName = "Counter Window"))
class UAnimNotifyState_CounterWindow : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    // Attack type for pose-matching (AC3 mode)
    UPROPERTY(EditAnywhere, Category = "Counter")
    EAttackType AttackType = EAttackType::Light;

    // Attack direction for procedural adjustment
    UPROPERTY(EditAnywhere, Category = "Counter")
    EAttackDirection AttackDirection = EAttackDirection::Horizontal;

    // Counter data for this specific attack (AC3: counter-kill, Chain: parry response)
    UPROPERTY(EditAnywhere, Category = "Counter")
    TObjectPtr<UPairedAnimationData> CounterData;

    virtual void NotifyBegin(...) override;
    virtual void NotifyEnd(...) override;
};
```

### 1.2 Attack Type Tagging System

**File**: `CombatTypes.h` - Add:

```cpp
// Attack direction for procedural pose-matching
UENUM(BlueprintType)
enum class EAttackDirection : uint8
{
    Horizontal,     // Side swings
    Vertical,       // Overhead/uppercut
    Thrust,         // Stabs, lunges
    Sweep,          // Low attacks
    Grab            // Unarmed grabs
};

// Counter context passed to animation selection
USTRUCT(BlueprintType)
struct FCounterContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EAttackType AttackType;

    UPROPERTY(BlueprintReadOnly)
    EAttackDirection AttackDirection;

    UPROPERTY(BlueprintReadOnly)
    AActor* Attacker;

    UPROPERTY(BlueprintReadOnly)
    UPairedAnimationData* SpecificCounterData;  // If attack has specific counter

    UPROPERTY(BlueprintReadOnly)
    float TimeInWindow;  // For perfect counter detection
};
```

### 1.3 Visual Counter Indicator

**File**: `Source/KatanaCombat/Public/UI/CounterIndicatorComponent.h` (NEW)

```cpp
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class UCounterIndicatorComponent : public UWidgetComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Indicator")
    TSubclassOf<UUserWidget> IndicatorWidgetClass;

    UPROPERTY(EditAnywhere, Category = "Indicator")
    FLinearColor NormalColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, Category = "Indicator")
    FLinearColor PerfectWindowColor = FLinearColor::Yellow;

    void ShowIndicator();
    void HideIndicator();
    void SetPerfectWindowActive(bool bActive);
};
```

### 1.4 Stickiness / Targeting Lock

**File**: `TargetingComponent.h` - Add:

```cpp
// Lock targeting to specific enemy after parry/counter
UFUNCTION(BlueprintCallable, Category = "Targeting|Counter")
void LockToCounterTarget(AActor* Target);

UFUNCTION(BlueprintCallable, Category = "Targeting|Counter")
void ReleaseCounterLock();

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Targeting|Counter")
TWeakObjectPtr<AActor> CounterLockedTarget;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Targeting|Counter")
bool bIsCounterLocked = false;
```

### 1.5 CombatComponent Counter API

**File**: `CombatComponent.h` - Add:

```cpp
// Mode selection
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Counter|Mode")
ECounterSystemMode CounterMode = ECounterSystemMode::Chain;

// Core counter API (mode-agnostic)
UFUNCTION(BlueprintCallable, Category = "Combat|Counter")
bool TryCounter();  // Routes to appropriate mode

UFUNCTION(BlueprintPure, Category = "Combat|Counter")
bool CanCounter() const;

UFUNCTION(BlueprintPure, Category = "Combat|Counter")
AActor* FindCounterableEnemy() const;

// Counter context
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Counter")
FCounterContext CurrentCounterContext;

// Delegates
UPROPERTY(BlueprintAssignable, Category = "Counter")
FOnCounterExecuted OnCounterExecuted;  // (AActor* Target, bool bWasPerfect)
```

---

## Phase 2: Counter System Implementation (PRIMARY)

Full pose-matched counter-kill system. This IS the counter system - not a "mode."

### 2.1 Counter-Kill Selection Logic

**File**: `CombatComponent.cpp`

```cpp
bool UCombatComponent::TryCounter_AC3Mode()
{
    FCounterContext Context = CurrentCounterContext;
    if (!Context.Attacker) return false;

    // 1. Get pose-matched counter animation
    UPairedAnimationData* CounterKill = ResolveCounterKillAnimation(Context);
    if (!CounterKill) return false;

    // 2. Determine if perfect counter (last 20% of window)
    bool bPerfect = (Context.TimeInWindow / CounterWindowDuration) > 0.8f;

    // 3. Apply procedural adjustments for pose-matching
    FProceduralCounterParams ProceduralParams;
    ProceduralParams.AttackDirection = Context.AttackDirection;
    ProceduralParams.AttackerLocation = Context.Attacker->GetActorLocation();
    ProceduralParams.ContactHeight = GetEstimatedContactHeight(Context);

    // 4. Execute counter-kill (always lethal in AC3 mode)
    return ExecuteCounterKill(CounterKill, Context.Attacker, bPerfect, ProceduralParams);
}

UPairedAnimationData* UCombatComponent::ResolveCounterKillAnimation(const FCounterContext& Context)
{
    // Priority 1: Attack-specific counter (from AnimNotifyState)
    if (Context.SpecificCounterData)
        return Context.SpecificCounterData;

    // Priority 2: Weapon counter pool by attack type
    if (UWeaponData* WeaponData = GetCurrentWeaponData())
    {
        if (UPairedAnimationData* PooledCounter = WeaponData->GetCounterForAttackType(Context.AttackType, Context.AttackDirection))
            return PooledCounter;
    }

    // Priority 3: Generic counter fallback
    return DefaultCounterKillData;
}
```

### 2.2 Counter-Kill Animation Pool

**File**: `WeaponData.h` - Add:

```cpp
// Pool of counter-kill animations organized by attack type
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Counter")
TMap<EAttackDirection, FCounterKillPool> CounterKillPools;

// Get appropriate counter-kill for context
UFUNCTION(BlueprintPure, Category = "Counter")
UPairedAnimationData* GetCounterForAttackType(EAttackType Type, EAttackDirection Direction) const;
```

**File**: `CombatTypes.h` - Add:

```cpp
USTRUCT(BlueprintType)
struct FCounterKillPool
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TObjectPtr<UPairedAnimationData>> CounterAnimations;

    UPairedAnimationData* GetRandomCounter() const;
};
```

### 2.3 Perfect Counter Enhancements (AC3)

```cpp
void UCombatComponent::ExecuteCounterKill(UPairedAnimationData* CounterData, AActor* Target,
                                          bool bPerfect, const FProceduralCounterParams& Params)
{
    if (bPerfect)
    {
        // Slow-mo for dramatic effect
        UCinematicEffectsUtilityLibrary::ApplySlowMotion(GetWorld(), 0.2f, 0.4f);

        // Enhanced VFX
        PlayPerfectCounterVFX();
    }

    // Counter-kills are ALWAYS lethal in AC3 mode
    CounterData->bIsLethal = true;

    // Execute paired animation with procedural adjustments
    ExecutePairedAnimationWithProcedural(CounterData, Target, Params);
}
```

---

## ~~Phase 3: Chain Mode~~ (REMOVED)

> **User Decision**: "I really think using sekiro and having multiple stat pools governing finishers and enemy vulnerability state will introduce overcomplexity and result in an overengineered system that doesn't feel like it flows naturally. The player should be in control of the arena exerting their will and it will not feel this way if they are whittling away different stat bars."

Chain Mode and the posture system have been **intentionally removed** from this plan. The AC3/Arkham approach (Phase 2) is the sole counter system implementation.

**What was removed:**
- EChainCounterState state machine
- Posture system (MaxPosture, CurrentPosture, PostureRegenRate)
- Light/Heavy counter choice
- Multi-step parry → counter → finisher flow
- Guard break mechanics tied to posture

**What we keep:**
- Simple health-based finisher eligibility (< 25% health = finisher available)
- Existing `IsVulnerableToFinisher()` logic (works without posture)

---

## Phase 4: Procedural Animation Layer

Shared between both modes. Reduces hand-authored animation requirements.

### 4.1 Procedural Counter Parameters

**File**: `CombatTypes.h` - Add:

```cpp
USTRUCT(BlueprintType)
struct FProceduralCounterParams
{
    GENERATED_BODY()

    // Attack direction for IK/pose adjustment
    UPROPERTY(BlueprintReadOnly)
    EAttackDirection AttackDirection;

    // Contact point for IK targeting
    UPROPERTY(BlueprintReadOnly)
    FVector EstimatedContactPoint;

    // Height adjustment (low/mid/high)
    UPROPERTY(BlueprintReadOnly)
    float ContactHeight;

    // Attacker's weapon tip location (for blade contact IK)
    UPROPERTY(BlueprintReadOnly)
    FVector AttackerWeaponTip;
};
```

### 4.2 Control Rig Integration

**File**: `Source/KatanaCombat/Public/Animation/CounterControlRigParams.h` (NEW)

```cpp
// Parameters passed to Control Rig for runtime adjustment
USTRUCT(BlueprintType)
struct FCounterControlRigParams
{
    GENERATED_BODY()

    // IK target for weapon hand (blade contact point)
    UPROPERTY(BlueprintReadWrite, Category = "IK")
    FVector WeaponHandIKTarget;

    // Torso rotation adjustment (face attacker)
    UPROPERTY(BlueprintReadWrite, Category = "Adjustment")
    FRotator TorsoRotationOffset;

    // Arm extension blend (reach for contact)
    UPROPERTY(BlueprintReadWrite, Category = "IK")
    float ArmExtensionAlpha;

    // Height layer blend (low/mid/high additive)
    UPROPERTY(BlueprintReadWrite, Category = "Blend")
    float HeightBlendAlpha;
};
```

### 4.3 Procedural Adjustment Application

**File**: `CombatComponent.cpp`

```cpp
void UCombatComponent::ApplyProceduralCounterAdjustments(
    USkeletalMeshComponent* Mesh,
    const FProceduralCounterParams& Params)
{
    // Calculate Control Rig parameters
    FCounterControlRigParams RigParams;

    // IK: Weapon hand toward contact point
    RigParams.WeaponHandIKTarget = Params.EstimatedContactPoint;

    // Torso: Rotate toward attacker
    FVector ToAttacker = (Params.AttackerWeaponTip - Mesh->GetComponentLocation()).GetSafeNormal();
    RigParams.TorsoRotationOffset = ToAttacker.Rotation();

    // Height: Blend between low/mid/high additives
    RigParams.HeightBlendAlpha = FMath::GetMappedRangeValueClamped(
        FVector2D(50.0f, 150.0f),  // Low to high range
        FVector2D(0.0f, 1.0f),
        Params.ContactHeight);

    // Apply to Control Rig
    if (UControlRig* Rig = Mesh->GetAnimInstance()->GetControlRig())
    {
        Rig->SetControlValue(TEXT("CounterParams"), RigParams);
    }
}
```

---

## Files Summary

### Phase 0 - AI Prerequisites (EXISTS - Minor Updates)

| File | Status | Purpose |
|------|--------|---------|
| `AI/CombatTokenSubsystem.h/.cpp` | EXISTS | Token system for coordinated enemy attacks |
| `AI/EnemyStateTypes.h` | EXISTS | EEnemyAIState enum |
| `AI/EnemyCombatAIComponent.h/.cpp` | EXISTS | Enemy combat decision-making component |
| `AI/BTTask_*.h/.cpp` | EXISTS | BT tasks for attack token workflow |
| `BT_BasicEnemy` | NEEDS SETUP | Blueprint behavior tree asset |

### Phase 1-2 - Counter System (New Files)

| File | Purpose |
|------|---------|
| `AnimNotifyState_CounterWindow.h/.cpp` | Counter window detection on enemy's attack montage |
| `CounterIndicatorComponent.h/.cpp` | Visual indicator above enemy (optional) |
| `CounterControlRigParams.h` | Procedural adjustment parameters for pose-matching |
| `CounterSystemTests.cpp` | Test suite for counter-kill flow |

### Modified Files

| File | Changes |
|------|---------|
| `CombatTypes.h` | FCounterContext, EAttackDirection (already has ESwingDirection), FCounterKillPool |
| `CombatComponent.h/.cpp` | TryCounter(), FindCounterableEnemy(), CurrentCounterContext |
| `TargetingComponent.h/.cpp` | Counter lock / stickiness (LockToCounterTarget, ReleaseCounterLock) |
| `WeaponData.h` | Counter-kill animation pools (TMap<EAttackDirection, FCounterKillPool>) |

---

## Verification

### Build
- Compile with no errors
- All existing 207 tests pass

### Unit Tests (New)
- Counter window detection (AnimNotifyState_CounterWindow)
- Counter context building (FCounterContext population)
- Pose-matched animation selection (direction-based pool resolution)
- Perfect counter detection (TimeInWindow thresholds)
- Counter-kill execution flow

### Manual Testing
1. **Basic Flow**: Enemy attacks → indicator appears → tap counter → pose-matched counter-kill
2. **Direction Variety**: Horizontal vs Vertical vs Thrust attacks → different counter animations
3. **Perfect Timing**: Counter in last 20% of window → slow-mo + enhanced VFX
4. **Multi-Enemy**: Counter-kill enemy A → immediately counter enemy B (flow state)
5. **Edge Cases**: Counter when no enemy attacking → nothing happens (graceful fail)

---

## Design Decisions

| Question | Decision | Rationale |
|----------|----------|-----------|
| System complexity | AC3-only, no modes | User rejected stat whittling, multi-bar complexity |
| Pose-matching approach | Attack type + direction tagging | Balances context-awareness with animation count |
| Procedural tool priority | IK + Control Rig | Already in UE5.6, reduces hand-authored needs |
| Counter window duration | 500ms (generous) | AC3 accessibility, player-first single-player |
| Perfect counter threshold | Last 20% of window | Rewards timing without punishing imprecision |
| Finisher eligibility | Health < 25% only | Simple, no posture bars to whittle |
| Stickiness release | After counter-kill or explicit disengage | Maintains flow state |

---

## Hit State Integration (From Audit Research)

**Reference**: `docs/audits/HIT_STATE_SYSTEM_AUDIT_2026-02-05.md`

### Key Insight: Counter ≠ Hit State System

In the AC3/Arkham approach, the counter system **bypasses** the hit state hierarchy entirely:

| Scenario | Result | Hit State Involvement |
|----------|--------|----------------------|
| Counter during Counter Window | Instant death | **None** - bypass entirely |
| Normal attack on enemy | Hitstun | Brief, enemy recovers |
| Health < 25% | Finisher-eligible | Simple health check |

### Key Fix Already Applied

**Bug**: `FHitReactionEntry::StunDuration` defaulted to 0.3f → unintended finisher triggers.

**Fix**: Changed default to 0.0f in `CombatTypes.h` and `HitReactionData.h`. This was committed and tested.

### Simplified Finisher Eligibility

With posture system removed, finisher eligibility is now:
```cpp
bool IsVulnerableToFinisher() const
{
    // Simple: low health = vulnerable
    return GetHealthPercent() <= 0.25f;  // 25%
}
```

No stagger tracking, no posture bars, no guard break states. Just health threshold.

---

## Future Enhancements (Post Core Implementation)

These features wait until the base counter system is proven:

- **Multi-Enemy Counter Chains**: AC3-style double/triple kills when multiple enemies attack simultaneously
- **Environmental Counters/Finishers**: Context-sensitive executions near walls, ledges, objects
- **Weapon-Specific Pools**: Different counter animations per weapon type

---

## PRIORITY 0: AttackData Cleanup (Investigation Complete)

> **User Concern**: "There seem to be a lot of fields in the attack data asset that are either not in use or for which it is not clear what they do and the downstream effects they carry."

---

### P0-A: Validation Circular Dependency Errors (ROOT CAUSE FOUND)

**Location**: `AttackData.cpp` lines 303-517

**Root Cause**: Cascading validation in `DetectCycles()` function

The validation calls `DetectCycles()` which recursively validates ALL downstream assets in combo chains. With chains like `LightAttack_1 → LightAttack_2 → ... → LightAttack_11`:
- Each of 11 assets validates ~10 downstream assets
- String-based error filtering (PT-17 NOTE, lines 314-327) doesn't work correctly
- Errors from nested calls report their own asset name, not the root
- **Result**: ~100-200 cascading duplicate errors

**Fix Options** (in order of preference):

| Fix | Effort | Description |
|-----|--------|-------------|
| **Fix 1: Prevent Re-traversal** | Low | Line 425 removes from Visited set allowing re-entry. Keep visited items in set to prevent cascading. |
| **Fix 2: Centralized Validation** | Medium | Single graph walk, collect all errors at root level only |
| **Fix 3: Proper Context Tracking** | Medium | Pass FDataValidationContext to DetectCycles(), use Context.GetAssociatedObject() |

**Recommended**: Fix 1 - Minimal change, prevents the recursive explosion.

---

### P0-B: Deprecated Fields (AUDIT COMPLETE)

| Field | Status | Action |
|-------|--------|--------|
| `PostureDamage` | DEPRECATED - marked "[NOT YET IMPLEMENTED]" | Remove or comment out |
| `CounterDamageMultiplier` | DEPRECATED - never referenced | Remove or comment out |
| `ChargedPostureDamage` | DEPRECATED - guard system not wired | Remove or comment out |
| `HeavyDirectionalFollowUps` | DEPRECATED - removed from gameplay | Remove references and map |

**UNCLEAR Fields** (declared but not enforced):

| Field | Issue | Action |
|-------|-------|--------|
| `bUseSectionOnly` | Declared, no runtime use | Document intention or remove |
| `bJumpToSectionStart` | Declared, no runtime use | Document intention or remove |
| `bEnforceMaxHoldTime` | Gate exists, no enforcement | Wire up or remove |
| `MaxHoldTime` | Gated but not enforced | Wire up or remove |
| `MaxChargeDamageMultiplier` | Declared but not applied to damage calc | Wire up or remove |
| `RequiredContextTags` | TODO comment, not implemented | Keep for future or remove |

---

### P0-C: Editor Tools (AUDIT COMPLETE - ALL WORKING ✅)

**Result**: All 6 editor tools are fully functional. No broken or obsolete tools.

| Tool | Status | Purpose |
|------|--------|---------|
| AttackDataTools | ✅ Working | Auto-calculate timing, generate notifies, validation |
| AttackDataCustomization | ✅ Working | Custom details panel for AttackData assets |
| HitReactionDataCustomization | ✅ Working | Custom details panel for HitReactionData |
| HitReactionEntryCustomization | ✅ Working | Struct customization for array elements |
| ReactionMontageVariantCustomization | ✅ Working | Variant-based reaction customization |
| Paired Animation Preview | ✅ Working | 6,000+ line professional tool for finisher validation |

**Access**: Window > Paired Animation Preview

---

### Implementation Order

| Step | Task | Files |
|------|------|-------|
| 1 | Fix validation cascading (Fix 1) | `AttackData.cpp` line 425 |
| 2 | Remove deprecated fields | `AttackData.h` |
| 3 | Document or wire unclear fields | `AttackData.h`, `CombatComponent.cpp` |
| 4 | Test validation no longer spams | Save any AttackData asset |
| 5 | THEN proceed to counter system | Phase 1-2 below |

---

## DETAILED TOUCH POINT MAPS (Bug Fixes)

### BUG-1: Movement Disabled After Finisher - TOUCH POINTS

| Location | Action | Reason |
|----------|--------|--------|
| `CombatComponent.cpp:4333-4340` | Add explicit movement restore | `CompletePairedAnimation()` cleanup path |
| `CombatComponent.cpp:4380-4385` | Add movement restore in `EndPairedAnimation()` | Alternative exit path |
| `CombatComponent.h:906` | Document `bMovementCurrentlyDisabled` lifecycle | For future maintenance |
| `HitReactionComponent.cpp:871-875` | Verify `DisableMovement()` in `ActivateRagdoll()` | Ensure ragdoll path correct |

**Root Cause Chain**:
```
Finisher completes
  → CompletePairedAnimation() [line 4168]
  → ClearHoldState() [line 948] - NO OP (finishers don't use hold)
  → UpdateMovementFromMontageState() [line 2802]
  → Checks HoldState.IsHolding() = false [line 2824]
  → Checks HoldState.bIsEasing = false [line 2830]
  → bShouldLockMovement calculated as false [line 2848]
  → BUT bMovementCurrentlyDisabled was true from finisher start
  → Flag never explicitly cleared
```

**Fix Code** (add to `CompletePairedAnimation()` BEFORE `SetPhase(None)`):
```cpp
// Restore movement explicitly since finishers don't use hold mechanics
if (bMovementCurrentlyDisabled)
{
    if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
    {
        MovementComp->SetMovementMode(MOVE_Walking);
    }
    bMovementCurrentlyDisabled = false;
}
```

---

### BUG-2: Procedural Blend System - TOUCH POINTS

| Location | Action | Reason |
|----------|--------|--------|
| `CombatComponent.h:~970` | Add `CalculateProceduralBlendTime()` declaration | New function |
| `CombatComponent.cpp:1416-1528` | Replace `FAlphaBlendArgs` usage in `PlayAttackMontage()` | Use procedural blend |
| `CombatComponent.cpp:1504` | Remove preset blend time usage | Replace with procedural |
| `CombatComponent.cpp:2902-2917` | Keep playrate restoration (still needed) | Complements procedural blend |
| `AttackData.h:~150` | Mark `ComboBlendInTime`/`ComboBlendOutTime` deprecated | Keep fields, ignore in code |

**Input Capture During Blend - Additional Touch Points**:
| Location | Action | Reason |
|----------|--------|--------|
| `CombatComponent.cpp:1423-1444` | Enhance "rapid input during blend" fix | Clear stale inputs |
| `CombatComponent.cpp:615-618` | Add queue clearing on blend start | Prevent stale context |
| `CombatTypes.h:1332` | Document `ComboBlendEndTime` purpose | For maintenance |

**Procedural Blend Logic**:
```cpp
float UCombatComponent::CalculateProceduralBlendTime(UAnimMontage* FromMontage, UAnimMontage* ToMontage) const
{
    constexpr float MinBlend = 0.05f;
    constexpr float MaxBlend = 0.2f;

    if (!FromMontage) return MinBlend;

    if (UAnimInstance* AnimInstance = GetAnimInstance())
    {
        float Position = AnimInstance->Montage_GetPosition(FromMontage);
        float Length = FromMontage->GetPlayLength();
        float Progress = Length > 0 ? Position / Length : 1.0f;

        // Near end = fast blend (natural), mid-animation = slow blend (smooth interrupt)
        return FMath::Lerp(MaxBlend, MinBlend, Progress);
    }
    return MinBlend;
}
```

---

### BUG-3: CurrentAttack Cleared While ComboWindow Active - TOUCH POINTS

**ROOT CAUSE IDENTIFIED FROM DEBUG LOGS:**
```
[RESOLVE] Input=Light, ComboWindow=ACTIVE, CurrentAttack=nullptr
[RESOLVE] Resolved to Default: 'LightAttack_1'  // Should be LightAttack_2!
```

The `CurrentAttackData` pointer is being cleared while `bComboWindowActive` remains true.
This causes combo chains to reset to attack 1 instead of progressing.

| Location | Action | Reason |
|----------|--------|--------|
| `CombatComponent.cpp:993` | Check CurrentAttackData assignment | Set in ExecuteAction after PlayAttackMontage |
| `CombatComponent.cpp:2519` | Check SetPhase(None) | May clear CurrentAttackData |
| `CombatComponent.cpp:2539` | Check ClearHoldState() | May clear attack state |
| `CombatComponent.cpp:2570-2700` | Check OnMontageEnded | May clear CurrentAttackData before combo window expires |
| `CombatComponent.h:674` | Review `CurrentAttackData` lifecycle | When is it cleared vs when should it be |

**CONFIRMED Root Cause Chain:**
```
Attack 1 completes (Recovery → end)
  → OnMontageEnded fires
  → SetPhase(None) called [line 2519]
  → CurrentAttackData = nullptr [line 2530]  // EVENT-based clear
  → BUT bComboWindowActive still true (TIME-based expiration at 6.733s)
  → Second tap arrives: ComboWindow=ACTIVE, CurrentAttack=nullptr
  → GetAttackForInput checks bComboWindowActive → true → tries combo
  → But CurrentAttackData->NextComboAttack would crash, so falls back to default
```

**TIME vs EVENT desync:**
- `bComboWindowActive`: Cleared by checkpoint expiration (TIME-based, 6.733s)
- `CurrentAttackData`: Cleared by SetPhase(None) (EVENT-based, montage end)

**Fix Option A** (Recommended - Safe):
In `GetAttackForInput()`, add guard: if `bComboWindowActive && !CurrentAttackData`, treat as non-combo.
```cpp
// After line 3343
if (bShouldCombo && !CurrentAttackData)
{
    // Combo window is active but attack reference is gone - can't continue combo
    bShouldCombo = false;
}
```

**Fix Option B** (More invasive):
In `SetPhase(None)`, when clearing `CurrentAttackData`, also clear `bComboWindowActive` and expire combo checkpoints.

**Additional Bug Found - Missing Heavy Attack:**
```
Error: [RESOLVE] CRITICAL: Default Heavy attack is nullptr!
```
CombatSettings is missing DefaultHeavyAttack reference.

| Location | Action | Reason |
|----------|--------|--------|
| `CombatSettings` asset | Assign DefaultHeavyAttack | Missing reference in data asset |

---

### BUG-4: Input Down/Release Pairing - TOUCH POINTS

| Location | Action | Reason |
|----------|--------|--------|
| `CombatComponent.cpp:543-561` | Review PRESS event flow | HeldInputs[InputType] = CurrentTime |
| `CombatComponent.cpp:562-584` | Review RELEASE event flow | ProcessInputPair() call |
| `CombatComponent.cpp:3293-3307` | Enhance `ProcessInputPair()` | Currently stub - needs implementation |
| `CombatComponent.h:692` | Document `HeldInputs` TMap lifecycle | Press/release matching |
| `ActionQueueTypes.h:94-129` | Review `FQueuedInputAction` struct | Timestamp handling |

**Input Loss Vulnerabilities** (from audit):

| Vulnerability | Location | Risk | Mitigation |
|---------------|----------|------|------------|
| Paired animation blocks input | `CombatComponent.cpp:597` | Medium | Input LOST during finishers (by design) |
| CombatSettings null | `CombatComponent.cpp:456` | Low | Caught at startup |
| AttackData resolution fails | `CombatComponent.cpp:634` | Medium | Needs fallback |
| Direction capture in wrong context | `CombatComponent.cpp:522` | Low | By design |
| Hold state reset mid-ease | `CombatComponent.cpp:2119` | Low | Handled by guards |

**Input Flow Chain** (confirmed by audit):
```
EnhancedInput::ETriggerEvent::Started
  → PlayerCharacter::OnLightAttackPressed() [line 151]
  → CombatComponent::OnInputEventAuto() [line 155]
  → OnInputEventWithTransform() [line 377]
  → OnInputEvent() [line 453]
    → CanProcessInput() check [line 462]
    → HeldInputs[InputType] = CurrentTime [line 551]
    → QueueAction() [line 587]
      → DetermineExecutionMode() [line 3309]
      → IMMEDIATE (Recovery/None) or QUEUED (Windup/Active)
```

---

## REGRESSION AUDIT FINDINGS

### P0 - Critical (Fix Immediately)

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| R1 | Race condition in `CompletePairedAnimation()` | `CombatComponent.cpp:2593-2605` | Double damage application |
| R2 | Missing `GetOwner()` null check | `CombatComponent.cpp:4214-4215` | Crash if attacker destroyed |
| R3 | `OnAnyMontageBlendingOut` re-entrancy | `HitReactionComponent.cpp:791` | State corruption |

**R1 Details**: `OnMontageEnded()` calls `CompletePairedAnimation()` without checking if already completing. Multiple montage callbacks during blend transitions could trigger race.

**R2 Details**: `HitInfo.HitDirection = (Victim->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();` - GetOwner() not null-checked.

**R3 Details**: `ExitPairedAnimationState()` clears pending state, but no guard against re-entrancy if callback fires twice.

### P1 - High (Fix Before Counter System)

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| R4 | `TryExecuteFinisher` target not validated | `CombatComponent.cpp:1062-1089` | Silent failure |
| R5 | `bReactionsSuppressed` not reset on all paths | `HitReactionComponent.cpp:1148-1196` | Permanent stuck state |
| R6 | `CurrentFinisherVictim` weak ptr not validated | `CombatComponent.cpp:4193` | UB on pending-kill actor |
| R7 | Multiple montage callbacks bypass state machine | `CombatComponent.cpp:2592-2618` | Inconsistent state |

### P2 - Medium (Fix During Implementation)

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| R8 | `bRagdollActivated` double-activation race | `HitReactionComponent.cpp:821-826` | Frozen instead of ragdoll |
| R9 | Missing MovementComponent null check | `CombatComponent.cpp:2814-2818` | Crash on custom characters |
| R10 | Timer cleanup incomplete in EndPlay | `CombatComponent.cpp:100-104` | Memory leak |
| R11 | `GetWorld()` null during shutdown | `CombatComponent.cpp:489` | PIE crash |
| R12 | ActionQueue modification during iteration | `CombatComponent.cpp:2702-2756` | Rare crash |
| R13 | AnimInstance null not checked | `HitReactionComponent.cpp:42` | Silent failure |

---

## ROLLBACK STRATEGY

### Commit Checkpoint Plan

Each bug fix gets its own commit with rollback checkpoint:

| Phase | Commit Message | Rollback Hash |
|-------|----------------|---------------|
| DATA | `Fix missing DefaultHeavyAttack in CombatSettings` | Current HEAD (150cd3a) |
| BUG-3 | `Fix combo chain broken when CurrentAttack cleared before ComboWindow expires` | Post DATA commit |
| BUG-1 | `Fix movement disabled after finisher (BUG-1)` | Post BUG-3 commit |
| BUG-2 | `Implement procedural blend system (BUG-2)` | Post BUG-1 commit |
| BUG-4 | `Fix input pairing during blends (BUG-4)` | Post BUG-2 commit |
| R1-R3 | `Fix P0 regressions (R1-R3)` | Post BUG-4 commit |
| R4-R7 | `Fix P1 regressions (R4-R7)` | Post R1-R3 commit |

**Note**: BUG-3 prioritized over BUG-1/2 because it's the primary combo chain issue identified in debug logs.

### Rollback Commands

```bash
# If BUG-1 fix causes issues:
git revert HEAD  # Reverts BUG-1

# If BUG-2 causes issues but BUG-1 is fine:
git revert HEAD  # Reverts BUG-2, keeps BUG-1

# Emergency full rollback to known-good state:
git reset --hard 150cd3a  # Back to pooled FX commit

# Partial rollback keeping some fixes:
git revert <specific-commit-hash>
```

### Test Checkpoints

| After | Verify |
|-------|--------|
| DATA | DefaultHeavyAttack resolve errors gone from logs |
| BUG-3 | Tap twice from idle → both attacks play (combo chain works) |
| BUG-1 | Movement works after finisher, all 207 tests pass |
| BUG-2 | Combo blends feel smooth, no "partial blend" issues |
| BUG-4 | Hold detection works during rapid combo transitions |
| R1-R3 | No crashes in finisher edge cases |
| R4-R7 | No stuck states, weak pointers validated |

### Pre-Implementation Safeguard

Before ANY code changes:
```bash
git stash  # Save any uncommitted work
git log -1 --oneline  # Note current HEAD
# Expected: 150cd3a Implement pooled impact FX system (UCombatFXData)
```

---

## VERIFICATION PLAN

### Build Verification
- Compile with no errors after each commit
- All 207 existing tests pass

### Manual Testing Checklist

**BUG-1 (Movement)**:
- [ ] Execute finisher → verify movement immediately available
- [ ] Execute finisher → attack → verify attack works
- [ ] Execute finisher → evade → verify evade works

**BUG-2 (Procedural Blend)**:
- [ ] Light combo 1→2→3 → verify smooth transitions
- [ ] Rapid input during combo → verify no "partial blend"
- [ ] Interrupt mid-attack → verify new attack blends smoothly

**BUG-3 (Input Pairing)**:
- [ ] Hold light → release during blend → verify direction captured
- [ ] Rapid tap during recovery → verify input queued
- [ ] Hold during active phase → verify hold activates at window

**Regression Fixes**:
- [ ] Kill attacker during finisher → no crash (R2)
- [ ] Interrupt finisher → no stuck state (R5)
- [ ] Rapid finisher attempts → no double damage (R1)
