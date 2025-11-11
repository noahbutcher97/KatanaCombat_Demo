# KatanaCombat

**Ghost of Tsushima-inspired melee combat system for Unreal Engine 5.6**

A deep, technical combat framework emphasizing responsive attack chains, precision timing, and posture-based defense. Built with C++ for performance and designer-friendly data assets for flexibility.

---

## Recent Updates (2025-11-11)

**Universal Combo Blending System** - All combo transitions now support configurable blend times:
- `ComboBlendOutTime` - Smooth exit when transitioning FROM this attack (default: 0.1s)
- `ComboBlendInTime` - Smooth entry when this attack is the TARGET of transition (default: 0.1s)
- Works for all combo types: light→light, light→heavy, directional follow-ups

**Heavy Attack Charge System Enhancements**:
- `ChargeLoopSection` / `ChargeReleaseSection` - Montage section support for distinct charge/release animations
- `ChargeLoopBlendTime` / `ChargeReleaseBlendTime` - Smooth transitions between charge phases (0.3s/0.2s defaults)
- Fallback to idle when no release section configured

**Critical Bug Fixes**:
- Fixed light attack early release freeze (ease-out direction tracking)
- Fixed input lockout after charged heavy with fallback
- Implemented proper section-to-section blending in `JumpToSectionWithBlend()`

**V2 System Status**: Core mechanics complete (input queue, phase management, combo system, hold mechanics, blending). Next: Parry & Evade systems.

---

## Features

### Core Combat Mechanics
- **Hybrid Combo System**: Blends responsive input buffering with snappy animation canceling for fluid attack chains
- **Posture System**: Guard meter management with guard breaks, perfect parries, and counter windows (inspired by Sekiro)
- **Data-Driven Attacks**: Reusable attack definitions with montage section support
- **Motion Warping**: Cinematic chase attacks that close distance to targets
- **Directional Follow-ups**: Hold-and-release mechanics for branching combo paths

### Technical Highlights
- **Component-Based Architecture**: Four modular components (Combat, Targeting, Weapon, HitReaction) that work on any character
- **Animation-Driven Timing**: AnimNotifyStates control attack phases, hit detection, and combo windows
- **Montage Section Reuse**: Multiple attacks share one animation montage via section markers
- **Editor Tools**: Automated AnimNotify generation, timing validation, conflict detection

### Design Philosophy
- **Feel First**: Combat prioritizes responsive controls and impactful hits
- **Technical Depth**: High skill ceiling with advanced techniques (cancels, perfect parries, directional chains)
- **Readable Systems**: Designers can configure attacks without touching code
- **Pragmatic Design**: ~1000 line CombatComponent, 4 core components, no over-engineering

---

## Quick Start

### Installation
1. Clone or download this project
2. Open `KatanaCombat.uproject` with Unreal Engine 5.6
3. Compile C++ code (Build > Compile or Ctrl+Alt+F11)
4. Enable required plugins (should auto-enable):
   - Motion Warping
   - State Tree
   - Gameplay State Tree

### Basic Setup
1. Add components to your character Blueprint:
   - `CombatComponent`
   - `TargetingComponent`
   - `WeaponComponent`
   - `HitReactionComponent`
   - `MotionWarpingComponent` (from plugin)

2. Create weapon sockets on skeletal mesh:
   - `weapon_start` - Handle/base of weapon
   - `weapon_end` - Tip of weapon

3. Create a `CombatSettings` Data Asset:
   - Content Browser > Right-click > Miscellaneous > Data Asset > CombatSettings
   - Configure default values (or use defaults)

4. Create an `AttackData` asset:
   - Right-click > Miscellaneous > Data Asset > AttackData
   - Assign animation montage
   - Configure damage, posture damage, hitstun
   - Set timing mode (AnimNotify-driven recommended)

5. Assign to CombatComponent:
   - Select character Blueprint
   - Find CombatComponent in Components panel
   - Set `DefaultLightAttack` and `DefaultHeavyAttack`
   - Set `CombatSettings` reference

6. Add AnimNotifyStates to attack montage:
   - Open attack montage in Animation Editor
   - Add `AnimNotifyState_AttackPhase` for Windup, Active, Recovery
   - Add `AnimNotify_ToggleHitDetection` at Active start/end
   - (Optional) Add `AnimNotifyState_ComboWindow` during Recovery

7. Bind input events:
   ```cpp
   // In your character class or input component
   CombatComponent->OnLightAttackPressed();
   CombatComponent->OnLightAttackReleased();
   CombatComponent->OnHeavyAttackPressed();
   CombatComponent->OnHeavyAttackReleased();
   CombatComponent->OnBlockPressed();
   CombatComponent->OnBlockReleased();
   ```

8. Test in editor and tune values

For complete setup instructions, see [GETTING_STARTED.md](GETTING_STARTED.md).

---

## Project Structure

```
Source/KatanaCombat/
├── Public/
│   ├── CombatTypes.h                    # Enums, structs, system-wide delegates
│   ├── Core/                            # Core combat components
│   │   ├── CombatComponent.h            # State machine, attacks, posture, combos
│   │   ├── TargetingComponent.h         # Cone-based targeting, motion warp setup
│   │   ├── WeaponComponent.h            # Socket-based hit detection
│   │   └── HitReactionComponent.h       # Damage reception, hit reactions
│   ├── Data/                            # Data assets
│   │   ├── AttackData.h                 # Attack configuration
│   │   └── CombatSettings.h             # Global tuning values
│   ├── Animation/                       # AnimNotifies and AnimInstance
│   │   ├── SamuraiAnimInstance.h        # Animation Blueprint bridge
│   │   ├── AnimNotifyState_AttackPhase.h
│   │   ├── AnimNotifyState_ComboWindow.h
│   │   ├── AnimNotifyState_ParryWindow.h
│   │   ├── AnimNotifyState_HoldWindow.h
│   │   └── AnimNotify_ToggleHitDetection.h
│   ├── Characters/                      # Character implementations
│   │   └── SamuraiCharacter.h
│   └── Interfaces/                      # Interface contracts
│       ├── CombatInterface.h
│       └── DamageableInterface.h
└── Private/                             # .cpp implementations

Source/KatanaCombatEditor/              # Editor-only tools
├── Public/
│   └── AttackDataTools.h                # Automated notify generation, validation
└── Private/
    └── (implementations)
```

---

## Core Concepts

### Attack Phases (Exclusive)
Every attack has 3 sequential phases:
1. **Windup**: Telegraph, motion warping active, vulnerable to parry
2. **Active**: Hit detection enabled, damage dealt
3. **Recovery**: Vulnerable, combo window opens

### Windows (Independent, Can Overlap)
Windows modify behavior during phases:
- **Combo Window**: Allows early execution of next attack (snappy path)
- **Parry Window**: Attacker is vulnerable to being parried
- **Hold Window**: Animation pauses for directional input
- **Cancel Window**: Can interrupt with specific inputs

### Input Buffering (Always On)
- Input is **always buffered** during attacks
- Combo window modifies **when** execution happens, not **whether**
- **Snappy path**: Input during combo window cancels recovery early
- **Responsive path**: Input waits for recovery to complete naturally

### Parry System (Contextual Block)
- Parry is not a separate input - it's a **contextual block action**
- Block during enemy's **parry window** (Windup phase) = Perfect Parry
- Defender checks `ICombatInterface::IsInParryWindow()` on nearby attackers
- Successful parry opens counter window on attacker (1.5x damage)

### Posture System
- Characters have posture (0-100)
- Depletes when blocking attacks
- Regenerates based on state (attacking > not blocking > idle)
- Posture reaches 0 → **Guard Break** (2s stun, vulnerable to finishers)

---

## Combat Flow Example

```
Player presses Light Attack
    ↓
CombatComponent::OnLightAttackPressed()
    ↓
Find appropriate attack:
    - In combo? Use NextComboAttack
    - After hold? Use DirectionalFollowUps[Direction]
    - Otherwise: Use DefaultLightAttack
    ↓
ExecuteAttack(AttackData)
    ↓
Setup motion warp to nearest target
    ↓
Play attack montage (section-aware)
    ↓
Transition to Attacking state
    ↓
AnimNotifyState_AttackPhase callbacks:
    - Windup Begin → Motion warping active
    - Active Begin → WeaponComponent::EnableHitDetection()
    - Active End → WeaponComponent::DisableHitDetection()
    - Recovery Begin → Open combo window, check buffered inputs
    ↓
During combo window:
    - Input queued? → Cancel recovery, execute immediately (snappy)
    - OR wait for recovery end → Execute buffered attack (responsive)
    ↓
Combo chains or return to Idle
```

---

## Documentation

- **[README.md](README.md)** (this file) - Project overview
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Complete technical deep dive
- **[ARCHITECTURE_QUICK.md](ARCHITECTURE_QUICK.md)** - Quick reference for developers
- **[GETTING_STARTED.md](GETTING_STARTED.md)** - Step-by-step setup guide
- **[ATTACK_CREATION.md](ATTACK_CREATION.md)** - Attack authoring workflow
- **[API_REFERENCE.md](API_REFERENCE.md)** - Complete API documentation
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Common issues and solutions
- **[Source/KatanaCombatTest/README.md](../Source/KatanaCombatTest/README.md)** - C++ unit test suite documentation

---

## System Requirements

- **Engine**: Unreal Engine 5.6
- **Language**: C++20
- **Platform**: Windows, Mac, Linux (tested on Windows)
- **Plugins**: Motion Warping, State Tree, Gameplay State Tree

---

## Key Design Patterns

### Component Responsibilities
- **CombatComponent** (~1000 lines): State machine, attack execution, posture, combos, parry/counters
  - Intentionally large - combat flow logic is tightly coupled
- **TargetingComponent** (~300 lines): Enemy selection, directional cone targeting
- **WeaponComponent** (~200 lines): Socket-based swept sphere hit detection
- **HitReactionComponent** (~300 lines): Damage application, hit reactions, hitstun

### Data-Driven Configuration
All tunable values live in Data Assets:
- **AttackData**: Damage, timing, combos, motion warp settings per attack
- **CombatSettings**: Global values (posture regen, timing windows, defaults)

### Event-Driven Communication
Components communicate via delegates:
- `OnCombatStateChanged` → AnimInstance, UI, AI
- `OnWeaponHit` → Character applies damage
- `OnDamageReceived` → VFX, SFX, UI
- `OnGuardBroken` → Camera shake, slow-mo, finisher prompt

---

## Examples

### Creating a Combo Chain
```
Light1 (Default) → Light2 → Light3 → Light4
  ↓                  ↓        ↓         ↓
Heavy1            Heavy2   Heavy3   Heavy4

Configuration:
Light1.NextComboAttack = Light2
Light1.HeavyComboAttack = Heavy1
Light2.NextComboAttack = Light3
Light2.HeavyComboAttack = Heavy2
...
```

### Directional Follow-ups
```cpp
// In AttackData for Light3:
DirectionalFollowUps[Forward] = SpinSlash
DirectionalFollowUps[Backward] = BackstepSlash
DirectionalFollowUps[Left] = LeftSweep
DirectionalFollowUps[Right] = RightSweep

// Player holds Light during Light3, then releases with direction input
// → Executes corresponding follow-up attack
```

### Adding an Enemy
1. Create character class inheriting `ACharacter`
2. Implement `ICombatInterface` and `IDamageableInterface`
3. Add the 4 core components
4. Create `AnimInstance` inheriting `SamuraiAnimInstance`
5. Create `AttackData` assets for enemy attacks
6. Assign to `DefaultLightAttack` / `DefaultHeavyAttack`
7. Configure hit reactions in `HitReactionComponent`
8. AI uses `ExecuteAttack()` from Behavior Tree tasks

---

## Testing & Quality Assurance

KatanaCombat includes a comprehensive **C++ unit test suite** to validate core design principles and catch regressions.

### Test Suite

The `KatanaCombatTest` module provides **7 test files** with **45+ assertions** covering:

- **State Machine** - Valid/invalid transitions, edge cases
- **Input Buffering** - Always-buffered, responsive vs snappy paths
- **Hold Mechanics** - Button state detection (not duration tracking)
- **Parry System** - Defender-side detection, contextual blocking
- **Attack Execution** - ExecuteAttack vs ExecuteComboAttack separation
- **Memory Safety** - Null handling, graceful fallbacks
- **Architecture** - Phases vs windows separation validation

### Running Tests

**In Editor**:
1. Window → Developer Tools → Session Frontend
2. Automation tab → Filter: "KatanaCombat"
3. Select tests and click "Start Tests"

**Command Line**:
```bash
UnrealEditor.exe "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat"
```

**See** [KatanaCombatTest README](../Source/KatanaCombatTest/README.md) for complete test documentation.

---

## Debugging

Enable debug visualization:
```cpp
CombatComponent->bDebugDraw = true;    // State, phase, timing windows
TargetingComponent->bDebugDraw = true;  // Cone, targets, distances
WeaponComponent->bDebugDraw = true;     // Swept traces, hit points
```

Check logs:
- `LogCombat` - State transitions, attack execution
- `LogAnimation` - Montage playback issues
- `LogWeapon` - Hit detection events

---

## Inspirations

- **Ghost of Tsushima**: Stance system, precise timing, cinematic feel
- **Sekiro**: Posture system, perfect parries, guard breaks
- **Devil May Cry**: Snappy cancels, long combo chains
- **Sifu**: Responsive attack strings, hold-and-release mechanics
- **God of War**: Heavy/light attack paradigm, directional attacks

---

## Future Features

Planned expansions:
- Finisher system for guard-broken enemies
- Special attacks with resource management
- Weapon switching with unique movesets
- Aerial combat (launchers, air combos)
- Frame-tight cancels into special moves
- Multiplayer support (server-authoritative)

---

## License

Copyright Epic Games, Inc. All Rights Reserved.

---

## Contributing

This is a learning/reference project. Feel free to fork and modify for your own projects.

For questions or issues, check [TROUBLESHOOTING.md](TROUBLESHOOTING.md) first.

---

## Credits

Built with Unreal Engine 5.6 using C++20. Inspired by character action games and precision combat systems.

**Key Principles**:
1. Feel First - Combat should be responsive and impactful
2. Technical Depth - High skill ceiling with advanced techniques
3. Readable Systems - Designers can understand and modify
4. Pragmatic Design - No over-engineering, consolidate where appropriate