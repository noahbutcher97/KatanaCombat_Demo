# KatanaCombat

**Ghost of Tsushima-inspired melee combat system for Unreal Engine 5.6**

A deep, technical combat framework emphasizing responsive attack chains, precision timing, and posture-based defense. Built with C++ for performance and designer-friendly data assets for flexibility.

---

## Recent Updates (2025-01-29)

### v3.0.0 - Architecture Consolidation & Motion Warping Unification

**Status**: ✅ Major infrastructure refactor complete - unified combat system, modular settings, adaptive motion warping

**Key Changes**:
- **V1 Removed**: Single unified `CombatComponent` (no more V1/V2 distinction)
- **Character Hierarchy**: New `BaseCombatCharacter` → `PlayerCharacter` / `EnemyCharacter`
- **Modular Settings**: `TargetingSettings` and `MotionWarpingSettings` as separate data assets
- **Unified Motion Warping**: `AnimNotifyState_CombatWarp` auto-selects translation+rotation vs rotation-only

**AnimNotifyState_CombatWarp** - Single notify replaces dual motion warping setup:
- Detects at runtime which warp target exists (AttackTarget vs RotationTarget)
- Enemy found → Translation + Rotation (move toward target)
- No enemy → Rotation only (face direction, no sliding)
- Neither → Skip warp entirely

**Modular Settings Pattern** - Three-tier configuration hierarchy:
```cpp
1. Component->SettingsOverride      // Per-instance override (highest priority)
2. CombatSettings->SubsystemSettings // Character-type default
3. Hardcoded fallback               // Safe defaults (lowest priority)
```

**Next Steps**: Phase 6 (Parry & Evade systems), Phase 7 (Posture integration), Phase 8+ (Polish & AI)

---

## Features

### Core Combat Mechanics
- **Hybrid Combo System**: Blends responsive input buffering with snappy animation canceling for fluid attack chains
- **Posture System**: Guard meter management with guard breaks, perfect parries, and counter windows (inspired by Sekiro)
- **Data-Driven Attacks**: Reusable attack definitions with montage section support
- **Motion Warping**: Cinematic chase attacks that close distance to targets
- **Directional Follow-ups**: Hold-and-release mechanics for branching combo paths

### Technical Highlights
- **Unified Combat System**: Event-driven architecture with FIFO input queue and snap/responsive execution modes
- **Component-Based Architecture**: Four modular components (Combat, Targeting, Weapon, HitReaction) that work on any character
- **Animation-Driven Timing**: AnimNotifyStates control attack phases, hit detection, and combo windows
- **Montage Section Reuse**: Multiple attacks share one animation montage via section markers
- **Procedural Easing**: 10 easing types for smooth playrate transitions without authored curves
- **27 Montage Utilities**: Blueprint-exposed library for timing queries, blending, section navigation
- **Universal Blending**: Configurable crossfade times for all combo transitions (per-attack control)
- **Editor Tools**: Automated AnimNotify generation, timing validation, custom details panels with section selectors

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

6. Add AnimNotifies to attack montage:
   - Open attack montage in Animation Editor
   - Add 4x `AnimNotify_AttackPhaseTransition` at phase boundaries:
     1. None → Windup (attack start)
     2. Windup → Active (damage frames begin)
     3. Active → Recovery (damage frames end)
     4. Recovery → None (attack complete)
   - Hit detection is automatic during Active phase (no toggle notify needed)
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
│   ├── ActionQueueTypes.h               # Input/action queue data structures
│   ├── Core/                            # Core combat components
│   │   ├── CombatComponent.h            # Main combat component (event-driven, FIFO queue)
│   │   ├── TargetingComponent.h         # Cone-based targeting, soft aim assist, motion warp
│   │   ├── WeaponComponent.h            # Socket-based hit detection
│   │   └── HitReactionComponent.h       # Damage reception, hit reactions
│   ├── Utilities/                       # Utility libraries
│   │   ├── MontageUtilityLibrary.h      # 27 Blueprint functions for montage operations
│   │   └── CombatUtils.h                # Combat helper functions
│   ├── Data/                            # Data assets
│   │   ├── AttackData.h                 # Attack configuration (includes FAttackWarpConfig)
│   │   ├── AttackConfiguration.h        # Attack moveset package (PDA)
│   │   ├── CombatSettings.h             # Global tuning values + data asset references
│   │   ├── TargetingSettings.h          # Targeting system configuration
│   │   └── MotionWarpingSettings.h      # Motion warp defaults
│   ├── Animation/                       # AnimNotifies and AnimInstance
│   │   ├── SamuraiAnimInstance.h        # Animation Blueprint bridge
│   │   ├── AnimNotify_AttackPhaseTransition.h  # Event-driven phase transitions
│   │   ├── AnimNotifyState_CombatWarp.h        # Unified combat-aware motion warping
│   │   ├── AnimNotifyState_ComboWindow.h
│   │   ├── AnimNotifyState_ParryWindow.h
│   │   └── AnimNotifyState_HoldWindow.h
│   ├── Characters/                      # Character implementations
│   │   ├── BaseCombatCharacter.h        # Abstract base with common combat interfaces
│   │   ├── PlayerCharacter.h            # Player-specific character
│   │   └── EnemyCharacter.h             # Enemy-specific character
│   ├── Interfaces/                      # Interface contracts
│   │   ├── CombatInterface.h
│   │   ├── DamageableInterface.h
│   │   └── TeamMemberInterface.h        # Team affiliation (friend/foe)
│   └── Debug/                           # Debug utilities
│       ├── DebugConfig.h                # CVar-based debug configuration
│       └── DebugUtils.h                 # Debug visualization helpers
└── Private/                             # .cpp implementations

Source/KatanaCombatEditor/              # Editor-only tools
├── Public/
│   ├── AttackDataTools.h                # Automated notify generation, validation
│   └── AttackDataCustomization.h        # Custom details panel for AttackData
└── Private/
    └── (implementations)

Source/KatanaCombatTest/                # C++ Unit Test Suite
├── README.md                            # Test documentation
└── (14 test suites, 126 tests)
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
- **[CHANGELOG.md](CHANGELOG.md)** - Combat system change history and bug fixes
- **[ROADMAP.md](ROADMAP.md)** - Planned features and system status
- **[Source/KatanaCombatTest/README.md](../Source/KatanaCombatTest/README.md)** - C++ unit test suite documentation

### For AI/Copilot Agents
- **[copilot-instructions.md](../copilot-instructions.md)** - Comprehensive guide for GitHub Copilot agents
- **[copilot-setup-steps.yaml](../copilot-setup-steps.yaml)** - Build, test, and validation procedures

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
1. Create character class inheriting `AEnemyCharacter` (or `ABaseCombatCharacter`)
   - `AEnemyCharacter` automatically sets `TeamId = ETeamId::Enemy`
   - All combat interfaces are already implemented
2. Create `AnimInstance` inheriting `SamuraiAnimInstance`
3. Create `AttackData` assets for enemy attacks
4. Create a `CombatSettings` data asset for this enemy type
   - Reference appropriate `TargetingSettings` and `MotionWarpingSettings`
5. Assign to `DefaultLightAttack` / `DefaultHeavyAttack` on CombatComponent
6. Configure hit reactions in `HitReactionComponent`
7. AI uses `ExecuteAttack()` from Behavior Tree tasks

**Character Hierarchy**:
```cpp
ABaseCombatCharacter  // Base class with all combat interfaces
├── APlayerCharacter  // TeamId::Player, input handling
└── AEnemyCharacter   // TeamId::Enemy, AI-ready
```

---

## Testing & Quality Assurance

KatanaCombat includes a comprehensive **C++ unit test suite** to validate core design principles and catch regressions.

### Test Suite

The `KatanaCombatTest` module provides **14 test suites** with **126 tests** covering:

- **Core Combat** - State transitions, input buffering, hold mechanics, parry detection, attack execution, phases vs windows
- **Components** - Targeting, weapon hit detection, hit reactions (directional, i-frames, stun)
- **Systems** - Damage application, death system, multi-component integration
- **Robustness** - Debug visualization, memory safety, null handling

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

## Combat System Architecture

KatanaCombat features a **unified event-driven combat system**:

### CombatComponent - Core Implementation
- **Architecture**: Event-driven with `AnimNotify_AttackPhaseTransition` callbacks
- **Input System**: Timestamped FIFO queue with snap/responsive/immediate modes
- **Phase Management**: Automatic transitions based on AnimNotify events
- **Hit Detection**: Automatic during Active phase
- **Blending**: Universal combo crossfade with per-attack blend times
- **Hold Mechanics**: Procedural easing with 10 easing types, bidirectional transitions
- **Motion Warping**: Unified `SetupAttackWarp()` with adaptive translation/rotation
- **Debug Tools**: CVar-based visualization (phase timeline, queue state, execution stats)
- **Status**: Core mechanics complete, parry/evade systems next

### Character Hierarchy
```cpp
ACharacter (UE Base)
  └── ABaseCombatCharacter (implements IDamageable, ICombat, ITeamMember)
        ├── APlayerCharacter (player input, debug widget)
        └── AEnemyCharacter (AI-ready, default enemy team)
```

### Modular Configuration
Combat behavior is configured through a three-tier data asset system:
```cpp
// 1. Per-instance override (highest priority)
TargetingComponent->TargetingSettingsOverride = MyCustomSettings;

// 2. Character-type default (from CombatSettings)
CombatSettings->TargetingSettings = DA_PlayerTargeting;

// 3. Hardcoded fallback (lowest priority, always safe)
```

---

## Debugging

### CVar-Based Debug System (New in v3.0.0)

Enable debug visualization via console commands:
```
Combat.Debug.All 1         // Enable all debug visualization
Combat.Debug.Direction 1   // Direction arrows and input display
Combat.Debug.Targeting 1   // Targeting cones, soft aim assist
Combat.Debug.Weapon 1      // Weapon traces, hit points
Combat.Debug.Phase 1       // Phase indicators and timeline
Combat.Debug.Queue 1       // Action queue state
Combat.Debug.Hold 1        // Hold state tracking
Combat.Debug.LogVerbose 1  // Verbose logging
```

### Debug Visualization Features
- Color-coded phase indicators (Windup=Orange, Active=Red, Recovery=Yellow)
- Direction arrows: Blue (Camera), Green (Character), Yellow (Input), Magenta (Resolved)
- Real-time action queue state with scheduled execution times
- Soft aim assist cone visualization with target scoring
- Hold state tracking (duration, input type, ease direction)

### Log Categories
- `LogCombat` - State transitions, attack execution
- `LogCombatWarp` - Motion warping mode selection (Target vs Rotation)
- `LogTargeting` - Soft aim assist scoring, target selection
- `LogAnimation` - Montage playback issues
- `LogWeapon` - Hit detection events

### Console Commands
```
showdebug animation  // View current state, montage info
stat fps             // Performance monitoring
slomo 0.3            // Slow motion for timing verification
```

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