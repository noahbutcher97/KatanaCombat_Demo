# Paired Animation System - Implementation Plan

## Overview
Implementing a production-quality paired animation system for KatanaCombat inspired by:
- **Assassin's Creed 3** (primary reference) - Parry→Counter→Finisher flow, 3200 fight animations
- **Ghost of Tsushima** - Weapon-based paired animations, "Lethality Contract", stance system
- **Batman Arkham Knight** - Freeflow combat, token-based AI coordination, IK contact points

## User Design Requirements

### Core Principles
- **Modular with accessible interfaces** - Easy swap-in testing of animation pairs
- **Environmental awareness** - Terrain, obstacles, walls, ledges
- **Spatial awareness** - Character positioning and gap closing
- **Kinematic awareness** - Weapon/mesh overlap detection
- **Anatomical awareness** - Contact points, character self-overlap
- **Data-driven + Procedural** - Configuration combined with math-based detection

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

### Needs Implementation
| Component | Priority | Complexity |
|-----------|----------|------------|
| AttackData paired fields | P0 | Low |
| Victim-side warp setup | P0 | Medium |
| AnimNotifyState_PairedSync | P0 | Medium |
| PairedAnimationComponent | P1 | High |
| Sync point delegate/events | P1 | Low |
| Impact normal extraction | P2 | Low |
| Obstacle validation | P2 | Medium |
| Anatomical positioning | P3 | High |

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

### New Files (6)
| File | Purpose |
|------|---------|
| `Source/KatanaCombat/Public/Utilities/PairedAnimationUtilityLibrary.h` | Static utility functions |
| `Source/KatanaCombat/Private/Utilities/PairedAnimationUtilityLibrary.cpp` | Implementation |
| `Source/KatanaCombat/Public/Data/PairedAnimationData.h` | Data asset for paired anims |
| `Source/KatanaCombat/Private/Data/PairedAnimationData.cpp` | Implementation |
| `Source/KatanaCombat/Public/Animation/AnimNotifyState_PairedAnimationSync.h` | Sync point notify |
| `Source/KatanaCombat/Private/Animation/AnimNotifyState_PairedAnimationSync.cpp` | Implementation |

### Modified Files (6)
| File | Changes |
|------|---------|
| `CombatTypes.h` | Add `FPairedWarpConfig`, `FFinisherTriggerConfig`, `FOnPairedAnimationStarted` delegate |
| `AttackData.h` | Add FinisherData, CounterData fields |
| `TargetingComponent.h/.cpp` | Add `SetupVictimWarp()` |
| `HitReactionComponent.h/.cpp` | Add `IsVulnerableToFinisher()`, `GetFinisherTriggerReason()`, trigger paired reaction |
| `CombatComponent.h/.cpp` | Finisher execution flow, finisher input detection |
| `CombatSettings.h` | Add `FFinisherTriggerConfig` defaults |

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

### Manual Testing
1. Create test PairedAnimationData asset
2. Configure finisher on existing attack
3. Trigger finisher in game
4. Verify both characters animate in sync
5. Verify terrain adjustment works
6. Verify damage applies at sync point

---

## Implementation Order

### Week 1: Foundation
1. Create `FPairedWarpConfig` struct in CombatTypes.h
2. Create `UPairedAnimationData` data asset
3. Add paired fields to AttackData
4. Create `UPairedAnimationUtilityLibrary` with basic functions

### Week 2: Warp System
5. Add `SetupVictimWarp()` to TargetingComponent
6. Implement victim continuous tracking (mirror attacker)
7. Create `AnimNotifyState_PairedAnimationSync`

### Week 3: Integration
8. Connect finisher flow in CombatComponent
9. Trigger victim reaction via HitReactionComponent
10. Add delegate for sync point events

### Week 4: Polish & Test
11. Add obstacle validation
12. Implement slow-motion integration
13. Write comprehensive tests
14. Create example paired animation asset

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

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| Animation clipping | Use obstacle validation before triggering |
| Terrain issues | Leverage existing ground sampling |
| Sync drift | Continuous warp tracking like attacker system |
| Network replication | Design for future but defer implementation |
| Performance | Event-driven (no tick), timer-based updates |

---

## Sources Referenced

- [GDC: Animating The 3rd Assassin](https://gdcvault.com/play/1017635/Animation-Bootcamp-Animating-The-3rd) - AC3 3200 animations
- [GDC: Motion Warping in Gears of War 4](https://www.gdcvault.com/play/1024219/Motion-Warping-in-Gears-of) - Warp points
- [GDC: Master of the Katana](https://gdcvault.com/play/1027194/Master-of-the-Katana-Melee) - GoT combat
- [Freeflow Arena UE5](https://discover.therookies.co/2025/10/28/building-a-cinematic-combat-system-in-unreal-engine-5/) - Modern recreation
- [Contextual Animation Plugin](https://vorixo.github.io/devtricks/contextual-anim/) - UE5.3+ paired anims
- [Motion Matching For Honor](https://www.gameanim.com/2016/05/03/motion-matching-ubisofts-honor/) - Sync techniques
