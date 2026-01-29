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
# Full command with absolute paths
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat" -unattended -nopause -NullRHI -nosplash -stdout
```

**Test Results**: Check the log file at `D:\UnrealProjects\5.6\KatanaCombat\Saved\Logs\KatanaCombat.log`
```powershell
# View test results summary
grep -E "Test Completed" D:/UnrealProjects/5.6/KatanaCombat/Saved/Logs/KatanaCombat.log | grep -E "(Success|Fail)" | sed 's/.*Result={\([^}]*\)}.*/\1/' | sort | uniq -c

# View failing tests with errors
grep -E "Test Completed.*Fail" D:/UnrealProjects/5.6/KatanaCombat/Saved/Logs/KatanaCombat.log
```

**Debug Visualization** (CVar-controlled, use console commands):
```
Combat.Debug.All 1         // Enable all debug visualization
Combat.Debug.Direction 1   // Direction transformation arrows
Combat.Debug.Targeting 1   // Targeting cones and targets
Combat.Debug.Weapon 1      // Weapon trace visualization
Combat.Debug.Phase 1       // Attack phase indicators
Combat.Debug.Queue 1       // Action queue state
Combat.Debug.Hold 1        // Hold state visualization
Combat.Debug.DrawDuration 2.0  // Debug shape persistence (seconds)
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
└── Utilities/
    └── MontageUtilityLibrary.h  ← 27 montage utility functions
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

## Common Mistakes to Avoid

- Hold/ParryWindow as attack phases (they're windows, not phases)
- Gating input with combo window (input always buffered)
- Tracking hold duration (check button state at window start)
- ParryWindow on defender animation (goes on attacker's montage)
- Declaring delegates in component headers (use CombatTypes.h)
- Using TArray for cancel inputs (use bitmask)

## Troubleshooting

**Attacks not executing**: Check `GetCombatState()` == Idle, `DefaultLightAttack` assigned, `AnimInstance` valid

**Combos not chaining**: Check `AnimNotifyState_ComboWindow` in montage, `NextComboAttack` set in AttackData

**Hits not detecting**: Check weapon sockets (`WeaponStart/WeaponEnd`), `AnimNotify_AttackPhaseTransition(Active)` present

**Parry not working**: `AnimNotifyState_ParryWindow` must be on ATTACKER's montage, defender calls `IsInParryWindow()` on enemy

## Coding Guidelines

**DO**:
- Use timers over tick (minimize tick overhead)
- Maintain 4-component separation (intentional architecture)
- Preserve Blueprint exposure (`UFUNCTION(BlueprintCallable)`)
- Update existing files (don't create "_V2" variants)

**DON'T**:
- Create duplicate functions with suffixes
- Use deprecated features (`AnimNotifyState_AttackPhase`, `AnimNotify_ToggleHitDetection`)
- Assume `FGeometry::GetRenderTransform()` exists (UE 5.6 removed it)
- Convert `FLinearColor` to `FColor` directly (use `.ToFColor(true)`)
- Use component tick without explicit permission

## Git Conventions

- **Clean commit messages**: No trailers, sign-offs, or co-author tags - just the message and content
- Include rollback checkpoint (previous commit hash) in significant commits
- Use descriptive commit messages with bullet points for changes
- Bypass pre-commit hooks with `--no-verify` if they have errors (hooks in `.claude/hooks/` may have issues)

## Active Development

Track ongoing work across sessions. Update this section when starting/completing major tasks.

**Current Focus**:
- Hit Reaction Polish: Cycled animation arrays with n-2 randomization (prevents flip-flopping)

**Next Up**:
- Paired Animation System: Synced finisher/counter animations with position constraints

**Recently Completed**:
- Death System (2025-01-29): Directional death animations, ragdoll transitions, bIsDead flag
- Documentation Audit (2025-01-29): V1/V2 removal, test count updates, deprecated notify cleanup

**Plans**: See `docs/plans/` for detailed implementation plans and `docs/plans/archive/` for completed plans.

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
