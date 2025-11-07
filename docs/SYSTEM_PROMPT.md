# KatanaCombat System - Comprehensive AI Assistant Prompt

## Project Overview

**KatanaCombat** is an Unreal Engine 5.6 C++ combat system project focused on creating a deep, technical melee combat framework inspired by character action games (Devil May Cry, Bayonetta) and precision timing systems (Sekiro, Sifu). The system emphasizes:

- **Hybrid Combo System**: Blending responsive (input buffering) and snappy (animation canceling) attack chains
- **Posture-Based Defense**: Guard meter management with guard breaks, parries, and counter windows
- **Data-Driven Design**: AttackData assets with montage section support for reusable animations
- **Animation-Driven Timing**: AnimNotify events control phase transitions, AnimNotifyStates control windows (combo, hold, parry)
- **Component-Based Architecture**: Modular, reusable components for combat, targeting, weapons, and hit reactions

---

## Core Philosophy

### 1. Component-Based Modularity
The system is built around **reusable ActorComponents** that can be attached to any character (player, enemy, NPC):
- `UCombatComponent` - State machine, attacks, posture, combos, parry/counters
- `UTargetingComponent` - Directional cone targeting, motion warping setup
- `UWeaponComponent` - Socket-based hit detection, swept sphere tracing
- `UHitReactionComponent` - Damage reception, hit reactions, hitstun management

These components communicate via:
- Direct references (cached in BeginPlay)
- Blueprint-exposed events/delegates
- Interface contracts (`ICombatInterface`, `IDamageableInterface`)

### 2. Data-Driven Attack Configuration
Attacks are defined as **UAttackData** DataAssets containing:
- Animation montage references with **section support** (multiple attacks per montage)
- Damage, posture damage, hitstun values
- Combo chains (light/heavy branches, directional follow-ups)
- Heavy attack charging parameters
- Light attack hold-and-release mechanics
- Motion warping configuration
- Timing modes (AnimNotify-driven vs manual fallbacks)

### 3. Animation-Driven Timing
Attack phases are marked with **AnimNotify_AttackPhaseTransition** events:
- **Windup**: Montage start → Active transition (implicit, auto-begins)
- **Active**: Transition event → Recovery transition (hit detection automatic)
- **Recovery**: Transition event → Montage end (implicit, auto-ends)
- **Hold** (optional): Window notify for light attack pause (separate from phases)

**Event-Based System**:
- Phases use **2 transition events** (Active, Recovery) instead of 6 NotifyStates
- Hit detection **automatically** enables/disables with Active phase (no manual toggles)
- Implicit boundaries eliminate phase gaps and timing desync
- Windows (Hold, Combo, Parry, etc.) remain as NotifyStates (duration-based, non-contiguous)

### 4. Hybrid Combo System
Combines two paradigms:
- **Responsive Path**: Input buffering during recovery, attacks execute when animation completes
- **Snappy Path**: Combo window input cancels recovery early, executes immediately

This allows both deliberate, weighty attacks and fast-paced cancel chains.

### 5. Posture System
Characters have a **posture meter (0-100)**:
- Depletes when blocking attacks
- Regenerates based on state (attacking > not blocking > idle)
- When depleted → **Guard Break** (long stun, vulnerable to finishers)
- Perfect parry → Attacker enters **Counter Window** (vulnerable, increased damage taken)

---

## Architecture Deep Dive

### State Machine (UCombatComponent)
10 combat states managed via `ECombatState` enum:
```
Idle → Attacking → Idle
  ↓                  ↑
Blocking ← → HitStunned
  ↓                  ↑
GuardBroken    (parry/counter)
```

State transitions are validated via `CanTransitionTo()` to prevent illegal transitions.

### Attack Execution Flow
1. Player presses light/heavy attack button
2. `UCombatComponent::OnLightAttackPressed()` called
3. Check `CanAttack()` (state validation, posture checks)
4. Find appropriate attack:
   - If in combo: Use `NextComboAttack` or `HeavyComboAttack`
   - If after hold: Use `DirectionalFollowUps[Direction]`
   - Otherwise: Use `DefaultLightAttack` or `DefaultHeavyAttack`
5. `ExecuteAttack(AttackData)`:
   - Set `CurrentAttackData`
   - Setup motion warping to nearest target
   - Play attack montage (section-aware if specified)
   - Transition to `Attacking` state
6. Attack montage plays with phase transitions:
   - **Montage Start** → `CurrentPhase = Windup` (implicit, automatic)
   - **AnimNotify_AttackPhaseTransition(Active)** → `HandlePhaseBegin(Active)`:
     * Automatically enables hit detection (no manual toggle needed)
     * Sets `CurrentPhase = Active`
   - **AnimNotify_AttackPhaseTransition(Recovery)** → `HandlePhaseEnd(Active)`:
     * Automatically disables hit detection
     * Processes combo window inputs (snappy path)
     * Sets `CurrentPhase = Recovery`
   - **Montage End** → `CurrentPhase = None` (implicit, automatic)
     * Processes buffered inputs (responsive path)
7. During combo window:
   - Input queued → Snappy path (cancel recovery, execute immediately)
   - OR wait for recovery to complete → Responsive path (execute buffered attack)
8. Combo chains or returns to Idle

### Hit Detection System
**UWeaponComponent** performs **swept sphere traces** from `weapon_start` to `weapon_end` sockets:
- **Automatically enabled/disabled** with Active phase (no manual toggles)
- `HandlePhaseBegin(Active)` → `WeaponComponent::EnableHitDetection()`
- `HandlePhaseEnd(Active)` → `WeaponComponent::DisableHitDetection()`
- Traces every tick while enabled
- Stores hit actors to prevent double-hitting
- Broadcasts `OnWeaponHit` event with `HitActor`, `HitResult`, `AttackData`
- Character receives event, applies damage via `IDamageableInterface::ApplyDamage()`

**NOTE**: Manual `AnimNotify_ToggleHitDetection` is deprecated - hit detection is now tied directly to Active phase timing.

### Damage Flow
1. `WeaponComponent` detects hit, fires `OnWeaponHit`
2. `ASamuraiCharacter::OnWeaponHitTarget()` handles event
3. Constructs `FHitReactionInfo` (attacker, direction, damage, stun, counter flag)
4. Calls `IDamageableInterface::ApplyDamage()` on hit actor
5. Victim's `HitReactionComponent` processes:
   - Apply damage modifiers (super armor, invulnerability, resistance)
   - Check if blocking:
     - Yes → Apply posture damage, check for guard break
     - No → Apply health damage, play hit reaction, apply hitstun
6. If counter window active → Multiply damage by counter multiplier

### Posture & Guard Break
- Posture regenerates continuously based on state:
  - Attacking: 50/s (rewards aggression)
  - Not blocking: 30/s
  - Idle: 20/s
- Blocking attack → `ApplyPostureDamage(AttackData.PostureDamage)`
- Posture reaches 0 → `TriggerGuardBreak()`:
  - Transition to `GuardBroken` state
  - Play guard break animation
  - Stun for `GuardBreakStunDuration` (2s default)
  - Recover to 50% posture after stun
  - Vulnerable to finisher attacks

### Parry & Counter Windows
**Perfect Parry** (Sekiro-style):
- Block during enemy's Windup phase (0.3s window) = Parry
- Enemy enters `CounterWindow` state (1.5s)
- Enemy is vulnerable, takes increased damage (1.5x multiplier)
- Attacker's posture recovers

**Perfect Evade** (similar concept):
- Evade during enemy's Active phase (0.2s window)
- Opens counter window on enemy

### Combo System Details
**Light Combo Chain**:
```
Light1 → Light2 → Light3 → Light4
  ↓       ↓        ↓         ↓
Heavy1  Heavy2   Heavy3   Heavy4  (Heavy branches)
```

**Hold-and-Release (Light Attacks)**:
- Hold light attack during specific attacks (marked `bCanHoldAtEnd`)
- Enter `HoldingLightAttack` state
- Release + direction → Execute `DirectionalFollowUps[Direction]`
- Optional max hold time enforcement

**Charged Heavy Attacks**:
- Hold heavy attack → Enter `ChargingHeavyAttack` state
- Charge time scales damage (up to 2.5x) and posture damage
- Animation playback slows during charge (0.5x speed)
- Release → Execute with multiplied damage

### Targeting System
**UTargetingComponent** provides **directional cone targeting**:
- 120° default cone (60° each side)
- Filters by distance, line of sight, targetable classes
- Converts `EAttackDirection` enum to world space vectors
- Integrates with `UMotionWarpingComponent` for attack chasing
- Optional target locking for lock-on systems

**Motion Warping** (via MotionWarping plugin):
- Setup warp target before attack execution
- Character smoothly warps toward target during Windup phase
- Configurable min/max distances, rotation speed
- Line of sight checks to prevent warping through walls

### Animation System Integration
**USamuraiAnimInstance** bridges animation blueprints and combat logic:
- Reads state from components every frame (NativeUpdateAnimation)
- Exposes Blueprint-readable variables:
  - `CombatState`, `CurrentPhase`, `bIsAttacking`, `bIsBlocking`
  - `Speed`, `Direction`, `bIsInAir`, `bIsInCombat`
  - `ComboCount`, `bCanCombo`, `PosturePercent`
  - `ChargePercent`, `bIsCharging`, `HitDirection`
- Routes AnimNotify callbacks to components:
  - `OnAttackPhaseBegin/End` → `CombatComponent`
  - `OnComboWindowOpened/Closed` → `CombatComponent`
  - `OnEnableHitDetection` → `WeaponComponent`

---

## Key Data Structures

### CombatTypes.h Enums
- `ECombatState`: Idle, Attacking, HoldingLightAttack, ChargingHeavyAttack, Blocking, Parrying, GuardBroken, Finishing, HitStunned, Evading, Dead
- `EAttackType`: None, Light, Heavy, Special
- `EAttackPhase`: None, Windup, Active, Recovery
  - **NOTE**: Hold, HoldWindow, CancelWindow are WINDOWS (tracked as booleans), NOT phases
- `EAttackDirection`: None, Forward, Backward, Left, Right
- `EHitReactionType`: None, Flinch, Light, Medium, Heavy, Knockback, Knockdown, Launch, Custom
- `EInputType`: None, LightAttack, HeavyAttack, Block, Evade, Special
- `ETimingFallbackMode`: AutoCalculate, RequireManualOverride, UseSafeDefaults, DisallowMontage

### Key Structs
- `FAttackPhaseTimingOverride`: Manual timing values (windup, active, recovery, hold window)
- `FBufferedInput`: Input type, direction, timestamp, consumed flag
- `FAttackPhaseTiming`: Complete phase timing (start/end times for all phases)
- `FHitReactionData`: Reaction type, stun duration, knockback force, launch force, custom montage
- `FHitReactionInfo`: Attacker, hit direction, attack data, damage, stun, counter flag, impact point
- `FHitReactionAnimSet`: Directional hit reactions (front/back/left/right montages)
- `FMotionWarpingConfig`: Warp settings (min/max distance, rotation speed, line of sight)

### Delegates

**System-wide delegates** (declared in `CombatTypes.h`):
- `FOnCombatStateChanged(ECombatState NewState)`
- `FOnAttackHit(AActor* HitActor, float Damage)`
- `FOnPostureChanged(float NewPosture)`
- `FOnGuardBroken()`
- `FOnPerfectParry(AActor* ParriedActor)`
- `FOnPerfectEvade(AActor* EvadedActor)`
- `FOnFinisherAvailable(AActor* Target)`

**Component-specific delegates** (declared in component headers):
- `FOnWeaponHit` (WeaponComponent) - Hit detection events
- `FOnDamageReceived` (HitReactionComponent) - Damage application
- `FOnHitReactionStarted` (HitReactionComponent) - Hit reaction playback
- `FOnStunBegin/FOnStunEnd` (HitReactionComponent) - Stun state changes

---

## Editor Tools & Workflow

### AttackData Asset Workflow
1. Create `UAttackData` DataAsset in Content Browser
2. Assign attack montage and section name (if using sections)
3. Configure damage, posture damage, hitstun
4. Set up combo chains (NextComboAttack, HeavyComboAttack)
5. Place notifies in montage:
   - **AnimNotify_AttackPhaseTransition(Active)**: At end of windup (where damage begins)
   - **AnimNotify_AttackPhaseTransition(Recovery)**: At end of active (where damage ends)
   - **AnimNotifyState_ComboWindow** (optional): During recovery for combo chaining
   - **AnimNotifyState_HoldWindow** (optional): During windup for charge/directional input
   - **AnimNotifyState_ParryWindow** (optional): During attacker's windup for parry detection

### UAttackDataTools (Editor Module)
Static utility functions for editor automation:
- `GeneratePhaseTransitionNotifies()`: Create Active/Recovery transition events (2 notifies)
- `GenerateComboWindowNotify()`: Add combo window during recovery
- `GenerateHoldWindowNotify()`: Add hold window during windup (for heavy attacks)
- `GenerateParryWindowNotify()`: Add parry window on attacker's windup
- `GenerateAllNotifies()`: One-click notify setup (transitions + windows)
- `ValidateMontageSection()`: Check for errors (missing section, invalid references)
- `ValidatePhaseSetup()`: Verify correct phase transition placement
- `FindSectionConflicts()`: Detect multiple attacks using same section
- `MigrateFromOldPhaseSystem()`: Convert deprecated NotifyStates to new event system
- `BatchGenerateNotifies()`: Process multiple assets at once

**Note**: Old functions `GenerateAttackPhaseNotifies()` and `GenerateHitDetectionNotifies()` are deprecated - use `GeneratePhaseTransitionNotifies()` instead.

Exposed as Blueprint-callable functions for Editor Utility Widgets.

### Montage Section Support
**Problem**: Creating unique montages for every attack is expensive.

**Solution**: Multiple attacks share one montage, differentiated by sections:
```
Montage: "Samurai_LightAttacks"
  - Section "Light1" (0.0s - 1.0s)
  - Section "Light2" (1.0s - 2.0s)
  - Section "Light3" (2.0s - 3.2s)
  - Section "Light4" (3.2s - 4.5s)

AttackData_Light1: Montage + Section "Light1"
AttackData_Light2: Montage + Section "Light2"
AttackData_Light3: Montage + Section "Light3"
AttackData_Light4: Montage + Section "Light4"
```

Each AttackData reads timing from notifies **within its section**, allowing precise per-attack control.

### Custom Details Panel (Future)
`FAttackDataCustomization` provides enhanced editor UI:
- Visual timeline showing phase layout
- One-click notify generation buttons
- Timing validation warnings
- Conflict detection highlights
- Section dropdown with preview

---

## Design Patterns & Conventions

### Naming Conventions
- **Components**: `UCombatComponent`, `UTargetingComponent`, `UWeaponComponent`
- **Interfaces**: `ICombatInterface`, `IDamageableInterface`
- **Data Assets**: `UAttackData`, `UCombatSettings`
- **Enums**: `ECombatState`, `EAttackType`, `EAttackPhase`
- **Structs**: `FHitReactionInfo`, `FAttackPhaseTiming`
- **Delegates**: `FOnCombatStateChanged`, `FOnPostureChanged`
- **AnimNotifies (Phases)**: `AnimNotify_AttackPhaseTransition`
- **AnimNotifies (Windows)**: `AnimNotifyState_ComboWindow`, `AnimNotifyState_HoldWindow`, `AnimNotifyState_ParryWindow`
- **AnimNotifies (Deprecated)**: `AnimNotifyState_AttackPhase`, `AnimNotify_ToggleHitDetection`

### Code Organization
```
Source/
  KatanaCombat/
    Public/
      Core/                 # Core combat components
        CombatComponent.h
        TargetingComponent.h
        WeaponComponent.h
        HitReactionComponent.h
      Data/                 # Data assets
        AttackData.h
        CombatSettings.h
      Animation/            # AnimNotifies and AnimInstance
        AnimNotify_AttackPhaseTransition.h          # Phase transitions (NEW)
        AnimNotifyState_ComboWindow.h               # Combo window
        AnimNotifyState_HoldWindow.h                # Hold detection window
        AnimNotifyState_ParryWindow.h               # Parry detection window
        AnimNotifyState_AttackPhase.h               # DEPRECATED - Old phase system
        AnimNotify_ToggleHitDetection.h             # DEPRECATED - Hit detection now automatic
        SamuraiAnimInstance.h
      Characters/           # Character classes
        SamuraiCharacter.h
      Interfaces/           # Interface contracts
        CombatInterface.h
        DamageableInterface.h
      CombatTypes.h         # Shared enums, structs, delegates
    Private/
      (corresponding .cpp files)
  KatanaCombatEditor/
    Public/
      AttackDataTools.h     # Editor utilities
      Customizations/       # Custom details panels (future)
    Private/
      (corresponding .cpp files)
```

### Component Dependency Flow
```
ASamuraiCharacter (coordinator)
  ├─ UCombatComponent (state machine, attacks, posture)
  │    ├─ Uses → UTargetingComponent (find targets)
  │    ├─ Uses → UWeaponComponent (hit detection control)
  │    └─ Uses → UMotionWarpingComponent (setup warps)
  ├─ UTargetingComponent (targeting logic)
  │    └─ Uses → UMotionWarpingComponent
  ├─ UWeaponComponent (hit detection)
  │    └─ Broadcasts → OnWeaponHit → Character
  └─ UHitReactionComponent (damage reception)
       └─ Uses → UCombatComponent (posture queries)
```

### Blueprint Exposure
All components expose key functions as `UFUNCTION(BlueprintCallable)`:
- Enables rapid prototyping in Blueprint
- Allows designers to extend functionality
- Facilitates AI behavior trees and state trees
- Supports UI widget bindings (health bars, combo counters)

### Event-Driven Communication
Components communicate via delegates rather than tight coupling:
- `CombatComponent::OnCombatStateChanged` → AnimInstance, UI, AI
- `WeaponComponent::OnWeaponHit` → Character applies damage
- `HitReactionComponent::OnDamageReceived` → VFX, SFX, UI
- `CombatComponent::OnGuardBroken` → Camera shake, slow-mo, finisher prompt

---

## Technical Considerations

### Performance
- Components cache references in `BeginPlay()` to avoid repeated `GetComponentByClass()` calls
- Hit detection only active during Active phase (not every frame)
- Swept sphere traces optimized with single-channel queries
- Motion warping uses plugin's optimized root motion adjustments

### Memory
- AttackData assets are lightweight (mostly pointers and scalars)
- Montage section reuse minimizes animation memory footprint
- Hit actors array cleared between attacks (no unbounded growth)

### Thread Safety
- All combat logic runs on Game Thread (standard UE actor/component lifecycle)
- No special threading concerns (blueprint-callable functions are inherently GT)

### Replication (Multiplayer Readiness)
Current implementation is **single-player focused**, but structured for future netcode:
- State machine can be replicated via `UPROPERTY(Replicated)`
- Attack execution can use `UFUNCTION(Server, Reliable)`
- Hit detection can validate on server, apply damage via RPC
- Montage playback supports `PlayMontageSimulated` for clients

---

## Common Workflows

### Adding a New Attack
1. Open attack montage in Animation Editor
2. Create new section (or use existing section range)
3. Place `AnimNotify_AttackPhaseTransition` notifies:
   - **First notify**: At end of windup, set `TransitionToPhase = Active`
   - **Second notify**: At end of active, set `TransitionToPhase = Recovery`
4. Place window notifies (optional):
   - `AnimNotifyState_ComboWindow` during recovery (for combo chaining)
   - `AnimNotifyState_HoldWindow` during windup (for heavy attack charge)
   - `AnimNotifyState_ParryWindow` during windup (for parry vulnerability)
5. Create `UAttackData` asset
6. Assign montage + section name
7. Configure damage, posture, hitstun values
8. Link into combo chain via `NextComboAttack` or `HeavyComboAttack`
9. Assign to `DefaultLightAttack` or `DefaultHeavyAttack` if starter attack

**Note**: Hit detection is automatic - no manual toggles needed! Enabled/disabled automatically with Active phase.

### Creating a Combo Chain
```
Light1 (Default) → Light2 → Light3 → Light4
  ↓                  ↓        ↓         ↓
Heavy1            Heavy2   Heavy3   Heavy4

Light1.NextComboAttack = Light2
Light1.HeavyComboAttack = Heavy1
Light2.NextComboAttack = Light3
Light2.HeavyComboAttack = Heavy2
... (and so on)
```

### Implementing an Enemy
1. Create character class inheriting `ACharacter`
2. Implement `ICombatInterface` and `IDamageableInterface`
3. Add `UCombatComponent`, `UTargetingComponent`, `UWeaponComponent`, `UHitReactionComponent`
4. Create AnimInstance inheriting `USamuraiAnimInstance`
5. Create AttackData assets for enemy attacks
6. Assign to component's `DefaultLightAttack` / `DefaultHeavyAttack`
7. Configure hit reactions in `HitReactionComponent`
8. Implement AI controller:
   - Use `ExecuteAttack()` from Behavior Tree tasks
   - Check `CanAttack()` in conditions
   - Monitor `GetCombatState()` for decision-making

### Debugging Combat Issues
Enable debug visualization:
- `CombatComponent->bDebugDraw = true` (state, phase, timing windows)
- `TargetingComponent->bDebugDraw = true` (cone, targets, distances)
- `WeaponComponent->bDebugDraw = true` (swept traces, hit points)

Check logs:
- `LogCombat` category for state transitions, attack execution
- `LogAnimation` for montage playback issues
- `LogWeapon` for hit detection events

Common issues:
- **Attacks not executing**: Check `CanAttack()` returns true, verify state machine allows transition
- **Combos not chaining**: Ensure `NextComboAttack` is set, combo window is opening (check AnimNotify placement)
- **Hits not detecting**: Verify weapon sockets exist, hit detection enabled during Active phase, trace channel configured
- **Timing feels wrong**: Adjust AnimNotifyState positions, or use manual timing overrides

---

## Future Expansion Points

### Systems to Add
- **Finisher System**: Guard-broken enemies vulnerable to cinematic finishers
- **Special Attacks**: Resource-based (stamina/meter) powerful moves
- **Weapon Switching**: Multiple weapon types with unique movesets
- **Aerial Combat**: Launchers, air combos, ground bounces
- **Cancels & Links**: Frame-tight canceling into special moves
- **Just-Frame Mechanics**: Timed inputs for enhanced effects
- **Guard Impact**: Parry-like clash system (Soulcalibur style)
- **Revenge Mode**: Low health power-up state
- **Multiplayer**: Server-authoritative combat, lag compensation

### Technical Improvements
- **Custom Details Panel**: Visual timeline editor for AttackData
- **Animation Compression**: Optimize montage memory usage
- **Hit Pause**: Frame freeze on hit impact (DMC-style hitstop)
- **Camera System**: Dynamic camera angles during attacks
- **VFX/SFX Integration**: Niagara particles, audio one-shots on hit
- **Damage Numbers**: Floating text spawners
- **Training Mode**: Frame data display, combo recorder

---

## Design Goals & Inspirations

### Goals
1. **Feel First**: Combat should feel responsive, impactful, skill-rewarding
2. **Technical Depth**: High skill ceiling with advanced techniques (cancels, perfect parries)
3. **Readable Systems**: Designers can understand and modify without diving deep into code
4. **Reusability**: Components work on any character (player, enemy, boss)
5. **Data-Driven**: Attacks defined in assets, not hardcoded

### Inspirations
- **Devil May Cry**: Snappy cancels, long combo chains, style system
- **Bayonetta**: Responsive dodge offsets, witch time counter windows
- **Sekiro**: Posture system, perfect parries, guard breaks
- **Sifu**: Responsive attack strings, hold-and-release mechanics
- **God of War**: Heavy/light attack paradigm, directional attacks
- **For Honor**: Guard stance system, directional blocking (future)
- **Monster Hunter**: Weighty heavy attacks, commitment to animations

---

## Important Notes for AI Assistant

### When Modifying Code
1. **Maintain component separation**: Don't consolidate logic into character class
2. **Preserve Blueprint exposure**: Keep `UFUNCTION(BlueprintCallable)` on key functions
3. **Follow naming conventions**: Match existing patterns for consistency
4. **Update AnimInstance variables**: If adding state, expose to Animation Blueprint
5. **Document enum additions**: New combat states need documentation
6. **Test state transitions**: Verify `CanTransitionTo()` logic for new states
7. **Validate timing modes**: Ensure AnimNotify-driven and manual timing both work

### When Adding Features
1. **Check for existing systems**: Don't duplicate targeting, hit detection, etc.
2. **Use delegates for events**: Prefer event-driven over polling
3. **Consider reusability**: Will enemies/NPCs need this too?
4. **Plan for data assets**: Can designers configure this without code?
5. **Think about editor tools**: Should `AttackDataTools` include utilities?

### When Debugging
1. **Enable debug draws**: Visual feedback is invaluable
2. **Check component initialization**: Cached references set in `BeginPlay()`?
3. **Verify AnimNotify placement**: Most timing issues stem from notify positioning
4. **Trace state machine**: Use debug logs for state transition paths
5. **Test in isolation**: Create test map with single enemy for focused testing

### When Explaining to User
1. **Reference file:line numbers**: e.g., `CombatComponent.h:245`
2. **Show state machine paths**: Visualize transitions
3. **Explain timing windows**: Use ASCII timelines
4. **Provide examples**: Reference existing attacks (Light1, Heavy1)
5. **Link to inspirations**: "Like DMC's cancel system, but..."

---

## Quick Reference

### Key Files
- `CombatComponent.h/cpp` - State machine, attack execution, posture, combos (566 lines)
- `AttackData.h/cpp` - Attack configuration data asset
- `CombatTypes.h` - Shared enums, structs, delegates (401 lines)
- `SamuraiAnimInstance.h/cpp` - Animation Blueprint bridge
- `WeaponComponent.h/cpp` - Hit detection system
- `HitReactionComponent.h/cpp` - Damage reception, hit reactions
- `TargetingComponent.h/cpp` - Directional targeting, motion warping
- `AttackDataTools.h/cpp` - Editor utilities for asset workflow

### Key Functions
- `UCombatComponent::ExecuteAttack(AttackData)` - Execute an attack
- `UCombatComponent::SetCombatState(NewState)` - Change state
- `UCombatComponent::ApplyPostureDamage(Amount)` - Damage posture
- `UWeaponComponent::EnableHitDetection()` - Start tracing
- `UHitReactionComponent::ApplyDamage(HitInfo)` - Receive damage
- `UTargetingComponent::FindTarget(Direction)` - Find nearest enemy

### Key Events
- `OnCombatStateChanged` - State machine transitions
- `OnWeaponHit` - Weapon trace detected hit
- `OnDamageReceived` - Actor took damage
- `OnGuardBroken` - Posture depleted
- `OnPerfectParry` - Successful parry executed
- `OnComboCountChanged` - Combo chain progressed

### Plugins Required
- MotionWarping (enabled in .uproject)
- StateTree (enabled, for AI)
- GameplayStateTree (enabled, for AI)

### Engine Version
- Unreal Engine 5.6
- C++20 compilation
- Enhanced Input System

---

## Summary

**KatanaCombat** is a modular, component-based combat system for Unreal Engine 5.6 emphasizing:

1. **Deep combat mechanics** (posture, parries, combos, counters)
2. **Data-driven design** (AttackData assets, montage sections)
3. **Event-based phase timing** (AnimNotify transitions with automatic hit detection)
4. **Reusable components** (player and enemies share same systems)
5. **Editor-friendly workflow** (tools for automation, validation, batch operations, migration)

The system is structured for extensibility, supporting future features like multiplayer, aerial combat, finishers, and special attacks while maintaining clean separation of concerns and designer-friendly workflows.

When working on this project, prioritize **feel**, **clarity**, and **reusability**. Combat should be fun to play, easy to understand, and simple to extend.
