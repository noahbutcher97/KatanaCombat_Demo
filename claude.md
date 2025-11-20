# Claude Code - KatanaCombat Project

**AI assistant onboarding guide for Ghost of Tsushima-inspired melee combat system (UE 5.6, C++).**

---

## ✅ FIXED: Directional Attack System (Architectural Fix)

**Status**: ✅ **COMPLETE** (2025-11-19, Full Architectural Refactor)

**Original Symptoms**:
1. Moving while attacking → incorrectly triggered directional follow-ups (no hold required)
2. Holding direction + spamming attack → infinite loop of same directional
3. Multiple failed "fixes" that addressed symptoms but not root cause

**Root Cause**: **Semantic Input Conflation**
- System treated movement stick (continuous, for locomotion) AS directional attack input (discrete, intentional)
- `LastDirectionalInput` sampled EVERY frame from movement stick
- No distinction between "moving forward while attacking" vs "intentionally inputting forward for directional attack"
- `GetComboAttack()` checked directionals WITHOUT hold completion validation (bypassed Priority 2 gate)

**Architectural Solution**: **Context-Aware Input Sampling**

**Core Principle**: Movement input ≠ Attack input. Direction sampled ONLY at hold release, not continuously.

**Design**:
```
Normal Combo Flow:
  Player moves stick + taps attack → Context = Movement (direction ignored)
  → DirectionalInputBuffer empty → AttackDirection = None
  → Normal combo chain executes ✅

Directional Attack Flow:
  Player holds attack → Context switches to DirectionalInput
  → Animation freezes (hold completes)
  → Player releases WITH direction → DirectionalInputBuffer captures direction
  → Resolution uses buffered direction → Directional attack executes
  → Buffer cleared (prevents reuse) ✅
```

**Implementation** (5 files, 3 phases):

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
- ✅ Spam attack while moving → Normal combo (direction ignored, context = Movement)
- ✅ Hold → release with direction → Directional attack (direction captured at release)
- ✅ No infinite loops (buffer cleared after consumption)
- ✅ Direction sampled at release (most intentional timing)
- ✅ Attacks can have BOTH NextComboAttack AND DirectionalFollowUps (directional requires hold)

---

## Instant Context

**Project**: 4-component combat system, pragmatic design, data-driven tuning

**Core Identity** (MEMORIZE):
1. **Phases vs Windows**: Phases exclusive (Windup→Active→Recovery), Windows overlap (ParryWindow, ComboWindow, HoldWindow)
2. **Input ALWAYS Buffered**: Combo window modifies WHEN, not WHETHER
3. **Parry = Contextual Block**: Defender checks enemy's ParryWindow (attacker's montage)
4. **Hold = Button State Check**: At window start, NOT duration tracking
5. **Movement ≠ Attack Input**: Direction sampled ONLY at hold release (context-aware), not continuously
6. **Delegates in CombatTypes.h**: Declared ONCE, components use `UPROPERTY` only

**Essential Docs**:
- `docs/SYSTEM_PROMPT.md` (25 KB, 10 min) - **READ FIRST** before any work
- `docs/ARCHITECTURE_QUICK.md` (8 KB, 3 min) - Keep open while coding
- `docs/ARCHITECTURE.md` (52 KB) - Deep dive for complex features

**Default Values**: ComboInputWindow 0.6s | ParryWindow 0.3s | ComboBlendOut/In 0.1s | MaxPosture 100.0f | LightDamage 25.0f | HeavyDamage 50.0f

---

## Recent Changes (Reverse Chronological)

### 2025-11-19: Camera Pitch Bug Fix ✅ **CRITICAL**

**Fixed**: Directional attacks executing incorrectly when camera had pitch (looking up/down)
**Impact**: Debug visualization showed correct direction but wrong attack executed

#### Root Cause: Camera Pitch Corruption
**Symptom**: Player looks up/down → press directional attack → wrong direction executes
**Location**: `CombatTypes.h:598-599` (VectorToCharacterRelativeDirection)

**The Bug**:
```cpp
// WRONG (before):
const FVector CameraForward = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::X); // ❌ Includes pitch!
const FVector CameraRight = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);   // ❌ Includes pitch!
```

When camera pitch != 0, the CameraForward/CameraRight vectors had Z components, corrupting the WorldInput calculation even after Z normalization.

**The Fix** (`CombatTypes.h:599`):
```cpp
// CORRECT (after):
const FRotator FlatCameraRotation = FRotator(0.0f, CameraRotation.Yaw, 0.0f); // ✅ Flatten to yaw-only
const FVector CameraForward = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::X);
const FVector CameraRight = FRotationMatrix(FlatCameraRotation).GetScaledAxis(EAxis::Y);
```

**Why Visualization Worked**: SamuraiCharacter::Tick() already flattened camera rotation (line 83), but the actual capture code in VectorToCharacterRelativeDirection() didn't!

**Discovery Process**:
1. User noticed visualization arrows showed correct direction but wrong attack executed
2. User moved visualization from OnInputEventWithTransform to Tick() to see frame-by-frame updates
3. Investigation revealed visualization used GetDirectionalInputFromMovement() which worked correctly
4. Traced back to find VectorToCharacterRelativeDirection() used full camera rotation without flattening
5. Compared both code paths → found pitch corruption in actual capture

**Files Modified**: `CombatTypes.h:596-603` (added camera rotation flattening)

**Behavior Now**:
- ✅ Looking up/down while executing directional attack → correct direction executes
- ✅ Visualization and execution now use identical transformation logic
- ✅ Horizontal directional input independent of camera pitch

---

### 2025-11-19: Diagonal Rotation Bug Fix + DirectionDebugLibrary ✅

**Fixed**: Critical axis mismatch causing directional attacks to fail at diagonal character rotations
**Added**: Comprehensive direction transformation debug utilities

#### Issue 1: Diagonal Character Rotation Bug ⚠️ CRITICAL
**Symptom**: Directional attacks worked at cardinal directions (N/E/S/W) but failed at diagonals (NE/SE/SW/NW)
**Root Cause**: Axis swap in `VectorToCharacterRelativeDirection()` - Unreal Engine X/Y axes incorrectly mapped
**Location**: `CombatTypes.h:547-548`

**The Bug**:
```cpp
// WRONG (before):
const FVector CameraRight = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::X);   // ❌ X is Forward, not Right!
const FVector CameraForward = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y); // ❌ Y is Right, not Forward!
```

**The Fix**:
```cpp
// CORRECT (after):
const FVector CameraForward = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::X); // ✓ X axis = Forward
const FVector CameraRight = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);   // ✓ Y axis = Right
```

**Why Diagonals Failed**: Cardinal rotations (0°/90°/180°/270°) worked by accident due to orthogonal axes. Diagonal rotations (45°/135°/225°/315°) created 90° rotation error, causing wrong direction resolution.

**Files Modified**: `CombatTypes.h:547-548`

#### Issue 2: DirectionDebugLibrary - Comprehensive Debug Utilities ✅

**Added**: New Blueprint Function Library for modular direction transformation debugging

**New Files**:
- `Source/KatanaCombat/Public/Utilities/DirectionDebugLibrary.h` (~145 lines)
- `Source/KatanaCombat/Private/Utilities/DirectionDebugLibrary.cpp` (~175 lines)

**Helper Functions** (6 total):
1. `YawToCardinalDirection()` - Convert yaw to compass direction (N, NE, E, etc.)
2. `FormatRotationDebug()` - Format rotation: "Yaw=45.0° (NE)"
3. `FormatVector2DDebug()` - Format vector: "(X=0.71, Y=0.71) magnitude=1.00"
4. `FormatInputDirectionDebug()` - Format EInputDirection enum
5. `FormatAttackDirectionDebug()` - Format EAttackDirection enum
6. `CalculateYawDelta()` - Shortest angular distance with wrapping
7. `GetMeshRotationOffset()` - Detect mesh rotation offset from actor

**Visual Debug Visualization** (inline in CombatComponentV2.cpp:272-326):
- 5 colored arrows with 3D text labels
- Blue (Camera), Green (Character), Yellow (Input), Orange (CharRelative), Magenta (Resolved)
- Spheres, arrows with proper offsets, 5-second persistence

**Diagnostic Logging** (inline in CombatComponentV2.cpp:332-361):
```
[DIRECTION DIAGNOSTIC]
=== Input ===
Raw Input: (X=1.00, Y=0.00) magnitude=1.00

=== Rotations ===
Camera Yaw:    0.0° (N)
Character Yaw: 270.0° (W)
Yaw Delta:     270.0°
Mesh Offset:   (P=0.0°, Y=0.0°, R=0.0°)

=== Transformation Step 1: Camera-Relative to World ===
CameraForward: (X=1.00, Y=0.00, Z=0.00)
CameraRight:   (X=0.00, Y=1.00, Z=0.00)
WorldInput:    (X=0.00, Y=1.00, Z=0.00)
World Angle:   90.0°

=== Transformation Step 2: World to Character-Relative ===
InverseYaw:    -270.0°
CharRelative:  (X=1.00, Y=0.00, Z=0.00)
CharRelative2D: (X=1.00, Y=0.00) magnitude=1.00
Char Angle:    0.0°

=== Resolution ===
Resolved:      Right
Attack:        Right
Expected:      [VERIFY MANUALLY]
```

**Integration**: Comprehensive inline debug in `OnInputEventWithTransform()` and enhanced logging in directional capture points

**Debug Flag**: Uses existing `CombatSettings->bDebugDraw` (data-driven, no new complexity)

---

### 2025-11-19: Directional Attack Architectural Fix ✅

**Completed**: Full system refactor addressing root cause of all directional attack bugs
- **Problem**: Semantic input conflation (movement = attack input)
- **Solution**: Context-aware input sampling with discrete directional buffer
- **Impact**: Clean separation between movement and attack input, future-proof architecture
- **Duration**: 3-5 days (estimated), implemented in 1 session

**See comprehensive documentation above for full details.**

---

### 2025-11-18: Graceful Fallback Chain (Phase 1) ✅

**Implemented**: Combat system now NEVER breaks due to configuration errors
- **Goal**: Make combat system failure-proof with modular combo chains that degrade gracefully
- **Result**: ✅ Combat always executes an attack, even with circular dependencies or missing tags

**Changes Made** (3 tasks, 4-6 hours):
1. **Cycle Detection Fallback** (`MontageUtilityLibrary.cpp:1006-1017`)
   - Cycle detected → falls back to default attack (not nullptr)
   - Logs error but continues to Priority 4 (default attacks)
   - **Behavior**: Circular reference A→B→C→A → falls back to DefaultLightAttack

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
Priority 4: Default Attacks (DefaultLightAttack/Heavy) → Fallback: Emergency (Tier 5)
Tier 5: Emergency Fallback (repeat current attack) → NEVER returns nullptr
```

**Impact**: Designers can now work freely without fear of breaking combat with configuration errors.

---

### 2025-11-16: Directional Loop Bug Fix ✅

**Fixed**: Infinite loop when holding direction + spamming attack
- **Root Cause**: Queued actions captured stale `LastDirectionalInput` at queue time
- **Solution**: Added `bDirectionalInputConsumed` flag (Option 2 - consumption tracking)
- **Files**: `CombatComponentV2.h:360`, `CombatComponentV2.cpp:119,1968,2047`
- **Behavior**: Each directional input triggers ONE directional follow-up, then falls back to normal combos
- **Reset**: New directional input resets consumption, enabling new directional attack

---

### 2025-11-12: Context-Aware Attack Resolution (Phase 1) ✅

**Goal**: Implement GameplayTag system + cycle detection for context-aware attack resolution
**Result**: ✅ Infrastructure complete (directional bug required separate fix)

**Implemented** (infrastructure valuable, keep):
1. **GameplayTag System** (`AttackData.h:242-256`, `DefaultGameplayTags.ini` NEW)
   - Properties: `FGameplayTagContainer AttackTags/RequiredContextTags`
   - 15 tags: Capabilities (CanCombo, CanDirectional, Terminal), Types (Light/Heavy/Special), Context (ParryCounter, LowHealthFinisher)
   - Designer-friendly, no code changes for new attacks

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

**Why Failed**: Clear signal fires during resolution, but queued actions already captured stale direction at queue time. See Option 1/2 fixes above.

---

### 2025-11-12: Directional Input + Phase Transition Fix ✓

**Fixed**:
1. **Directional Input** (`CombatComponentV2.cpp:1922-1944`)
   - Bug: Hardcoded `EAttackDirection::None` in resolution
   - Fix: Convert `LastDirectionalInput` → `EAttackDirection` via `CombatHelpers::InputToAttackDirection()`

2. **Phase Transition Artifacts** (`CombatComponentV2.cpp:1475-1547`)
   - Bug: `OnMontageEnded()` → `SetPhase(None)` → queued action → `SetPhase(Windup)` → old Active notify fires → desync
   - Fix: Track `bStartedNewAttack` flag, skip None transition if new attack started

---

### 2025-11-11: Universal Combo Crossfade + Critical Bug Fixes ✓

**Implemented**:
1. **Combo Blending** (`AttackData.h:102-110`)
   - Per-attack `ComboBlendOutTime/ComboBlendInTime` (0.1s default, 0.0-1.0s range)
   - Automatic blend on combo transitions (all types: Light→Light, Heavy→Any, Hold→Directional)
   - Tuning examples: Fast (0.05s), Weighty (0.2s)

2. **CRITICAL: Fixed Light Attack Freeze** (`ActionQueueTypes.h:271`, `CombatComponentV2.cpp:947,1096,1151`)
   - Bug: Early release at playrate 0.5 → comparison `0.5 > 0.0` = ease-in → continued to 0.0 (freeze)
   - Fix: Added `bIsEasingOut` flag to explicitly track ease direction

3. **CRITICAL: Fixed Hold Ease Blend Artifacts** (`CombatComponentV2.cpp:1650-1666`)
   - Bug: `ClearHoldState()` cleared timer but left playrate at 0.75 → combo blended in with wrong rate
   - Fix: Restore playrate to 1.0 before clearing hold state

4. **Charge Blend Implementation** (`MontageUtilityLibrary.cpp:468-504`)
   - Bug: `JumpToSectionWithBlend()` instant jump only
   - Fix: `Montage_Stop()` with blend-out + `Montage_PlayWithBlendSettings()` at target section

---

### 2025-11-10: V1/V2 Independence ✓

**Decoupled V1/V2** - No cross-dependencies, clean architecture
- V2 now uses own `PlayAttackMontage()` → direct `AnimInstance->Montage_Play()`
- Removed V1 parameter coupling (`bAllowDuringRecovery`)
- Architecture: `ASamuraiCharacter` → `CombatComponent (V1)` + `CombatComponentV2 (V2)` + `CombatSettings` (shared config)

---

### 2025-11-07: V2 Queue Processing & Execution (Phase 5) ✓

**Implemented**:
- Action execution via V2's `PlayAttackMontage()` (updated 2025-11-10)
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

---

### 2025-11-07: MontageUtilityLibrary Advanced Features (Phase 4d) ✓

**27 Functions** (6 categories):
1. **Procedural Easing**: `EvaluateEasing()`, `EaseLerp()`, `CalculateTransitionPlayRate()` (10 easing types)
2. **Hold Mechanics**: `CalculateChargeLevel()`, `GetMultiStageHoldPlayRate()`, `GetHoldStageIndex()`
3. **Section Utils**: `GetMontageSections()`, `GetSectionStartTime/Duration()`, `JumpToSectionWithBlend()`
4. **Window Queries**: `GetActiveWindows()`, `IsWindowActive()`, `GetWindowTimeRemaining()`, `GetNextCheckpoint()`
5. **Blending**: `CrossfadeMontage()`, `BlendOutMontage()`
6. **Debug**: `DrawCheckpointTimeline()`, `LogCheckpoints()`

**Key Innovation**: Procedural easing (no curve assets needed)
```cpp
float PlayRate = CalculateTransitionPlayRate(1.0f, 0.2f, ElapsedTime, 0.5f, EEasingType::EaseOutQuad);
```

---

### 2025-11-07: Montage Utility Library (Phase 4c) ✓

**Created**: `MontageUtilityLibrary.h/.cpp` - Blueprint Function Library
- Time queries, playback control, checkpoint discovery, timing validation
- Encapsulates null checks (Character→Mesh→AnimInstance→Montage)

---

### 2025-11-07: AttackConfiguration PDA Refactoring ✓

**Three-tier architecture**:
```
ASamuraiCharacter → CombatSettings (combat style) → AttackConfiguration (attack moveset)
    → DefaultLightAttack/DefaultHeavyAttack/SprintAttack/JumpAttack/PlungingAttack
```
**Benefit**: Mix-and-match combat rules with different movesets (e.g., `CombatSettings_Aggressive` + `AttackConfig_Katana` vs `AttackConfig_Greatsword`)

---

## V2 System Status (As of 2025-11-12)

**✅ Implemented**:
1. Input System: Timestamped queue with press/release matching
2. Action Queue: FIFO execution with snap/responsive/immediate modes
3. Phase Management: Event-driven transitions (Windup→Active→Recovery→None)
4. Combo System: Light→Light, Light→Heavy, Heavy branching
5. Hold Mechanics: Light (ease slowdown), Heavy (charge loop) ✅
6. Directional Follow-ups: Fixed infinite loop bug with consumption flag ✅
7. Graceful Fallback Chain: 5-tier system that NEVER breaks combat ✅
8. Blending: Universal crossfade with per-attack blend times
9. Debug Visualization: Phase, queue, timeline, stats
10. Montage Utilities: 27 functions
11. Editor Tools: Custom AttackData panel with validation
12. Context System: GameplayTag resolution with cycle detection

**Robustness** (NEW):
- ✅ Circular references → falls back to default attack (not nullptr)
- ✅ Missing tags → falls back to normal combo chain
- ✅ Missing default attacks → emergency fallback repeats current attack
- ✅ Validation in BeginPlay catches configuration errors early
- ✅ On-screen warnings in editor for missing defaults

**Performance**:
- Timer-based easing (60Hz), not tick-based
- Event-driven queue processing (at checkpoints only)
- Minimal tick overhead

**Tests**: 7 test files, 45+ assertions, all passing

---

## Planned Next Steps

### Phase 2: AttackData Designer QoL (6-8 hours) - HIGH PRIORITY
- **Visual Tag Preview Widget**: Show active tags, context, combo chain at a glance in details panel
- **Comprehensive Tooltips**: Add detailed tooltips to 22+ properties with examples
- **Improved Validation**: Visual indicators for circular references, missing references, tag consistency
- **Test Combo Chain Button**: Simulate resolution without playing to verify fallbacks
- **Property Organization**: Categorize into subcategories, hide irrelevant fields with EditCondition

### Phase 6: Parry & Evade Systems
- Parry detection (check enemy's `AnimNotifyState_ParryWindow`)
- Dodge with i-frames and directional support
- Counter window system with damage multiplier

### Phase 7: Posture Integration
- Posture damage on block (`AttackData->PostureDamage`)
- State-based regeneration (attacking/idle/blocking)
- Guard break stun with vulnerability window

### Phase 8+: Polish
- Hit stop/hitstun, root motion, AI integration, advanced combos, UI/UX

---

## File Locations

```
Source/KatanaCombat/Public/
├── CombatTypes.h                              ← ALL enums, structs, DELEGATES
├── Core/
│   ├── CombatComponent.h                      ← V1 combat hub
│   ├── CombatComponentV2.h                    ← V2 combat hub
│   ├── TargetingComponent.h
│   ├── WeaponComponent.h
│   └── HitReactionComponent.h
├── Data/
│   ├── AttackData.h                           ← Attack config
│   ├── AttackConfiguration.h                  ← Attack moveset PDA
│   └── CombatSettings.h                       ← Global tuning
├── Animation/
│   ├── AnimNotify_AttackPhaseTransition.h     ← Phase transitions (NEW)
│   ├── AnimNotifyState_ParryWindow.h          ← Parry window
│   ├── AnimNotifyState_HoldWindow.h           ← Hold window
│   ├── AnimNotifyState_ComboWindow.h          ← Combo window
│   ├── AnimNotifyState_AttackPhase.h          ← DEPRECATED
│   └── AnimNotify_ToggleHitDetection.h        ← DEPRECATED (automatic now)
└── Utilities/
    └── MontageUtilityLibrary.h                ← 27 montage utilities

Config/
└── DefaultGameplayTags.ini                    ← 15 combat tags (NEW)
```

---

## Common Tasks

| Task | Documentation |
|------|--------------|
| System architecture | `docs/SYSTEM_PROMPT.md` |
| Quick lookups | `docs/ARCHITECTURE_QUICK.md` |
| Complex features | `docs/ARCHITECTURE.md` |
| Add attack | `docs/ATTACK_CREATION.md` |
| Migrate phases | `docs/PHASE_SYSTEM_MIGRATION.md` |
| API reference | `docs/API_REFERENCE.md` |
| Debugging | `docs/TROUBLESHOOTING.md` |
| Setup | `docs/GETTING_STARTED.md` |
| Testing | `Source/KatanaCombatTest/README.md` |

---

## Intelligent Infrastructure (Context + Agents)

**KatanaCombat uses TWO complementary AI systems for intelligent session management:**

### 1. Context Mode System
**Purpose**: Auto-switches documentation focus based on file type, conversation, and learned patterns
**Status**: ✅ **ACTIVE** (v4.0 - Unified with Agent Routing)

### Slash Commands

| Command | Description |
|---------|-------------|
| `/mode status` | View current mode, auto-switch status, and recent switches |
| `/mode [name]` | Manually switch to specific mode (full/combat-logic/animation/data-assets/testing) |
| `/mode suggest` | Get intelligent mode recommendation for current context |
| `/mode analyze [file]` | Show detailed confidence breakdown with visual report |
| `/mode config` | View/edit confidence thresholds and factor weights |
| `/mode learn` | View machine learning patterns and accuracy stats |
| `/mode train` | Manually record pattern for ML training |
| `/mode reset-learning` | Clear all learned patterns (requires confirmation) |
| `/mode auto enable` | Enable automatic mode switching |
| `/mode auto disable` | Disable automatic mode switching |

### Detection Algorithm (5 Factors)

**Factor Weights** (configurable via `/mode config`):
1. **File Patterns** (35%) - Path, extension, naming conventions
2. **Conversation Topics** (25%) - Keywords from recent messages
3. **Learned Patterns** (20%) - Bayesian inference + temporal decay + correlation boost
4. **Historical Success** (15%) - Per-file and per-mode accuracy tracking
5. **Time-Based Patterns** (5%) - Work hours heuristic

**Confidence Levels**:
- **High (≥80%)**: Auto-switch with silent notification
- **Medium (≥50%)**: Auto-switch with visible notification
- **Low (<30%)**: Show hint only, no auto-switch

**Presets** (apply via `/mode config`):
- `conservative` - High: 90%, Medium: 70%, Low: 50%
- `balanced` (default) - High: 80%, Medium: 50%, Low: 30%
- `aggressive` - High: 70%, Medium: 40%, Low: 20%

### Visual Confidence Report

Use `/mode analyze` to see detailed breakdown:
```
================================================================
      INTELLIGENT MODE DETECTION - CONFIDENCE REPORT
================================================================

SUGGESTED MODE: ANIMATION
CONFIDENCE:     54.5% (medium)

CONFIDENCE BAR: ###########################-----------------------

+-------------------------------------------------------------+
|  FACTOR CONTRIBUTIONS                                       |
+-------------------------------------------------------------+
|  file          #############------- 0.332  |
|    -> conf: 95% x weight: 35% -> animation                  |
|  learning      #-------------------  0.027  |
|    -> conf: 13.5% x weight: 20% -> animation                |
|  history       ###############-----  0.15   |
|    -> conf: 100% x weight: 15% -> animation                 |
|  time          #-------------------  0.035  |
|    -> conf: 70% x weight: 5% -> animation                   |
+-------------------------------------------------------------+
```

### Configuration Files

- **`.claude/.context-config.json`** - Thresholds, weights, presets
- **`.claude/.context-learning.json`** - ML learning database (Bayesian inference)
- **`.claude/.context-history.json`** - Switch history and analytics

### Implementation

**Primary Detector**: `.claude/scripts/intelligent-mode-detector.ps1` (5-factor ML)
**Fallback Chain**: intelligent → holistic → detect-mode → exit (graceful degradation)
**Hook Integration**: `.claude/hooks/auto-context.ps1` (runs on file open)

**Test Coverage**: 21 test scripts in `.claude/scripts/tests/`
- Integration tests: 3/3 passing (100%)
- Bayesian inference: Validated
- Correlation boost: Validated

### Tuning Example

Increase learning factor weight:
```
/mode config
> set weight learning 0.30
> set weight file 0.30
```

Apply aggressive preset:
```
/mode config
> apply aggressive
```

---

### 2. Agent Coordination System
**Purpose**: Routes complex tasks to specialist subagents for multi-file work and architecture-critical changes
**Status**: ✅ **ACTIVE** (Integrated with Context Modes v4.0)

### Available Specialist Agents

| Agent | Model | Specialty |
|-------|-------|-----------|
| **router** | inherit | Meta-agent that intelligently routes tasks to specialists |
| **ue-code-generator** | opus | Generates production-ready UE5.6 C++ code with compliance |
| **design-compliance-auditor** | opus | Enforces design principles and detects violations |
| **code-auditor** | inherit | Reviews code for standards adherence and optimization |
| **pipeline-feature** | opus | Orchestrates full feature delivery pipeline |
| **pipeline-bugfix** | opus | Coordinates systematic bug diagnosis and resolution |

### Predefined Pipelines

**Feature Pipeline** (`pipeline-feature`):
```
ue-code-generator → design-compliance-auditor → code-auditor
(Implement → Validate → Audit)
```

**Bugfix Pipeline** (`pipeline-bugfix`):
```
design-compliance-auditor → ue-code-generator → code-auditor
(Diagnose → Fix → Verify)
```

### When to Use Agents

**Use agents for**:
- Multi-file changes (3+ files)
- New system implementation
- Architecture-sensitive refactoring
- Critical bug fixes requiring validation

**Skip agents for**:
- Simple edits (single file, isolated changes)
- Documentation updates
- Quick prototyping
- Minor tweaks

### Integration with Context Modes

Each context mode recommends specific agents:

- **Animation Mode** → ue-code-generator, design-compliance-auditor
- **Combat Logic Mode** → design-compliance-auditor, pipeline-feature (CRITICAL)
- **Data Assets Mode** → ue-code-generator, code-auditor
- **Testing Mode** → ue-code-generator, code-auditor

**These recommendations appear automatically when auto-context switches modes.**

### Agent Commands

| Command | Description |
|---------|-------------|
| `/agent` or `/agent status` | Show all available agents and pipelines |
| `/agent info [pipeline]` | Show pipeline details (feature, bugfix, etc.) |
| `/agent validate [chain]` | Validate agent chain before launching |

---

## Quick Troubleshooting

**Attacks not executing**: Check `GetCombatState()` == Idle, `DefaultLightAttack` assigned, `AnimInstance` valid
**Combos not chaining**: Check `AnimNotifyState_ComboWindow` in montage, `NextComboAttack` set
**Hits not detecting**: Check weapon sockets (`WeaponStart/WeaponEnd`), `AnimNotify_AttackPhaseTransition(Active)` present (automatic hit detection)
**Parry not working**: `AnimNotifyState_ParryWindow` on ATTACKER's montage, defender calls `IsInParryWindow()` on enemy

**Debug Visualization**:
```cpp
CombatComponent->bDebugDraw = true;      // State, phases, windows
TargetingComponent->bDebugDraw = true;   // Cones, targets
WeaponComponent->bDebugDraw = true;      // Traces, hits
```

---

## Common Mistakes (Avoid)

❌ Hold/ParryWindow as attack phases (they're windows)
❌ Gating input with combo window (input always buffered)
❌ Tracking hold duration (check button state at window start)
❌ ParryWindow on defender animation (goes on attacker)
❌ Declaring delegates in component headers (use `CombatTypes.h`)
❌ TArray for cancel inputs (use bitmask)

---

## Session Start Checklist

**Before Coding**:
- [ ] Read `docs/SYSTEM_PROMPT.md` (10 min)
- [ ] Skim `docs/ARCHITECTURE_QUICK.md` (3 min)
- [ ] Understand: Phases vs Windows, Input Always Buffered, Parry = Defender-Side, Hold = Button State

**During Coding**:
- [ ] Reference `ARCHITECTURE_QUICK.md` for values
- [ ] Check `API_REFERENCE.md` for signatures
- [ ] Enable debug visualization

**When Explaining**:
- [ ] Use `file:line` references
- [ ] Show ASCII diagrams
- [ ] Explain design decisions

---

## Environment Notes

### GPU Crash Fix (RTX 5090 + UE 5.6)
**Issue**: Driver 581.57 + DX12 causes crashes during batch animation operations
**Fix Applied**: Switched to DX11 (`Config/DefaultEngine.ini:47`)
**Revert When**: Install Studio Driver 580.97 (stable) → change `DefaultGraphicsRHI_DX11` → `DefaultGraphicsRHI_DX12`
**Impact**: ~5-10% editor viewport slowdown (packaged games unaffected)

### Build Configuration (2025-11-03)
**Issue**: Marketplace plugin name conflicts (StateMachineSystem vs UFSM, etc.)
**Fix**: Disabled 14 conflicting plugins in `KatanaCombat.uproject:53-109`
**Enabled Plugins Only**: ModelingToolsEditorMode, StateTree, GameplayStateTree, MotionWarping
**Build Method**: Use Unreal Editor (not Rider directly) - RiderLink requires intact engine structure

---

## Coding Guidelines

**DO**:
- Encapsulate repetitive calls (reduce bloat)
- Overhaul existing code (don't create "_V2" variants)
- Update existing docs (don't create redundant files)
- Use timers over tick (minimize tick overhead)
- Maintain component separation (4 components intentional)
- Preserve Blueprint exposure (`UFUNCTION(BlueprintCallable)`)

**DON'T**:
- Create duplicate functions with suffixes
- Use deprecated features (`AnimNotifyState_AttackPhase`, `AnimNotify_ToggleHitDetection`)
- Assume `FGeometry::GetRenderTransform()` exists (UE 5.6 removed it)
- Convert `FLinearColor` to `FColor` directly (use `.ToFColor(true)`)
- Use component tick without explicit permission

---

**Test Coverage**: 96% design specification compliance (validated via audit)
**Build Status**: ✓ Succeeded | ⚠️ 1 known bug (directional loop, workaround available)
**Quality**: 7 test files, 45+ assertions passing

**Ready to code!** 🗡️