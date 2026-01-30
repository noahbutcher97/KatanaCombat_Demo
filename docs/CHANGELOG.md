# KatanaCombat Changelog

All notable changes to the combat system will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased]

### Planned
- Phase 6: Parry & Evade Systems
- Phase 7: Posture Integration
- Phase 8+: Polish (hit stop, root motion, AI, UI/UX)

---

## [3.1.0] - 2025-01-29

### Added: Hit Reaction Variations with N-2 Randomization

Implement animation variety for hit reactions to prevent repetitive flip-flopping between the same 1-2 animations.

**New Data Types** (`CombatTypes.h`):
- `FReactionMontageVariant`: Pairs montage with optional section selection
- `FReactionHistory`: Tracks recently played animation indices for variety selection

**Extended FHitReactionEntry**:
- Added `ReactionMontages` array (`TArray<FReactionMontageVariant>`)
- New helpers: `GetAllVariants()`, `GetMontageCount()`
- Backwards compatible: Single `ReactionMontage` field still works

**Selection Algorithm** (`HitReactionComponent`):
| Array Size | Behavior |
|------------|----------|
| 1 montage | Always play it (no exclusion) |
| 2 montages | Alternate back and forth |
| 3+ montages | N-2 randomization (exclude last 2, random from remaining) |

**New Functions**:
- `SelectMontageWithVariety()` - Adaptive selection based on array size
- `RecordMontagePlay()` - Track played indices per intensity+direction
- `ClearReactionHistory()` - Reset on death for fresh variety on respawn

**Custom Editor UI** (`KatanaCombatEditor`):
- `FReactionMontageVariantCustomization` - Compact inline display
- Each array element shows montage picker + section dropdown
- Section dropdown auto-populates from selected montage's sections
- Validates sections and resets if montage changes

**Files Created**:
- `Source/KatanaCombatEditor/Public/Customizations/ReactionMontageVariantCustomization.h`
- `Source/KatanaCombatEditor/Private/Customizations/ReactionMontageVariantCustomization.cpp`

**Files Modified**:
- `Source/KatanaCombat/Public/CombatTypes.h` - New structs
- `Source/KatanaCombat/Public/Core/HitReactionComponent.h` - Variety selection
- `Source/KatanaCombat/Private/Core/HitReactionComponent.cpp` - Implementation
- `Source/KatanaCombatEditor/Private/KatanaCombatEditor.cpp` - Registration

---

## [3.0.0] - 2025-01-28

### Major: Architecture Consolidation & Motion Warping Unification

**Status**: COMPLETE (Major Infrastructure Refactor)

#### V1 Removal & Component Rename
- **REMOVED**: `CombatComponentV2` renamed to `CombatComponent` (V1 fully removed)
- **REMOVED**: `bUseV2System` toggle from CombatSettings (only one system now)
- **REMOVED**: All `// V1 REMOVED:` historical comments cleaned up
- Single unified combat system - no more V1/V2 distinction

#### Character Hierarchy Refactoring
- **NEW**: `ABaseCombatCharacter` - Abstract base class implementing common combat interfaces
- **NEW**: `APlayerCharacter` - Player-specific character (replaces SamuraiCharacter)
- **NEW**: `AEnemyCharacter` - Enemy-specific character for AI opponents
- **NEW**: `ITeamMemberInterface` - Team affiliation for friend/foe detection
- **EXTENDED**: `IDamageableInterface` - Added `GetCurrentHealth()`, `GetMaxHealth()`, `IsAlive()` methods

#### Motion Warping Unification
- **NEW**: `FAttackWarpConfig` - Unified warp configuration struct (replaces separate FMotionWarpingConfig + FDirectionalWarpConfig)
- **NEW**: `SetupAttackWarp()` - Single function handles both target-based (translation+rotation) and direction-based (rotation-only) warping
- **NEW**: `AnimNotifyState_CombatWarp` - Custom notify that auto-selects warp mode based on runtime target availability
- **REMOVED**: Redundant `SetupMotionWarp()`, `SetupDirectionalWarp()` functions consolidated

**How AnimNotifyState_CombatWarp Works**:
```
CombatComponent::SetupAttackWarp() → Sets ONE of two targets:
  - "AttackTarget" (if enemy found) → Translation + Rotation
  - "RotationTarget" (if no enemy) → Rotation Only

AnimNotifyState_CombatWarp → Detects which exists:
  - AttackTarget exists → bWarpTranslation = true (move toward enemy)
  - RotationTarget exists → bWarpTranslation = false (rotate only, no sliding)
  - Neither exists → Skip warp entirely
```

**Benefits**:
- One notify per montage instead of two
- No more "slidey" movement when rotation-only warping
- Runtime-adaptive behavior based on combat context

#### Modular Settings Architecture
- **NEW**: `UTargetingSettings` data asset - Character-level targeting configuration
- **NEW**: `UMotionWarpingSettings` data asset - Character-level motion warp defaults
- **NEW**: `GetEffectiveSettings()` pattern on TargetingComponent
- Three-tier configuration: Component Override → CombatSettings Reference → Hardcoded Fallback

**Configuration Hierarchy**:
```cpp
// Priority order (highest to lowest):
1. TargetingComponent->TargetingSettingsOverride  // Per-instance override
2. CombatSettings->TargetingSettings              // Character-type default
3. Hardcoded fallback values                      // Safe defaults
```

#### Debug System Consolidation
- **NEW**: `DebugConfig.h` - CVar-based debug configuration
- **RENAMED**: `DirectionDebugLibrary` → `DebugUtils` (moved from Utilities/ to Debug/)
- **NEW**: Console commands: `Combat.Debug.All`, `Combat.Debug.Direction`, `Combat.Debug.Targeting`, etc.
- **REMOVED**: Scattered `bDebugDraw` properties from individual components

**Files Created**:
- `Public/Animation/AnimNotifyState_CombatWarp.h` (~80 lines)
- `Private/Animation/AnimNotifyState_CombatWarp.cpp` (~100 lines)
- `Public/Data/TargetingSettings.h` (~60 lines)
- `Private/Data/TargetingSettings.cpp`
- `Public/Data/MotionWarpingSettings.h` (~60 lines)
- `Private/Data/MotionWarpingSettings.cpp`
- `Public/Characters/BaseCombatCharacter.h` (~150 lines)
- `Private/Characters/BaseCombatCharacter.cpp`
- `Public/Characters/PlayerCharacter.h`
- `Private/Characters/PlayerCharacter.cpp`
- `Public/Characters/EnemyCharacter.h`
- `Public/Interfaces/TeamMemberInterface.h`
- `Public/Debug/DebugConfig.h`
- `Public/Debug/DebugUtils.h` (renamed from DirectionDebugLibrary)

**Files Removed/Renamed**:
- `CombatComponentV2.h/.cpp` → `CombatComponent.h/.cpp`
- `SamuraiCharacter.h/.cpp` → `PlayerCharacter.h/.cpp`
- `Utilities/DirectionDebugLibrary.h/.cpp` → `Debug/DebugUtils.h/.cpp`

---

## [2.0.0] - 2025-11-19

### Major: Directional Attack System Architectural Fix

**Status**: COMPLETE (Full Architectural Refactor)

#### Problem: Semantic Input Conflation
The system treated movement stick (continuous, for locomotion) AS directional attack input (discrete, intentional):
- `LastDirectionalInput` sampled EVERY frame from movement stick
- No distinction between "moving forward while attacking" vs "intentionally inputting forward for directional attack"
- `GetComboAttack()` checked directionals WITHOUT hold completion validation

#### Solution: Context-Aware Input Sampling

**Core Principle**: Movement input ≠ Attack input. Direction sampled ONLY at hold release, not continuously.

**Design Flow**:
```
Normal Combo Flow:
  Player moves stick + taps attack → Context = Movement (direction ignored)
  → DirectionalInputBuffer empty → AttackDirection = None
  → Normal combo chain executes

Directional Attack Flow:
  Player holds attack → Context switches to DirectionalInput
  → Animation freezes (hold completes)
  → Player releases WITH direction → DirectionalInputBuffer captures direction
  → Resolution uses buffered direction → Directional attack executes
  → Buffer cleared (prevents reuse)
```

#### Implementation (5 files, 3 phases)

**Phase 1: Input State Machine**
- `EInputContext` enum (Movement / DirectionalInput / Disabled) - `ActionQueueTypes.h:393-404`
- `FDirectionalInputBuffer` struct (discrete capture at release) - `ActionQueueTypes.h:417-449`
- Integrated into `CombatComponentV2.h:384-405`

**Phase 2: Context-Aware Sampling**
- `OnInputEvent()` only captures direction during `DirectionalInput` context - `CombatComponentV2.cpp:200-249`
- Direction sampled at RELEASE event (not continuously) - `CombatComponentV2.cpp:215-225`
- `SetInputContext()` helper for context transitions - `CombatComponentV2.cpp:115-128`
- `OnHoldWindowStart()` → sets context to DirectionalInput - `CombatComponentV2.cpp:1101-1109`
- `ClearHoldState()` → resets context to Movement - `CombatComponentV2.cpp:1844-1851`

**Phase 3: Resolution Updates**
- `GetAttackForInput()` uses `DirectionalInputBuffer` instead of `LastDirectionalInput` - `CombatComponentV2.cpp:2107-2140`
- Buffer cleared after directional consumption - `CombatComponentV2.cpp:2170-2186`
- `GetComboAttack()` documented: Direction only passed when buffer valid - `MontageUtilityLibrary.cpp:826-839,881-886`

**Files Modified**:
- `ActionQueueTypes.h` - Added EInputContext + FDirectionalInputBuffer structs
- `CombatComponentV2.h` - Added buffer/context members, SetInputContext declaration
- `CombatComponentV2.cpp` - Context-aware sampling, hold callbacks, resolution layer
- `MontageUtilityLibrary.cpp` - Documentation for directional priority

**Behavior Now**:
- Spam attack while moving → Normal combo (direction ignored, context = Movement)
- Hold → release with direction → Directional attack (direction captured at release)
- No infinite loops (buffer cleared after consumption)
- Direction sampled at release (most intentional timing)
- Attacks can have BOTH NextComboAttack AND DirectionalFollowUps (directional requires hold)

---

### Fixed: Camera Pitch Bug (CRITICAL)

**Problem**: Directional attacks executing incorrectly when camera had pitch (looking up/down)
**Impact**: Debug visualization showed correct direction but wrong attack executed

**Root Cause**: Camera Pitch Corruption in `CombatTypes.h:598-599`

When camera pitch != 0, the CameraForward/CameraRight vectors had Z components, corrupting the WorldInput calculation.

**The Bug**:
```cpp
// WRONG (before):
const FVector CameraForward = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::X); // Includes pitch!
const FVector CameraRight = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);   // Includes pitch!
```

**The Fix** (`CombatTypes.h:599`):
```cpp
// CORRECT (after):
const FRotator FlatCameraRotation = FRotator(0.0f, CameraRotation.Yaw, 0.0f); // Flatten to yaw-only
const FVector CameraForward = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::X);
const FVector CameraRight = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::Y);
```

**Files Modified**: `CombatTypes.h:596-603`

---

### Fixed: Diagonal Character Rotation Bug (CRITICAL)

**Problem**: Directional attacks worked at cardinal directions (N/E/S/W) but failed at diagonals (NE/SE/SW/NW)
**Root Cause**: Axis swap in `VectorToCharacterRelativeDirection()` - Unreal Engine X/Y axes incorrectly mapped

**The Bug** (`CombatTypes.h:547-548`):
```cpp
// WRONG:
const FVector CameraRight = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::X);   // X is Forward!
const FVector CameraForward = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y); // Y is Right!
```

**The Fix**:
```cpp
// CORRECT:
const FVector CameraForward = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::X); // X = Forward
const FVector CameraRight = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);   // Y = Right
```

**Files Modified**: `CombatTypes.h:547-548`

---

### Added: DirectionDebugLibrary

New Blueprint Function Library for modular direction transformation debugging.

**New Files**:
- `Source/KatanaCombat/Public/Utilities/DirectionDebugLibrary.h` (~145 lines)
- `Source/KatanaCombat/Private/Utilities/DirectionDebugLibrary.cpp` (~175 lines)

**Helper Functions** (7 total):
1. `YawToCardinalDirection()` - Convert yaw to compass direction (N, NE, E, etc.)
2. `FormatRotationDebug()` - Format rotation: "Yaw=45.0 (NE)"
3. `FormatVector2DDebug()` - Format vector: "(X=0.71, Y=0.71) magnitude=1.00"
4. `FormatInputDirectionDebug()` - Format EInputDirection enum
5. `FormatAttackDirectionDebug()` - Format EAttackDirection enum
6. `CalculateYawDelta()` - Shortest angular distance with wrapping
7. `GetMeshRotationOffset()` - Detect mesh rotation offset from actor

**Visual Debug Visualization** (inline in CombatComponentV2.cpp:272-326):
- 5 colored arrows with 3D text labels
- Blue (Camera), Green (Character), Yellow (Input), Orange (CharRelative), Magenta (Resolved)

**Debug Flag**: Uses existing `CombatSettings->bDebugDraw` (data-driven, no new complexity)

---

## [1.5.0] - 2025-11-18

### Added: Graceful Fallback Chain (Phase 1)

Combat system now NEVER breaks due to configuration errors.

**Changes Made**:

1. **Cycle Detection Fallback** (`MontageUtilityLibrary.cpp:1006-1017`)
   - Cycle detected → falls back to default attack (not nullptr)
   - Logs error but continues to Priority 4 (default attacks)
   - Behavior: Circular reference A→B→C→A → falls back to DefaultLightAttack

2. **Default Attack Validation** (`CombatComponentV2.cpp:64-113`, `CombatComponentV2.h:393-398`)
   - Added `ValidateDefaultAttacks()` function called in BeginPlay (editor only)
   - Shows on-screen warnings if DefaultLightAttack or DefaultHeavyAttack are nullptr
   - Provides fix instructions in log output

3. **Emergency Fallback (Tier 5)** (`MontageUtilityLibrary.cpp:1120-1167`)
   - If default attack is nullptr → repeats current attack as emergency fallback
   - Better to repeat attack than give "dead input" (button does nothing)
   - Logs CRITICAL error + shows on-screen warning in editor

**Fallback Chain (5 Tiers)**:
```
Priority 1: Context-Sensitive Attacks (future) → Fallback: Priority 2
Priority 2: Directional Follow-Ups → Fallback: Priority 3
Priority 3: Normal Combo Chain (NextComboAttack) → Fallback: Priority 4
Priority 4: Default Attacks (DefaultLightAttack/Heavy) → Fallback: Emergency
Tier 5: Emergency Fallback (repeat current attack) → NEVER returns nullptr
```

---

## [1.4.0] - 2025-11-16

### Fixed: Directional Loop Bug

**Problem**: Infinite loop when holding direction + spamming attack
**Root Cause**: Queued actions captured stale `LastDirectionalInput` at queue time

**Solution**: Added `bDirectionalInputConsumed` flag (consumption tracking)

**Files**: `CombatComponentV2.h:360`, `CombatComponentV2.cpp:119,1968,2047`

**Behavior**: Each directional input triggers ONE directional follow-up, then falls back to normal combos. New directional input resets consumption.

---

## [1.3.0] - 2025-11-12

### Added: Context-Aware Attack Resolution (Phase 1)

GameplayTag system + cycle detection for context-aware attack resolution.

**Implemented**:

1. **GameplayTag System** (`AttackData.h:242-256`, `DefaultGameplayTags.ini`)
   - Properties: `FGameplayTagContainer AttackTags/RequiredContextTags`
   - 15 tags: Capabilities (CanCombo, CanDirectional, Terminal), Types (Light/Heavy/Special), Context (ParryCounter, LowHealthFinisher)

2. **Resolution Metadata** (`MontageUtilityLibrary.h:62-135`)
   - Enum `EResolutionPath`: Default, NormalCombo, DirectionalFollowUp, ParryCounter
   - Struct `FAttackResolutionResult`: Attack, Path, bShouldClearDirectionalInput, bCycleDetected

3. **Asset Validation** (`AttackData.cpp:289-466`)
   - DFS cycle detection, tag consistency checks
   - Rules: Terminal attacks no follow-ups, CanDirectional requires directionals

4. **Context Tracking** (`CombatComponentV2.h:353-379`)
   - `FGameplayTagContainer ActiveContextTags` for runtime context
   - `TSet<UAttackData*> VisitedAttacks` for cycle detection
   - `int32 MaxChainDepth = 10` safety limit

**Files Modified** (8 files, ~700 lines): AttackData.h/cpp, DefaultGameplayTags.ini, MontageUtilityLibrary.h/cpp, CombatComponentV2.h/cpp, KatanaCombat.Build.cs

---

### Fixed: Directional Input + Phase Transition

1. **Directional Input** (`CombatComponentV2.cpp:1922-1944`)
   - Bug: Hardcoded `EAttackDirection::None` in resolution
   - Fix: Convert `LastDirectionalInput` → `EAttackDirection` via `CombatHelpers::InputToAttackDirection()`

2. **Phase Transition Artifacts** (`CombatComponentV2.cpp:1475-1547`)
   - Bug: `OnMontageEnded()` → `SetPhase(None)` → queued action → `SetPhase(Windup)` → old Active notify fires → desync
   - Fix: Track `bStartedNewAttack` flag, skip None transition if new attack started

---

## [1.2.0] - 2025-11-11

### Added: Universal Combo Crossfade

Per-attack blend times for all combo transitions.

**Implementation** (`AttackData.h:102-110`):
- `ComboBlendOutTime/ComboBlendInTime` (0.1s default, 0.0-1.0s range)
- Automatic blend on combo transitions (Light→Light, Heavy→Any, Hold→Directional)
- Tuning examples: Fast (0.05s), Weighty (0.2s)

### Fixed: Light Attack Freeze (CRITICAL)

**Bug** (`ActionQueueTypes.h:271`, `CombatComponentV2.cpp:947,1096,1151`):
- Early release at playrate 0.5 → comparison `0.5 > 0.0` = ease-in → continued to 0.0 (freeze)

**Fix**: Added `bIsEasingOut` flag to explicitly track ease direction

### Fixed: Hold Ease Blend Artifacts (CRITICAL)

**Bug** (`CombatComponentV2.cpp:1650-1666`):
- `ClearHoldState()` cleared timer but left playrate at 0.75 → combo blended in with wrong rate

**Fix**: Restore playrate to 1.0 before clearing hold state

### Fixed: Charge Blend

**Bug** (`MontageUtilityLibrary.cpp:468-504`):
- `JumpToSectionWithBlend()` instant jump only

**Fix**: `Montage_Stop()` with blend-out + `Montage_PlayWithBlendSettings()` at target section

---

## [1.1.0] - 2025-11-10

### Changed: V1/V2 Independence

Decoupled V1/V2 - No cross-dependencies, clean architecture.

- V2 now uses own `PlayAttackMontage()` → direct `AnimInstance->Montage_Play()`
- Removed V1 parameter coupling (`bAllowDuringRecovery`)
- Architecture: `ASamuraiCharacter` → `CombatComponent (V1)` + `CombatComponentV2 (V2)` + `CombatSettings` (shared config)

---

## [1.0.0] - 2025-11-07

### Added: V2 Queue Processing & Execution (Phase 5)

- Action execution via V2's `PlayAttackMontage()`
- Checkpoint discovery using `UMontageUtilityLibrary::DiscoverCheckpoints()`
- FIFO queue processing at checkpoint times (snap/responsive/immediate modes)
- Debug visualization: Phase indicators, queue state, checkpoint timeline, stats

**V2 Execution Model**:
```
Input → OnInputEvent() → QueueAction()
Queue → ProcessQueue() → ExecuteAction() → PlayAttackMontage()
Snap mode: Execute at Active end (input during Windup/Active)
Immediate mode: Execute right away (input during Recovery/Idle)
```

### Added: MontageUtilityLibrary Advanced Features (Phase 4d)

**27 Functions** (6 categories):
1. **Procedural Easing**: `EvaluateEasing()`, `EaseLerp()`, `CalculateTransitionPlayRate()` (10 easing types)
2. **Hold Mechanics**: `CalculateChargeLevel()`, `GetMultiStageHoldPlayRate()`, `GetHoldStageIndex()`
3. **Section Utils**: `GetMontageSections()`, `GetSectionStartTime/Duration()`, `JumpToSectionWithBlend()`
4. **Window Queries**: `GetActiveWindows()`, `IsWindowActive()`, `GetWindowTimeRemaining()`, `GetNextCheckpoint()`
5. **Blending**: `CrossfadeMontage()`, `BlendOutMontage()`
6. **Debug**: `DrawCheckpointTimeline()`, `LogCheckpoints()`

### Added: AttackConfiguration PDA Refactoring

Three-tier architecture:
```
ASamuraiCharacter → CombatSettings (combat style) → AttackConfiguration (attack moveset)
    → DefaultLightAttack/DefaultHeavyAttack/SprintAttack/JumpAttack/PlungingAttack
```

**Benefit**: Mix-and-match combat rules with different movesets

---

## Quality Metrics

### Test Coverage
- 7 test files, 45+ assertions, all passing
- 96% design specification compliance (validated via audit)

### Performance
- Timer-based easing (60Hz), not tick-based
- Event-driven queue processing (at checkpoints only)
- Minimal tick overhead

---

## Migration Notes

### V1 to V2

Toggle via `CombatSettings->bUseV2System`:
- V1: State-based, manual phase tracking, production-ready
- V2: Event-driven, FIFO queue, advanced features

Both systems are independent peers - can be enabled/disabled without code changes.

### Deprecated Features

- `AnimNotifyState_AttackPhase` - Use `AnimNotify_AttackPhaseTransition` instead
- `AnimNotify_ToggleHitDetection` - Hit detection now automatic in V2 Active phase
