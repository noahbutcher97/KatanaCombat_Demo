# Claude Code - KatanaCombat Project

**AI assistant onboarding guide for Ghost of Tsushima-inspired melee combat system (UE 5.6, C++).**

---

## ⚠️ CRITICAL: Known Bug - Directional Follow-Up Infinite Loop

**Status**: V2 only, workaround available (use V1: `bUseV2System = false`)

**Symptom**: Holding direction + spamming attack causes infinite loop of same directional attack

**Root Cause**: Action queue captures stale `LastDirectionalInput` at queue time
- Timeline: T=0s input queued → T=0.1s input queued (captures SAME direction) → T=0.5s clear signal fires (too late, queue already has stale data)
- Each `FActionQueueEntry` is independent snapshot; clearing component state doesn't affect queued actions

**Recommended Fix** (Option 2, ~1 hour):
```cpp
// CombatComponentV2.h - Add consumption flag
bool bDirectionalInputConsumed = false;

// CombatComponentV2.cpp - OnInputEvent()
if (InputDirection != EInputDirection::None)
{
    LastDirectionalInput = InputDirection;
    bDirectionalInputConsumed = false;  // Reset on new input
}

// CombatComponentV2.cpp - GetAttackForInput()
EAttackDirection AttackDirection = EAttackDirection::None;
if (!bDirectionalInputConsumed && LastDirectionalInput != EInputDirection::None)
{
    AttackDirection = CombatHelpers::InputToAttackDirection(LastDirectionalInput);
}
// ... resolution ...
if (Result.Path == EResolutionPath::DirectionalFollowUp)
{
    const_cast<UCombatComponentV2*>(this)->bDirectionalInputConsumed = true;
}
```

**Alternative Fix** (Option 1, ~2 hours, more robust):
- Store `EInputDirection CapturedDirection` + `bool bDirectionConsumed` in `FActionQueueEntry`
- Clear ALL pending queue entries' directions when directional executes
- Prevents all stale state issues (not just directional)

**Files**: `CombatComponentV2.h/cpp`, `ActionQueueTypes.h` (Option 1 only)
**Details**: See "Attempted Fix Analysis" section below

---

## Instant Context

**Project**: 4-component combat system, pragmatic design, data-driven tuning

**Core Identity** (MEMORIZE):
1. **Phases vs Windows**: Phases exclusive (Windup→Active→Recovery), Windows overlap (ParryWindow, ComboWindow, HoldWindow)
2. **Input ALWAYS Buffered**: Combo window modifies WHEN, not WHETHER
3. **Parry = Contextual Block**: Defender checks enemy's ParryWindow (attacker's montage)
4. **Hold = Button State Check**: At window start, NOT duration tracking
5. **Delegates in CombatTypes.h**: Declared ONCE, components use `UPROPERTY` only

**Essential Docs**:
- `docs/SYSTEM_PROMPT.md` (25 KB, 10 min) - **READ FIRST** before any work
- `docs/ARCHITECTURE_QUICK.md` (8 KB, 3 min) - Keep open while coding
- `docs/ARCHITECTURE.md` (52 KB) - Deep dive for complex features

**Default Values**: ComboInputWindow 0.6s | ParryWindow 0.3s | ComboBlendOut/In 0.1s | MaxPosture 100.0f | LightDamage 25.0f | HeavyDamage 50.0f

---

## Recent Changes (Reverse Chronological)

### 2025-11-12: Context-Aware Attack Resolution (Phase 1 - INCOMPLETE) ⚠️

**Goal**: Fix directional loop bug with GameplayTag system + cycle detection
**Result**: ❌ Bug persists (see CRITICAL section above)

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
5. Hold Mechanics: Light (ease slowdown), Heavy (charge loop) ✅ | Directional follow-ups ❌ (bug)
6. Blending: Universal crossfade with per-attack blend times
7. Debug Visualization: Phase, queue, timeline, stats
8. Montage Utilities: 27 functions
9. Editor Tools: Custom AttackData panel with validation
10. Context System: GameplayTag resolution with cycle detection

**Performance**:
- Timer-based easing (60Hz), not tick-based
- Event-driven queue processing (at checkpoints only)
- Minimal tick overhead

**Tests**: 7 test files, 45+ assertions, all passing

---

## Planned Next Steps

### URGENT: Fix Directional Loop Bug (~1-2 hours)
See CRITICAL section above for Option 1 (per-action storage) or Option 2 (consumption flag)

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