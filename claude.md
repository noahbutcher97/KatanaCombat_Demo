# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**KatanaCombat** is a Ghost of Tsushima-inspired melee combat system for Unreal Engine 5.6 (C++). The system features:
- 4-component architecture (Combat, Targeting, Weapon, HitReaction)
- Hybrid combo system (responsive input buffering + snappy animation cancels)
- Posture-based defense with guard breaks and perfect parries
- Data-driven attack configuration via AttackData assets
- Death system with directional animations and ragdoll transitions
- Comprehensive test suite (14 suites, 126 tests)

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

**Run Tests** (Command Line):
```powershell
# Run tests in background (don't wait for completion callback - it hangs)
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat" -unattended -nopause -NullRHI -nosplash -stdout
```

**IMPORTANT**: The test command doesn't return a clean exit. Run it in background, wait ~5 minutes, then kill the process and check the log manually. This is the standard workflow.

**Test Results**: Check the log file at `D:\UnrealProjects\5.6\KatanaCombat\Saved\Logs\KatanaCombat.log`
```powershell
# View test results summary (pass/fail counts)
grep -E "Test Completed.*Result=" D:/UnrealProjects/5.6/KatanaCombat/Saved/Logs/KatanaCombat.log | sed 's/.*Result={\([^}]*\)}.*/\1/' | sort | uniq -c

# View failing tests with error messages
grep -E "Error:" D:/UnrealProjects/5.6/KatanaCombat/Saved/Logs/KatanaCombat.log | grep -i "automation\|test" | head -20

# View specific test failures
grep -E "Test Completed.*Fail" D:/UnrealProjects/5.6/KatanaCombat/Saved/Logs/KatanaCombat.log
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

6. **Delegates in CombatTypes.h**: System-wide delegates declared ONCE in CombatTypes.h. Components use `UPROPERTY` only.

## File Structure

```
Source/KatanaCombat/Public/
├── CombatTypes.h              ← ALL enums, structs, system-wide delegates
├── Core/
│   ├── CombatComponent.h      ← Combat state, attack execution, FIFO queue
│   ├── TargetingComponent.h   ← Soft-lock targeting, aim assist
│   ├── WeaponComponent.h      ← Hit detection, weapon state
│   └── HitReactionComponent.h ← Damage reception, hit reactions, death
├── Data/
│   ├── AttackData.h           ← Attack configuration asset
│   ├── AttackConfiguration.h  ← Attack moveset package (PDA)
│   ├── CombatSettings.h       ← Global tuning values
│   └── HitReactionSettings.h  ← Hit reaction configuration
├── Animation/
│   ├── AnimNotify_AttackPhaseTransition.h  ← Phase transitions
│   ├── AnimNotifyState_ParryWindow.h
│   ├── AnimNotifyState_HoldWindow.h
│   └── AnimNotifyState_ComboWindow.h
├── Characters/
│   ├── BaseCombatCharacter.h  ← Base class with 4 combat components
│   ├── PlayerCharacter.h      ← Player-specific combat
│   └── EnemyCharacter.h       ← Enemy-specific combat
├── Interfaces/
│   ├── DamageableInterface.h  ← Damage/health contract
│   ├── CombatInterface.h      ← Combat state contract
│   └── TeamMemberInterface.h  ← Team/faction contract
├── Math/
│   ├── CombatMathEnums.h      ← Distance formulas, bone chains, contact types
│   └── CombatMathTypes.h      ← Skeletal hierarchy, reach, contact predictions
└── Utilities/
    ├── MontageUtilityLibrary.h           ← 27 montage utility functions
    ├── PairedAnimationUtilityLibrary.h   ← Paired animation validation, contact points
    └── CinematicEffectsUtilityLibrary.h  ← Time dilation, hitstop, camera shake
```

## Key Default Values

| Parameter | Value | Notes |
|-----------|-------|-------|
| ComboInputWindow | 0.6s | |
| ParryWindow | 0.3s | |
| ComboBlendOut/In | 0.1s | Per-attack tunable |
| MaxPosture | 100.0f | |
| LightBaseDamage | 25.0f | |
| HeavyBaseDamage | 50.0f | |
| CounterDamageMultiplier | 1.5x | |

## Documentation

### Combat System Docs (`docs/`)

| Task | Documentation |
|------|--------------|
| **First read** | `docs/SYSTEM_PROMPT.md` (full system context) |
| Quick reference | `docs/ARCHITECTURE_QUICK.md` |
| Deep dive | `docs/ARCHITECTURE.md` |
| Add new attack | `docs/ATTACK_CREATION.md` |
| API reference | `docs/API_REFERENCE.md` |
| Paired animation spec | `docs/specs/PAIRED_ANIMATION_SPEC.md` |
| Debugging | `docs/TROUBLESHOOTING.md` |
| Change history | `docs/CHANGELOG.md` (bug fixes, feature history) |
| Future plans | `docs/ROADMAP.md` (planned features, system status) |
| Implementation plans | `docs/plans/` (active and archived feature plans) |

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

**Level 3 - Architecture Docs (`docs/`)**
Deep dives into component design and API details. Read when understanding or modifying architecture.

**Level 4 - Implementation Plans (`.claude/plans/`)**
Active development plans with gap tracking. Read when continuing phased implementation work.

| Need | Start Here |
|------|------------|
| Quick combat system rules | `CLAUDE.md` (this file) |
| Paired animation spec | `docs/specs/PAIRED_ANIMATION_SPEC.md` |
| Component architecture | `docs/ARCHITECTURE.md` |
| API details | `docs/API_REFERENCE.md` |
| Active plan status | `.claude/plans/synthetic-painting-ritchie.md` |
| Troubleshooting | `docs/TROUBLESHOOTING.md` |

## Common Mistakes to Avoid

- Hold/ParryWindow as attack phases (they're windows, not phases)
- Gating input with combo window (input always buffered)
- Tracking hold duration (check button state at window start)
- ParryWindow on defender animation (goes on attacker's montage)
- Declaring delegates in component headers (use CombatTypes.h)
- Using TArray for cancel inputs (use bitmask)
- Calling `BlueprintNativeEvent` interface methods directly (use `Execute_` pattern):
  ```cpp
  // WRONG (crashes): Character->GetCombatState();
  // CORRECT: ICombatInterface::Execute_GetCombatState(Character);
  ```

## Troubleshooting

**Attacks not executing**: Check `GetCombatState()` == Idle, `DefaultLightAttack` assigned, `AnimInstance` valid

**Combos not chaining**: Check `AnimNotifyState_ComboWindow` in montage, `NextComboAttack` set in AttackData

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
- Maintain 4-component separation (intentional architecture)
- Preserve Blueprint exposure (`UFUNCTION(BlueprintCallable)`) for PUBLIC API functions
- Update existing files (don't create "_V2" variants)

**DON'T**:
- Create duplicate functions with suffixes
- Use deprecated features (`AnimNotifyState_AttackPhase`, `AnimNotify_ToggleHitDetection`)
- Assume `FGeometry::GetRenderTransform()` exists (UE 5.6 removed it)
- Convert `FLinearColor` to `FColor` directly (use `.ToFColor(true)`)
- Use component tick without explicit permission
- **Make internal state variables `BlueprintReadOnly`**: If a parameter isn't meaningful to view/edit at runtime in the editor, don't expose it to Blueprint. This adds visual load and confusion. Reserve Blueprint visibility for intentional public API, not internal implementation details.

## Claude CLI Best Practices

### Session Continuity
- **Plan files persist**: Check `.claude/plans/` for active work from previous sessions
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

**Documentation**:
- **Technical Spec**: `docs/specs/PAIRED_ANIMATION_SPEC.md` - Complete system specification
- **Active Plan**: `.claude/plans/synthetic-painting-ritchie.md` - Implementation plan with gap tracking
- **Archived Plans**: `docs/plans/archive/` - Previous plan versions with dates

## Test Suite

**Coverage**: 14 test suites, 126 tests (all passing)

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
