# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**KatanaCombat** is a cinematic free-flow melee combat system (AC3/4 + Batman Arkham) for Unreal Engine 5.6 (C++). The system features:
- 5-component architecture (Combat, Targeting, Weapon, HitReaction, PairedAnimation)
- Hybrid combo system (responsive input buffering + snappy animation cancels + procedural blending)
- Contextual stagger defense with counter system (AC3 mode + Chain mode)
- Data-driven attack configuration via AttackData assets
- Per-hit impact effects (hitstop, audio, VFX) with pooled FX data assets
- Death system with directional animations and ragdoll transitions
- Comprehensive automation suite with current counts reported by the standard baseline runner

## Build & Development

**Build** (In Editor):
- Open `KatanaCombat.uproject` in Unreal Engine 5.6
- Build via Build > Compile or Ctrl+Alt+F11

**Build** (Command Line):
```powershell
# Navigate to UE5.6 Engine\Source directory and run UnrealBuildTool
cd "C:\Program Files\Epic Games\UE_5.6\Engine\Source"
dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" KatanaCombatEditor Win64 Development "-Project=D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -WaitMutex
```

**Run Tests** (In Editor):
1. Window → Developer Tools → Session Frontend
2. Automation tab → Filter: "KatanaCombat"
3. Select tests and click "Start Tests"

**Run Tests** (Command Line, preferred):
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "Tools\Codex\run-agent-baseline.ps1"
```

The baseline builds `KatanaCombatEditor`, runs `Automation RunTests KatanaCombat` with `;Quit`, writes timestamped evidence under `Saved/Logs/`, and exits nonzero on detected build or test failure. Direct `UnrealEditor-Cmd.exe` runs may still fail to exit cleanly; inspect the log rather than treating a lingering process as proof of failure.

**Test Results**: Check the log file at `D:\UnrealProjects\5.6\KatanaCombat\Saved\Logs\KatanaCombat.log`
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ".agents\skills\katana-verify\scripts\summarize-automation-log.ps1"
```

**Debug Visualization** (CVar-controlled, use console commands):
```
Combat.Debug.All 1              // Enable all debug visualization
Combat.Debug.Direction 1        // Direction transformation arrows
Combat.Debug.Targeting 1        // Targeting cones and targets
Combat.Debug.Weapon 1           // Weapon trace visualization
Combat.Debug.Phase 1            // Attack phase indicators
Combat.Debug.Environment 1      // Terrain/slope visualization
Combat.Debug.Queue 1            // Action queue state
Combat.Debug.Hold 1             // Hold state visualization
Combat.Debug.DrawDuration 2.0   // Debug shape persistence (seconds)

// Paired Animation Debug (finishers, counters)
Combat.Debug.PairedAnim 1             // Enable all paired animation debug
Combat.Debug.PairedAnim.Warp 1        // Warp targets (cyan crosshairs)
Combat.Debug.PairedAnim.Partners 1    // Partner connections (yellow lines)
Combat.Debug.PairedAnim.Sync 1        // Sync points (magenta spheres)
Combat.Debug.PairedAnim.Vulnerability 1 // Finisher vulnerability indicators
```

## Core Architecture Principles

**MEMORIZE these 6 design rules**:

1. **Phases vs Windows**: Phases are exclusive (Windup→Active→Recovery). Windows overlap (ParryWindow, ComboWindow, HoldWindow).

2. **Input ALWAYS Buffered**: Combo window modifies WHEN execution happens, not WHETHER input is captured.

3. **Parry = Contextual Block**: Defender checks enemy's ParryWindow (on attacker's montage), not their own.

4. **Hold = Button State Check**: At window start, check if button is STILL held. NOT duration tracking.

5. **Movement ≠ Attack Input**: Direction sampled ONLY at hold release (context-aware), never continuously from movement stick.

6. **Delegates (Two-Tier Rule)**: Cross-component delegates (used by multiple components or external systems) declared in CombatTypes.h. Component-internal delegates (only meaningful within one component) stay in that component's header. Components use `UPROPERTY` for delegate members.

7. **Semantics Ownership**: Enums own closed runtime state and results; booleans own local latches or direct authored gates; gameplay tags own open-ended authored capabilities, properties, and context requirements; data references own the concrete animation, VFX, audio, or paired-data payload. Gameplay-relevant tags must be consumed by runtime resolution and validation before content can rely on them.

## File Structure

```
Source/KatanaCombat/Public/
├── CombatTypes.h              ← ALL enums, structs, system-wide delegates
├── Core/
│   ├── CombatComponent.h           ← Combat state, attack execution, last-input-wins queue
│   ├── PairedAnimationComponent.h  ← Finishers, counters, partner tracking, input blocking
│   ├── TargetingComponent.h        ← Soft-lock targeting, aim assist
│   ├── WeaponComponent.h           ← Hit detection, weapon state
│   └── HitReactionComponent.h      ← Damage reception, hit reactions, death
├── Data/
│   ├── AttackData.h           ← Attack configuration asset
│   ├── AttackConfiguration.h  ← Attack moveset package (PDA)
│   ├── CombatSettings.h       ← Global tuning values
│   └── HitReactionSettings.h  ← Hit reaction configuration
├── Animation/
│   ├── AnimNotify_AttackPhaseTransition.h     ← Phase transitions
│   ├── AnimNotify_HoldWindowStart.h           ← Event-driven hold activation
│   ├── AnimNotifyState_CounterWindow.h        ← Counter window (pose-matching metadata)
│   ├── AnimNotifyState_ParryWindow.h
│   ├── AnimNotifyState_HoldWindow.h           ← Legacy; do not seed by default
│   ├── AnimNotifyState_ComboWindow.h          ← Legacy/manual override; do not seed by default
│   ├── AnimNotifyState_PairedAnimationSync.h  ← Sync point effects trigger
│   └── AnimNotifyState_PairedAnimationCollision.h ← Partner collision management
├── Characters/
│   ├── BaseCombatCharacter.h  ← Base class with 5 combat components
│   ├── PlayerCharacter.h      ← Player-specific combat
│   └── EnemyCharacter.h       ← Enemy-specific combat
├── Interfaces/
│   ├── DamageableInterface.h  ← Damage/health contract
│   ├── CombatInterface.h      ← Combat state contract
│   └── TeamMemberInterface.h  ← Team/faction contract
├── Math/
│   ├── CombatMathEnums.h      ← 10 enums (distance formulas, bone chains, contact types)
│   └── CombatMathTypes.h      ← 10 structs (skeletal hierarchy, reach, contact predictions)
└── Utilities/
    ├── MontageUtilityLibrary.h           ← 27 montage utility functions
    ├── PairedAnimationUtilityLibrary.h   ← 15 functions (validation, contact points)
    ├── CinematicEffectsUtilityLibrary.h  ← Time dilation, hitstop, camera shake
    ├── SkeletalAnalysisLibrary.h         ← 18 functions (bone chains, reach envelopes)
    ├── GeometryMathLibrary.h             ← 20 functions (distance, bounding volumes)
    ├── SpatialQueryLibrary.h             ← 15 functions (sphere/box/cone queries)
    └── PhysicsIntegrationLibrary.h       ← 15 functions (Verlet, trajectory prediction)
```

## Key Default Values

| Parameter | Value | Notes |
|-----------|-------|-------|
| ComboInputWindow | 0.6s | |
| ParryWindow | 0.3s | |
| ComboBlendOut/In | 0.1s | Per-attack tunable |
| MaxPosture | 100.0f | DEPRECATED - use contextual stagger |
| LightBaseDamage | 25.0f | |
| HeavyBaseDamage | 50.0f | |
| CounterDamageMultiplier | 1.5x | |

## Documentation

### Combat System Docs (`docs/`)

| Task | Documentation |
|------|--------------|
| **Quick reference** | `docs/architecture/ARCHITECTURE_QUICK.md` (start here for deeper context) |
| Deep dive | `docs/architecture/ARCHITECTURE.md` |
| Add new attack | `docs/guides/ATTACK_CREATION.md` |
| API reference | `docs/architecture/API_REFERENCE.md` |
| Paired animation spec | `docs/specs/PAIRED_ANIMATION_SPEC.md` |
| Debugging | `docs/guides/TROUBLESHOOTING.md` |
| Change history | `docs/reference/CHANGELOG.md` (bug fixes, feature history) |
| Future plans | `docs/reference/ROADMAP.md` (planned features, system status) |
| Implementation plans | `docs/plans/` (active and archived feature plans) |
| Audit findings | `docs/audits/AUDIT_SYNTHESIS_2026-02-03.md` (unified audit synthesis) |

### AI Infrastructure Docs (`.claude/`)

| Task | Documentation |
|------|--------------|
| Navigation hub | `.claude/INDEX.md` (start here for AI tooling) |
| Slash commands | `.claude/commands/README.md` |
| Specialist agents | `.claude/agents/README.md` |
| Context modes | `.claude/context-modes/README.md` |
| Hooks system | `.claude/hooks/README.md` |
| Infrastructure changelog | `.claude/CHANGELOG.md` |

### Documentation Hierarchy

This project uses a four-level documentation hierarchy optimized for Claude CLI:

**Level 1 - CLAUDE.md (Working Memory)**
Essential rules, patterns, and quick references loaded automatically for every interaction. Keep this file focused on actionable knowledge.

**Level 2 - Specification Files (`docs/specs/`)**
Detailed technical specifications for major systems. Read when working on that specific system.

**Level 3 - Architecture Docs (`docs/architecture/`)**
Deep dives into component design and API details. Read when understanding or modifying architecture.

**Level 4 - Implementation Plans (`docs/plans/`)**
Active development plans with gap tracking. Read when continuing phased implementation work.

| Need | Start Here |
|------|------------|
| Quick combat system rules | `CLAUDE.md` (this file) |
| Paired animation spec | `docs/specs/PAIRED_ANIMATION_SPEC.md` |
| Component architecture | `docs/architecture/ARCHITECTURE.md` |
| API details | `docs/architecture/API_REFERENCE.md` |
| Active plan status | `docs/plans/gap-tracker.md` |
| Troubleshooting | `docs/guides/TROUBLESHOOTING.md` |
| Audit findings | `docs/audits/AUDIT_SYNTHESIS_2026-02-03.md` |

## Common Mistakes to Avoid

- Hold/ParryWindow as attack phases (they're windows, not phases)
- Gating input with combo window (input always buffered)
- Tracking hold duration (check button state at window start)
- ParryWindow on defender animation (goes on attacker's montage)
- Declaring cross-component delegates in component headers (use CombatTypes.h for cross-component delegates; component-internal delegates stay in the component header)
- Using TArray for cancel inputs (use bitmask)
- Calling `BlueprintNativeEvent` interface methods directly (use `Execute_` pattern):
  ```cpp
  // WRONG (crashes): Character->GetCombatState();
  // CORRECT: ICombatInterface::Execute_GetCombatState(Character);
  ```

## Troubleshooting

**Attacks not executing**: Check `GetCombatState()` == Idle, `DefaultLightAttack` assigned, `AnimInstance` valid

**Combos not chaining**: Check `AnimNotify_AttackPhaseTransition` Active/Recovery timing, `NextComboAttack`/`HeavyComboAttack`, and whether `CurrentAttackData` is still valid. Default combo timing is inferred from phase transitions; do not add `AnimNotifyState_ComboWindow` for normal attacks.

**Hits not detecting**: Check weapon sockets (`WeaponStart/WeaponEnd`), `AnimNotify_AttackPhaseTransition(Active)` present

**Parry not working**: `AnimNotifyState_ParryWindow` must be on ATTACKER's montage, defender calls `IsInParryWindow()` on enemy

## Coding Guidelines

**CRITICAL: THOROUGH SOLUTIONS OVER QUICK FIXES — EVEN AT THE EXPENSE OF TIME**

ALWAYS prefer the more complete, well-architected implementation over shortcuts. This is a firm user preference: the thorough solution is always preferred, even when it takes significantly longer to implement.

- **Philosophy**: Time spent on proper implementation now saves exponentially more time debugging mysterious side effects later. Quick fixes tend to compound into technical debt that becomes increasingly painful to unravel.
- **Example (Collision)**: Use a tracked partner array with `IgnoreActorWhenMoving()` (supports multi-partner kills, easier debugging) over global pawn collision disable (`ECR_Ignore` on `ECC_Pawn`).
- **Example (Timing)**: Use `FPlatformTime::Seconds()` with `FTSTicker` for accurate real-time tracking instead of `GetTimerManager().SetTimer()` which is affected by time dilation.
- **When in doubt**: Choose the approach that handles more edge cases, provides clearer debugging information, and doesn't rely on approximations when accurate solutions exist.

**CRITICAL: EXPLORE BEFORE IMPLEMENTING**

Before implementing code that interacts with existing systems, ALWAYS launch an exploratory agent to gather full context about:
- The actual APIs available on components (method names, parameters, return types)
- How existing patterns work in similar code
- What properties/members exist vs. what you assume exists

This prevents implementation errors from incorrect API assumptions. Examples:
- `TargetingSettings` member doesn't exist - use `GetEffectiveSettings()` instead
- Combat state is queried via interface, not component

When touching unfamiliar code: **Explore first, implement second**.

**UE5 INTERFACE CALL PATTERN (BlueprintNativeEvent)**

When calling interface methods marked as `BlueprintNativeEvent`, you MUST use the `Execute_` static pattern, NOT direct method calls. Direct calls will crash at runtime.

```cpp
// INTERFACE DEFINITION (CombatInterface.h):
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
ECombatState GetCombatState() const;

// WRONG - Will compile but CRASHES at runtime:
ECombatState State = Character->GetCombatState();

// CORRECT - Use Execute_ static method:
ECombatState State = ICombatInterface::Execute_GetCombatState(Character);
```

This applies to ALL `BlueprintNativeEvent` interface methods:
- `ICombatInterface::Execute_GetCombatState(Actor)`
- `ICombatInterface::Execute_CanPerformAttack(Actor)`
- `IDamageableInterface::Execute_GetHealth(Actor)`
- etc.

**Why**: `BlueprintNativeEvent` creates a virtual thunk that routes to either C++ `_Implementation()` or Blueprint override. Direct calls bypass this routing and crash.

**DO**:
- Use timers over tick (minimize tick overhead)
- Maintain 5-component separation (Combat, Targeting, Weapon, HitReaction, PairedAnimation)
- Preserve Blueprint exposure (`UFUNCTION(BlueprintCallable)`) for PUBLIC API functions
- Update existing files (don't create "_V2" variants)

**DON'T**:
- Create duplicate functions with suffixes
- Use deprecated default-seeding features (`AnimNotifyState_AttackPhase`, `AnimNotify_ToggleHitDetection`, `AnimNotifyState_HoldWindow`, `AnimNotifyState_ComboWindow`)
- Seed holdable attacks with `AnimNotifyState_HoldWindow`; use `AnimNotify_HoldWindowStart` for event-driven hold activation
- Assume `FGeometry::GetRenderTransform()` exists (UE 5.6 removed it)
- Convert `FLinearColor` to `FColor` directly (use `.ToFColor(true)`)
- Use component tick without explicit permission
- **Make internal state variables `BlueprintReadOnly`**: If a parameter isn't meaningful to view/edit at runtime in the editor, don't expose it to Blueprint. This adds visual load and confusion. Reserve Blueprint visibility for intentional public API, not internal implementation details.

## Editor Tool Architecture Patterns

**CRITICAL: These patterns MUST be followed for all editor tooling in KatanaCombatEditor module.**

### Three-Layer Separation

Editor tools follow a strict three-layer architecture:

| Layer | Purpose | File Location | Contains |
|-------|---------|---------------|----------|
| **Types Layer** | Pure data definitions | `Data/PairedAnimationEditorTypes.h` | USTRUCT, UENUM, simple inline accessors |
| **Library Layer** | Stateless pure math | `PairedAnimationAnalysisLibrary.h/.cpp` | Static functions taking/returning primitives |
| **Subsystem Layer** | UObject state management | `PairedAnimationAnalysisSubsystem.h/.cpp` | UEditorSubsystem with UObject references |

### Rules by Layer

**Types Layer (`*Types.h`)**:
- ✅ USTRUCT and UENUM definitions
- ✅ Simple inline accessors (getters/setters)
- ✅ Factory methods (`CreateDefault()`, `CreateForRelationship()`)
- ✅ TWeakObjectPtr for UObject references (requires direct assignment)
- ❌ NO complex calculations (extract to Library)
- ❌ NO #include of Library headers (causes circular dependency)

**Library Layer (`*Library.h/.cpp`)**:
- ✅ Static BlueprintPure functions ONLY
- ✅ Input: primitives, enums, pure data structs
- ✅ Output: primitives, enums, pure data structs
- ✅ NO side effects, NO state
- ❌ NO UObject references (no USkeletalMeshComponent*, no AActor*)
- ❌ NO member variables

**Subsystem Layer (`*Subsystem.h/.cpp`)**:
- ✅ UEditorSubsystem base class
- ✅ UObject references (mesh components, montages, actors)
- ✅ Calls Library functions for math
- ✅ Manages editor-time state
- ❌ NO struct/enum definitions (put in Types)
- ❌ NO complex inline calculations (extract to Library)

### Example: Correct Pattern

```cpp
// Types file - pure data struct with factory
USTRUCT(BlueprintType)
struct FRotationConstraint
{
    float TargetYaw;
    float Tolerance;

    static FRotationConstraint CreateForRelationship(ESpatialRelationship Relationship);
    bool IsWithinConstraint(float TestYaw) const; // Simple inline OK
};

// Library file - pure stateless math
UCLASS()
class UAnalysisLibrary : public UBlueprintFunctionLibrary
{
    UFUNCTION(BlueprintPure)
    static float CalculateConfidence(float AngleDegrees);

    UFUNCTION(BlueprintPure)
    static bool IsYawWithinConstraint(float TargetYaw, float Tolerance, float TestYaw);
};

// Subsystem file - UObject operations
UCLASS()
class UAnalysisSubsystem : public UEditorSubsystem
{
    // Calls Library functions, passes results to/from UObjects
    FAnalysisResult AnalyzeMontage(UAnimMontage* Montage);
};
```

### Why This Matters

1. **Testability**: Library functions can be unit tested without UObject setup
2. **Reusability**: Pure math works in runtime, editor, or tests
3. **Maintainability**: Clear ownership - calculations in one place, state in another
4. **Compile Times**: Types file changes don't require recompiling Library
5. **Circular Dependencies**: Prevented by strict include hierarchy

### Synchronization Requirements

When struct methods duplicate Library logic (due to circular dependency prevention), add documentation:
```cpp
// Note: Logic synchronized with UAnalysisLibrary::IsYawWithinConstraint()
// If modifying, update both locations
bool IsWithinConstraint(float TestYaw) const { ... }
```

## Claude CLI Best Practices

### Session Continuity
- **Plan files persist**: Check `docs/plans/` for active work from previous sessions
- **CLAUDE.md is working memory**: This file provides context loaded automatically for every session
- **Use specs for detail**: Store detailed specifications in `docs/specs/` to keep CLAUDE.md scannable

### Exploration Before Implementation
- **ALWAYS use Explore agents** before modifying unfamiliar code
- **Verify actual API signatures** - don't assume method names or parameters exist
- **Check existing patterns** in similar components before implementing new features

### Code Quality Standards
- **Thorough solutions over quick fixes** (even at time expense) - see Coding Guidelines
- **Event-driven over tick-based** where possible for performance
- **Blueprint exposure only for intentional public API** - not internal state
- **Null checks on all weak references** and component accesses

### Documentation Updates
- **Update CLAUDE.md** when design decisions or architecture changes
- **Update specs** when implementation deviates significantly from spec
- **Archive completed plans** to `docs/plans/archive/` with date suffix

## Git Conventions

- **Clean commit messages**: No trailers, sign-offs, or co-author tags - just the message and content
- Include rollback checkpoint (previous commit hash) in significant commits
- Use descriptive commit messages with bullet points for changes
- Bypass pre-commit hooks with `--no-verify` if they have errors (hooks in `.claude/hooks/` may have issues)

## Active Development & System Status

Track ongoing work across sessions. This section provides detailed status of all major systems.

### Paired Animation System - PRIMARY FOCUS

**Overall Status**: ~60% complete | Component extracted, core flow implemented, animations needed

> The Paired Animation System (finishers, counters, parries) is the heart and soul of this project.
> UPairedAnimationComponent extracted from CombatComponent (Phase 3 complete). Math libraries complete.
> Core combat flow (parry, counter, finisher chain) needs animation assets to become playable.

#### Phase 5c: Math & Utility Libraries - COMPLETE (83 functions, 3,128 lines)

| Library | Functions | Lines | Key Capabilities |
|---------|-----------|-------|------------------|
| SkeletalAnalysisLibrary | 18 | 814 | Bone chains, reach envelopes, center of mass |
| GeometryMathLibrary | 20 | 499 | Distance calculations (5 formulas), bounding volumes |
| SpatialQueryLibrary | 15 | 706 | Sphere/box/cone queries, FOV checks |
| PhysicsIntegrationLibrary | 15 | 610 | Verlet integration, trajectory prediction |
| PairedAnimationUtilityLibrary | 15 | 499 | Contact points, obstacle validation |

#### Phase 5d: Preview Tool Enhancements - Foundation Complete, Ongoing (6,000+ lines)

| Feature | Status | Description |
|---------|--------|-------------|
| PT-1: Holistic Optimization | ✅ | Consistent results across frames using Time parameter |
| PT-2: Spatial Relationship | ✅ | Inference from neutral configuration |
| PT-3/PT-4: Bone Trajectories | ✅ | Chain visualization with time gradient |
| PT-5: Individual Bone Selection | ✅ | Hierarchy tree for bone selection |
| PT-6/PT-7: Joint Constraints | ✅ | Anatomical limits framework |
| PT-8/PT-9: Weapon Attachment | ✅ | Configurable grip sockets, mesh visualization |

**Latest Commit**: 3beddea - Weapon grip socket + montage section looping fix

#### Fully Implemented (Production Ready)
| Component | Files | Description |
|-----------|-------|-------------|
| Finisher Execution Flow | PairedAnimationComponent.cpp | `TryExecuteFinisher()` → `CompletePairedAnimation()` |
| Finisher Vulnerability | HitReactionComponent.h/.cpp | `IsVulnerableToFinisher()`, `GetFinisherTriggerReason()` |
| Symmetric Warp Tracking | TargetingComponent.h/.cpp | `SetupVictimWarp()`, `SetupAttackerPairedWarp()` with continuous tracking |
| Partner Collision Management | PairedAnimationComponent.h/.cpp | `PairedAnimationPartners` array + `IgnoreActorWhenMoving()` |
| Input Blocking | PairedAnimationComponent.cpp | `bBlockCombatInput` flag, queried by `CombatComponent::CanProcessInput()` |
| State Transition Safety | PairedAnimationComponent.cpp | `OnPairedPartnerDeath()`, `CancelPairedAnimation()`, EndPlay cleanup |
| Death Animation Handling | HitReactionComponent.h/.cpp | `bDeathHandledByPairedAnimation` flag prevents double death |
| Damage Application | PairedAnimationComponent.cpp | Intelligent calc: `Max(damage, currentHealth + 1)` for lethal |
| Guard Flags | PairedAnimationComponent.cpp | `bCompletingPairedAnimation` prevents double execution |
| Distance Validation | PairedAnimationComponent.cpp | Uses SoftAimRange (intentional - see design decisions) |
| Sync Point Validation | AnimNotifyState_PairedAnimationSync.cpp | Alignment check with auto-nudge |
| Cinematic Effects | CinematicEffectsUtilityLibrary.h/.cpp | `ApplySlowMotion()`, `TriggerCameraShake()`, `RestoreTimeDilation()` |
| Obstacle Validation | PairedAnimationUtilityLibrary.cpp | `ValidatePairedAnimation()`, `IsPathClear()` |
| Debug Visualization | CombatDebugHUD.cpp, DebugUtils.cpp | CVars for warp targets, partner connections, sync points |
| Test Suite | PairedAnimationTests.cpp | 34 tests covering core functionality |

#### Wired FX Systems (Commit f27a068, 3038b21, 0e6ae4e, 150cd3a)
| Component | Files | Status |
|-----------|-------|--------|
| Impact Audio | CinematicEffectsUtilityLibrary.h, CombatFXData.h | ✅ `ResolveAndPlayImpactSound()` with 4-tier resolution |
| Impact VFX | CinematicEffectsUtilityLibrary.h, CombatFXData.h | ✅ `ResolveAndSpawnImpactVFX()` with Niagara + surface alignment |
| Pooled FX | CombatFXData.h | ✅ `UCombatFXData` asset with random selection per attack type |
| Paired Animation Audio | PairedAnimationComponent.cpp | ✅ `TriggerSyncPointEffects()` plays ImpactSound, VictimReactionSound, AttackerVoiceLine |
| Paired Animation VFX | PairedAnimationComponent.cpp | ✅ `TriggerSyncPointEffects()` spawns ImpactVFX at contact midpoint |
| Per-Hit Hitstop | CinematicEffectsUtilityLibrary.h | ✅ `ApplyHitstop()` with FTSTicker for wall-clock accuracy |

#### Scaffolded (Property Slots Exist, Not Wired)
| Component | Files | What Exists | What's Missing |
|-----------|-------|-------------|----------------|
| Music Ducking | PairedAnimationData.h | `MusicDuckingDB` | No audio ducking implementation |
| Post-Process Effects | PairedAnimationData.h | `SlowMoPostProcessMaterial`, `ScreenBloodMaterial` | No post-process application |
| Blood Decals | PairedAnimationData.h | `bSpawnBloodDecals` | No decal spawning |
| Selective Hitstop | CinematicEffectsUtilityLibrary.h | `FreezeActors()`, `RestoreActors()` functions | Not called in finisher flow - uses world slow-mo instead |

#### Scaffolded (Code Complete, Needs Animations)
| Component | Files | Status |
|-----------|-------|--------|
| Counter AC3 Mode | PairedAnimationComponent.cpp | `TryCounter_AC3Mode()` — instant counter-kill via slow-mo + lethal damage |
| Counter Chain Mode | PairedAnimationComponent.cpp | `TryCounter_ChainMode()` — Parry→Counter→Finisher state machine |
| Chain State Machine | PairedAnimationComponent.h | `EChainCounterState`: None→ParryActive→CounterWindow→CounterActive→FinisherReady |
| Parry Window | PairedAnimationComponent.h | `bParryWindowActive` + `AnimNotifyState_ParryWindow` wired |
| Contextual Stagger | HitReactionComponent.h | `ApplyStagger()`, `IsStaggered()`, `EndStagger()` — replaces posture |
| Procedural Blending | CombatComponent.cpp | 6 easing strategies wired in `PlayAttackMontage()` |

#### Branch Acceptance Caveats
| Area | Status | Requirement |
|------|--------|-------------|
| Counter Chain Mode | Canonical but incomplete | Must be proven through public Block/attack input flow, active Chain context, paired completion handoff, and asset readiness. Protected helper tests are not enough. |
| SpecificCounterData Wiring | In scope | Resolve selected `UAttackData::CounterData` first, attacker notify `SpecificCounterData` only as an explicit fallback, then non-paired fallback. |

#### Planned (Not Yet Started)
| Component | Priority | Blocker |
|-----------|----------|---------|
| Counter Animations | P1 | Parry, counter attack, chain finisher montages needed |
| SpecificCounterData Wiring | P1 | In scope for Chain branch: selected `AttackData::CounterData` first; `SpecificCounterData` is an explicit fallback only. |
| Production Enemy AI | P2 | Minimal StateTree + `UCombatTokenSubsystem` combat proof is wired; perception, patrol, tactics, and production tuning remain future work. |

#### Editor/Runtime Unification Gap (Needs Further Inquiry)

> **Critical Architecture Issue**: Editor preview tools and runtime systems should use identical logic paths (WYSIWYG principle). Currently they diverge in several areas.

| Gap | Location | Issue |
|-----|----------|-------|
| **Schema Parity** | `CombatTypes.h` | `FAttackWarpConfig` (regular attacks) lacks `WarpTargetOffset` that `FPairedWarpConfig` has |
| **Runtime Parity** | `TargetingComponent.cpp` | `SetupAttackWarp()` ignores offsets; paired animation warps correctly apply them |
| **Logic Injection** | `WeaponComponent.cpp` | `PerformWeaponTrace` is private; editor cannot simulate hits without duplicating trace math |

**Proposed Solutions** (pending further investigation):
1. Add `FVector WarpTargetOffset` to `FAttackWarpConfig`
2. Update `SetupAttackWarp()` to apply offset rotated by target direction
3. Extract `PerformWeaponTrace` to public static function for shared editor/runtime use

**Goal**: What you see in the editor preview IS what happens at runtime.

#### Key Design Decisions
1. **SoftAimRange for Finisher Distance**: Intentional. Finisher-specific detection wasn't working. SoftAimRange is proven to work.
2. **Single UPairedAnimationData**: Architecture analysis recommends Option A - single data asset with EditCondition-based field hiding per ReactionType.
3. **World Slow-Mo Over Selective Hitstop**: Simpler implementation, similar visual effect. Selective freeze available if needed later.
4. **Death Handled by Paired Animation Flag**: Prevents HitReactionComponent from playing AM_Deaths after finisher - victim montage IS the death animation.

#### Entry Points for Finisher Flow
```
Player Input → CombatComponent::ExecuteAction()
  └→ PairedAnimationComponent::TryExecuteFinisher() (delegated from CombatComponent)
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
| 5-Component Architecture | ✅ Stable | Combat, Targeting, Weapon, HitReaction, PairedAnimation |
| Input Buffering | ✅ Stable | Last-input-wins queue, input always captured |
| Combo System | ✅ Stable | Phase-derived combo timing + PendingComboTransitions counter (INPUT-1 fixed) |
| Stagger/Counter | ✅ Scaffolded | Posture deprecated → contextual stagger. AC3 + Chain counter modes. |
| Hit Detection | ✅ Stable | Socket-based weapon traces, substep sweeps |
| Impact Effects | ✅ Stable | Per-hit hitstop, audio, VFX with pooled FX data assets |
| Procedural Blending | ✅ Stable | 6 easing strategies, wired in PlayAttackMontage |
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

**Documentation**:
- **Technical Spec**: `docs/specs/PAIRED_ANIMATION_SPEC.md` - Complete system specification
- **Gap Tracker**: `docs/plans/gap-tracker.md` - see file for current gap counts
- **Combat Polish Plan**: `docs/plans/combat-polish-plan.md` - Normal attack effects (active)
- **Audit Synthesis**: `docs/audits/AUDIT_SYNTHESIS_2026-02-03.md` - Unified audit findings
- **Archived Plans**: `docs/plans/archive/` - Previous plan versions with dates

## Test Suite

**Coverage**: Run the standard baseline for the current completed-result and failure counts. The dated baseline in `Source/KatanaCombatTest/README.md` is historical evidence, not a live total.

**Run Tests**:
- Editor: `Window → Developer Tools → Session Frontend → Automation tab → Filter: "KatanaCombat"`
- CLI: See Build & Development section above

**Test Categories**:
- Core Combat: State transitions, input buffering, hold mechanics, parry detection, attack execution
- Components: Targeting, weapon, hit reactions (directional, i-frames, stun, death)
- Systems: Damage flow, death system, integration, debug visualization, memory safety

**Full Documentation**: `Source/KatanaCombatTest/README.md`

## Known Issues

- **Pre-commit hooks have syntax errors**: PowerShell scripts in `.claude/hooks/` have parsing issues. Use `git commit --no-verify` to bypass until fixed.
- **DX12 crashes with RTX 5090**: See Environment Notes below for workaround.

## Environment Notes

**GPU Crash Workaround (RTX 5090 + UE 5.6)**: Currently using DX11 (`Config/DefaultEngine.ini:47`) due to driver 581.57 + DX12 crashes. Revert to DX12 when stable Studio Driver available.

**Plugin Conflicts**: 14 conflicting marketplace plugins disabled in `KatanaCombat.uproject:53-109`. Only enabled: ModelingToolsEditorMode, StateTree, GameplayStateTree, MotionWarping.
