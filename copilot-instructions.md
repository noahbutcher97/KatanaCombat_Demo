# Copilot Instructions for KatanaCombat

## Project Overview

**KatanaCombat** is a high-fidelity melee combat system for Unreal Engine 5.6, inspired by *Ghost of Tsushima* and *Sekiro*. This is a C++/Blueprint hybrid project implementing:

- **4-Component Architecture**: Combat, Targeting, Weapon, and HitReaction components
- **Hybrid Combo System**: Responsive input buffering with snappy animation cancels
- **Posture-Based Defense**: Guard breaks and perfect parries
- **Data-Driven Configuration**: Attack properties defined in AttackData assets
- **Paired Animation System**: Finishers, counters, and cinematic executions
- **Comprehensive Testing**: 14 test suites with 126 tests

## Core Design Principles

### Critical Architectural Laws (DO NOT BREAK)

1. **Phases vs. Windows**
   - **Phases** (`Windup`, `Active`, `Recovery`) are **EXCLUSIVE** - only one active at a time
   - **Windows** (`Parry`, `Combo`, `Hold`, `Cancel`) are **INDEPENDENT** - boolean flags that can overlap
   - Anti-Pattern: Treating a "Hold Window" as a separate "Hold Phase"

2. **Input Always Buffered**
   - Input is **ALWAYS** buffered during an attack
   - The `ComboWindow` determines **WHEN** it executes, not **WHETHER** it buffers
   - Anti-Pattern: Gating input registration behind `bCanCombo`

3. **Parry is Defender-Side**
   - Parry is a **Defender-Side** check
   - The Defender checks `Enemy->IsInParryWindow()`
   - The `ParryWindow` is defined on the **Attacker's** montage
   - Anti-Pattern: Putting the Parry Window on the defender's block animation

4. **Functional Consolidation**
   - `UCombatComponent` is intentionally dense (~1000 lines)
   - Do NOT fragment logic into tiny sub-components unless absolutely necessary
   - Maintain separation between the 4 main components

## Project Structure

```
Source/KatanaCombat/Public/
├── CombatTypes.h              # ALL enums, structs, system-wide delegates
├── Core/
│   ├── CombatComponent.h      # Combat state, attack execution, FIFO queue
│   ├── TargetingComponent.h   # Soft-lock targeting, aim assist
│   ├── WeaponComponent.h      # Hit detection, weapon state
│   └── HitReactionComponent.h # Damage reception, hit reactions, death
├── Data/
│   ├── AttackData.h           # Attack configuration asset
│   ├── CombatSettings.h       # Global tuning values
│   └── HitReactionSettings.h  # Hit reaction configuration
├── Animation/
│   ├── AnimNotify_AttackPhaseTransition.h
│   ├── AnimNotifyState_ParryWindow.h
│   └── AnimNotifyState_ComboWindow.h
├── Characters/
│   ├── BaseCombatCharacter.h  # Base class with 4 combat components
│   ├── PlayerCharacter.h      # Player-specific combat
│   └── EnemyCharacter.h       # Enemy-specific combat
└── Interfaces/
    ├── DamageableInterface.h  # Damage/health contract
    ├── CombatInterface.h      # Combat state contract
    └── TeamMemberInterface.h  # Team/faction contract
```

## Development Standards

### Code Quality

- **Thorough Solutions Over Quick Fixes**: Always prefer complete, well-architected implementations
- **Explore Before Implementing**: Use exploration to understand existing APIs before coding
- **Event-Driven Over Tick**: Minimize tick overhead, use timers and delegates
- **Data-Driven Configuration**: Never hardcode values - use `CombatSettings` and `AttackData` assets
- **Null Safety**: Always check weak references and component accesses

### Unreal Engine Specific

- **BlueprintNativeEvent Pattern**: MUST use `Execute_` static methods, never direct calls
  ```cpp
  // WRONG - Will crash at runtime:
  ECombatState State = Character->GetCombatState();
  
  // CORRECT - Use Execute_ pattern:
  ECombatState State = ICombatInterface::Execute_GetCombatState(Character);
  ```

- **Blueprint Exposure**: Only expose intentional public API, not internal state
- **Component Separation**: Maintain 4-component architecture, don't mix concerns
- **Delegates in CombatTypes.h**: System-wide delegates declared once in CombatTypes.h

### Testing

- **Test Coverage Required**: All new features must have corresponding tests
- **Follow Existing Patterns**: Consistent with tests in `Source/KatanaCombatTest/`
- **Run Tests Before PR**: Execute automation tests to verify changes
- **Test Location**: Unit tests in `Source/KatanaCombatTest/Private/`

## Common Mistakes to Avoid

- Hold/ParryWindow as attack phases (they're windows, not phases)
- Gating input with combo window (input always buffered)
- Tracking hold duration (check button state at window start)
- ParryWindow on defender animation (goes on attacker's montage)
- Declaring delegates in component headers (use CombatTypes.h)
- Calling `BlueprintNativeEvent` interface methods directly (use `Execute_` pattern)
- Making internal state variables `BlueprintReadOnly` unnecessarily

## Documentation

### Essential Reading

| Purpose | Document |
|---------|----------|
| Quick Reference | `claude.md` - Core rules and patterns |
| System Overview | `docs/SYSTEM_PROMPT.md` - Complete system context |
| Architecture | `docs/ARCHITECTURE.md` - Component design details |
| API Reference | `docs/API_REFERENCE.md` - Function signatures |
| Troubleshooting | `docs/TROUBLESHOOTING.md` - Common issues |
| Attack Creation | `docs/ATTACK_CREATION.md` - How to add new attacks |

### Context Modes

The project uses context modes for focused work. Available modes:

- **`combat-logic`**: Core mechanics (State Machine, Input Buffering, C++ logic)
- **`animation`**: Visuals & timing (AnimInstance, AnimNotify, Montages)
- **`data-assets`**: Content design (AttackData, CombatSettings)
- **`testing`**: QA & stability (Test suites, automation)
- **`full`**: Architecture (Cross-cutting changes)

Documentation in `.claude/context-modes/` and `.gemini/context-modes/`

## Build and Test Instructions

See `copilot-setup-steps.yaml` for detailed setup and validation steps.

### Quick Reference

**Build**: Open `KatanaCombat.uproject` in Unreal Engine 5.6, then Build > Compile

**Test**: Window → Developer Tools → Session Frontend → Automation tab → Filter: "KatanaCombat"

**Debug Visualization**: Use console commands like `Combat.Debug.All 1` to enable debug overlays

## Key Default Values

| Parameter | Value | Notes |
|-----------|-------|-------|
| ComboInputWindow | 0.6s | Input buffer window |
| ParryWindow | 0.3s | Perfect parry timing |
| ComboBlendOut/In | 0.1s | Per-attack tunable |
| MaxPosture | 100.0f | Posture system |
| LightBaseDamage | 25.0f | Base damage values |
| HeavyBaseDamage | 50.0f | Base damage values |

## Current Development Focus

**Primary**: Paired Animation System (Phase 5) - ~95% complete
- Finisher execution flow implemented
- Symmetric warp tracking active
- Partner collision management working
- Death animation handling complete
- Test suite in place (34 tests)

**Stable**: Core combat system, input buffering, combo chaining, hit detection, death system

**Deferred**: Guard break mechanics, network replication, environmental finishers

See `docs/ROADMAP.md` and `.claude/plans/` for detailed status.

## Task Guidelines

### For New Features

1. **Clarify**: Is it a State? Action? Does it need Data?
2. **Explore**: Understand existing APIs and patterns first
3. **Data First**: Add properties to `CombatSettings` or create `AttackData` asset
4. **Implement Logic**: Update state machine in `CombatComponent`
5. **Add Animation**: Create montage with proper notifies
6. **Test**: Write unit tests before marking complete
7. **Validate**: Run test suite and manual testing

### For Bug Fixes

1. **Diagnose**: Identify which component owns the logic
2. **Check Data**: Verify assets are configured correctly
3. **Test First**: Create reproduction test case
4. **Fix**: Minimal changes to address root cause
5. **Verify**: Ensure fix doesn't break existing behavior
6. **Re-test**: Run full test suite

### For Refactoring

1. **Preserve Architecture**: Maintain 4-component separation
2. **Keep Tests Green**: All 126 tests must pass
3. **No Breaking Changes**: Maintain public API compatibility
4. **Document Changes**: Update relevant docs if architecture shifts

## Troubleshooting

- **Attacks not executing**: Check `GetCombatState()` == Idle, `DefaultLightAttack` assigned
- **Combos not chaining**: Check `AnimNotifyState_ComboWindow` in montage, `NextComboAttack` set
- **Hits not detecting**: Check weapon sockets (`WeaponStart/WeaponEnd`), `AnimNotify_AttackPhaseTransition(Active)` present
- **Parry not working**: `AnimNotifyState_ParryWindow` must be on ATTACKER's montage

See `docs/TROUBLESHOOTING.md` for comprehensive debugging guide.

## Environment Notes

- **Platform**: Windows (Win32)
- **Engine**: Unreal Engine 5.6
- **Graphics**: Currently using DX11 (DX12 has issues with RTX 5090 + driver 581.57)
- **Plugins**: Only essential plugins enabled (ModelingToolsEditorMode, StateTree, GameplayStateTree, MotionWarping)

## Git Conventions

- Clean commit messages (no trailers or sign-offs)
- Descriptive messages with bullet points for changes
- Use `--no-verify` if pre-commit hooks have errors
- Review `.gitignore` to avoid committing build artifacts

## Questions?

For complex architectural questions or unclear requirements, please ask before implementing. The codebase has specific design decisions that may not be obvious from the code alone. See documentation in `docs/` and `.claude/` directories for detailed context.
