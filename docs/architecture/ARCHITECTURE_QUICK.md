# Architecture Quick Reference

**Condensed technical reference for KatanaCombat system.**

Full details in [ARCHITECTURE.md](ARCHITECTURE.md)

---

## Critical Design Corrections

### 1. Attack Phases (3, Not 5)

```cpp
enum class EAttackPhase : uint8
{
    Windup,     // Startup/telegraph
    Active,     // Damage frames
    Recovery    // End lag
    // NO "Hold" or "ParryWindow" phases!
};
```

**Architecture**: 5 core components with clear separation of concerns (Combat, Targeting, Weapon, HitReaction, PairedAnimation)

### 2. Window System (Independent)

```cpp
class UCombatComponent
{
    // Phases (mutually exclusive)
    EAttackPhase CurrentAttackPhase;

    // Windows (independent, can overlap)
    bool bIsInParryWindow;      // Attacker vulnerable to parry
    bool bIsInHoldWindow;       // Animation manipulation zone
    bool bIsInComboWindow;      // Early execution allowed
    bool bIsInCancelWindow;     // Can interrupt
    int32 AllowedCancelInputs;  // Bitmask
};
```

### 3. Input Buffering (Always Active)

```
Player Input During Attack:
    ↓
ALWAYS buffered (stored)
    ↓
Combo Window Active?
├─ YES → Execute at Active end (early, "snappy")
└─ NO → Execute at Recovery end (normal, "responsive")
```

### 4. Hold Mechanics (State Check)

```
OnLightAttackPressed():
    └─ bLightAttackPressed = true

HoldWindow Starts:
    └─ Is bLightAttackPressed STILL true?
        ├─ YES → Begin hold behavior
        └─ NO → Continue normal combo
```

### 5. Parry System (Contextual Block)

```
Defender Presses Block:
    ↓
Find nearby enemies
    ↓
Enemy in parry window?
├─ YES → PARRY ACTION (no damage, counter window)
└─ NO → BLOCK ACTION (posture damage)
```

**Parry window is on ATTACKER's montage**, defender checks `ICombatInterface::IsInParryWindow()`.

### Chain Counter Ownership

- `UCombatComponent` owns input capture, queue ownership, and attack-data resolution.
- `UCombatComponent` must not enqueue successful Chain Block/attack inputs.
- `UPairedAnimationComponent` owns Chain state, retained target/context, paired counter execution, paired finisher execution, and cleanup.
- The public Chain advance API is `TryAdvanceChainCounter(UAttackData* SelectedAttackData)`. Low-level state helpers remain protected/internal unless a test explicitly names them as internal primitive coverage.

### 6. Cancel System (Bitmask)

```cpp
enum class ECancelInputFlags : uint8
{
    None         = 0,
    LightAttack  = 1 << 0,  // 1
    HeavyAttack  = 1 << 1,  // 2
    Evade        = 1 << 2,  // 4
    Block        = 1 << 3,  // 8
    Special      = 1 << 4,  // 16
};

// Check
bool CanCancelWith(ECancelInputFlags Input) const
{
    return (AllowedCancelInputs & (int32)Input) != 0;
}
```

### 7. Delegate Architecture (Centralized)

**System-wide delegates** (used across multiple components):
- Declared ONCE in `CombatTypes.h`
- Components use `UPROPERTY` only, never `DECLARE_DYNAMIC_MULTICAST_DELEGATE`

**Component-specific delegates** (used only within one component):
- Declared in component header
- Example: `FOnWeaponHit` in `WeaponComponent.h`

This distinction keeps system-wide events centralized while allowing components to expose their own specific events.

---

## Component Structure

```
BaseCombatCharacter
├── CombatComponent (~3400 lines) - Core combat system
│   ├── Timestamped input queue (last-input-wins)
│   ├── Action queue with checkpoints
│   ├── Phase management (via AnimNotify events)
│   ├── Procedural easing (10 types)
│   ├── Universal blending system
│   ├── Hold mechanics (ease-in/ease-out)
│   └── Comprehensive debug visualization
│
├── TargetingComponent (~300 lines)
│   ├── Cone-based targeting
│   ├── Soft-lock aim assist
│   ├── Direction conversion
│   └── Motion warp setup
│
├── WeaponComponent (~200 lines)
│   ├── Socket tracing
│   ├── Hit detection
│   └── Damage multipliers
│
├── HitReactionComponent (~500 lines)
│   ├── Damage application
│   ├── Directional hit reactions
│   ├── Stun/i-frame management
│   ├── Death reactions (directional + ragdoll)
│   └── Pose snapshot for recovery
│
├── PairedAnimationComponent (~1500 lines)
│   ├── Finisher execution flow
│   ├── Counter system (AC3 + Chain modes)
│   ├── Partner collision management
│   ├── Input blocking during paired animations
│   └── Counter window management
│
└── MotionWarpingComponent (UE5 built-in)
    └── Animation-driven movement warping
```

**Support Libraries**:
- `MontageUtilityLibrary` (27 functions) - Timing, easing, sections, blending

---

## Phase & Window Timeline

```
Attack Execution:

Phases (Exclusive):
┌──────────┬────────┬──────────┐
│  Windup  │ Active │ Recovery │
└──────────┴────────┴──────────┘

Windows (Overlapping):
     ┌─────┤        │          Parry Window
     └─────┤        │
┌──────────┤        │          Cancel Window
└──────────┤        │
         ┌─┴────────┴──────┐   Combo Window
         └─────────────────┘
                    ┌──────┐   Hold Window
                    └──────┘
```

---

## AnimNotify Requirements

### Required Default Attack Notifies
- `AnimNotify_AttackPhaseTransition` to Active at end of windup.
- `AnimNotify_AttackPhaseTransition` to Recovery at end of active.

Hit detection is automatic during Active phase; do not add toggle notifies for default attacks.
Default combo timing is inferred from phase transitions; do not add explicit combo-window states for normal attack chains.

### Optional Current Notifies
- `AnimNotify_HoldWindowStart` for light hold activation.
- `AnimNotifyState_ParryWindow` and `AnimNotifyState_CounterWindow` for attacker-side defensive response windows.
- `AnimNotifyState_CancelWindow` for specific cancel inputs.
- Paired animation sync/collision notifies for finishers and counters.

### Chain Counter Requirements
- Attacker montages that can be parried require `AnimNotifyState_ParryWindow`.
- Attacker montages that can be directly countered require `AnimNotifyState_CounterWindow`.
- Counter-capable `UAttackData` should set `bHasCounterVariant` and `CounterData` when a paired counter animation exists.
- Paired counter/finisher montages require paired sync/collision notifies when they depend on impact timing or partner collision suppression.
- Successful Chain Block and attack inputs are consumed only when `UPairedAnimationComponent` returns success.
- `UCombatComponent` resolves selected attack data before crossing into Chain advance.

### Do Not Default-Seed
- `AnimNotifyState_AttackPhase`
- `AnimNotify_ToggleHitDetection`
- `AnimNotifyState_HoldWindow`
- `AnimNotifyState_ComboWindow`

---

## Default Tuning Values

### Timing
```cpp
ComboInputWindow:             0.6s
ParryWindow:                  0.3s
PerfectEvadeWindow:           0.2s
CounterWindowDuration:        1.5s
ComboResetDelay:              3.0s  // TODO: Move to CombatSettings (currently hardcoded)
```

### Blending (Added 2025-11-11)
```cpp
ComboBlendOutTime:            0.1s  // Blend-out when transitioning FROM this attack
ComboBlendInTime:             0.1s  // Blend-in when this attack is TARGET of combo
ChargeLoopBlendTime:          0.3s  // Blend into charge loop (heavy attacks)
ChargeReleaseBlendTime:       0.2s  // Blend out of charge loop on release
```

### Posture (DEPRECATED)
**Note**: The posture system has been deprecated and replaced by contextual stagger mechanics. Posture values are maintained for backwards compatibility but are no longer the primary defense mechanism.

```cpp
MaxPosture:                   100.0f
PostureRegenRate_Attacking:   50.0f  // Fastest
PostureRegenRate_NotBlocking: 30.0f
PostureRegenRate_Idle:        20.0f
GuardBreakStunDuration:       2.0f
GuardBreakRecoveryPercent:    0.5f   // 50%
```

**Use instead**: `ApplyStagger()`, `IsStaggered()`, `EndStagger()` from HitReactionComponent.

### Motion Warping
```cpp
MaxWarpDistance:              400.0f
MinWarpDistance:              50.0f
DirectionalConeHalfAngle:     60.0f  // 120° total
WarpRotationSpeed:            720.0f // deg/s
```

### Damage
```cpp
LightBaseDamage:              25.0f
HeavyBaseDamage:              50.0f
MaxChargeDamageMultiplier:    2.5f
CounterDamageMultiplier:      1.5f

// Posture Damage (when blocked)
LightPostureDamage:           10.0f
HeavyPostureDamage:           25.0f
ChargedPostureDamage:         40.0f

// Parry
ParryPostureDamage:           40.0f  // To attacker
```

---

## File Structure

```
Source/KatanaCombat/Public/
├── CombatTypes.h                    # Enums, structs, DELEGATES
├── ActionQueueTypes.h               # Input/action queue structures
├── Core/
│   ├── CombatComponent.h            # Combat state, attack execution, last-input-wins queue
│   ├── TargetingComponent.h         # Soft-lock targeting, aim assist
│   ├── WeaponComponent.h            # Hit detection, weapon state
│   ├── HitReactionComponent.h       # Damage reception, hit reactions, death
│   └── PairedAnimationComponent.h   # Finishers, counters, partner tracking
├── Characters/
│   ├── BaseCombatCharacter.h        # Base class with 5 combat components
│   ├── PlayerCharacter.h            # Player-specific combat
│   └── EnemyCharacter.h             # Enemy-specific combat
├── Utilities/
│   └── MontageUtilityLibrary.h      # 27 utility functions (BP-exposed)
├── Data/
│   ├── AttackData.h                 # Attack configuration
│   ├── AttackConfiguration.h        # Attack moveset package (PDA)
│   ├── CombatSettings.h             # Global combat tuning
│   └── HitReactionSettings.h        # Hit reaction configuration
├── Animation/
│   ├── SamuraiAnimInstance.h
│   ├── AnimNotify_AttackPhaseTransition.h  # Phase transitions
│   ├── AnimNotify_HoldWindowStart.h        # Event-driven hold activation
│   ├── AnimNotifyState_ParryWindow.h
│   ├── AnimNotifyState_CounterWindow.h
│   ├── AnimNotifyState_HoldWindow.h        # Legacy/default seeding sunset
│   └── AnimNotifyState_ComboWindow.h       # Legacy/manual override only
└── Interfaces/
    ├── CombatInterface.h            # Combat state contract
    ├── DamageableInterface.h        # Damage/health contract
    └── TeamMemberInterface.h        # Team/faction contract
```

---

## Key Interfaces

### ICombatInterface
```cpp
ECombatState GetCombatState() const;
EAttackPhase GetCurrentAttackPhase() const;
bool IsAttacking() const;
bool IsBlocking() const;
bool IsInCounterWindow() const;
bool IsInParryWindow() const;  // CRITICAL for parry detection
float GetPosturePercent() const;
bool IsGuardBroken() const;
```

### IDamageableInterface
```cpp
void ApplyDamage(const FDamageInfo& DamageInfo);
float GetCurrentHealth() const;
float GetMaxHealth() const;
bool IsDead() const;
bool IsVulnerableToFinisher() const;
```

---

## Common Mistakes to Avoid

❌ Making Hold or ParryWindow an attack phase
❌ Using combo window to gate input buffering
❌ Tracking hold duration instead of button state
❌ Putting parry window on defender's animation
❌ Declaring system delegates in component headers
❌ Using TArray for cancel inputs (use bitmask)
❌ Over-engineering with 7+ components
❌ Splitting core combat logic into too many small components

---

## Design Principles

1. **Clear Component Separation** - 5 core components with distinct responsibilities
2. **Phases Are Exclusive** - Only one at a time
3. **Windows Are Independent** - Can overlap
4. **Always Buffer Input** - Windows modify timing
5. **Hold at Window Start** - Check button state
6. **Parry is Contextual** - Block becomes Parry
7. **Bitmasks for Extensibility** - Easy to add types
8. **Centralize Delegates** - CombatTypes.h only
9. **Data-Driven Tuning** - All values in assets
10. **Extensible States** - Add as needed

---

---

## Automated Testing

KatanaCombat includes a comprehensive **C++ unit test suite** (`KatanaCombatTest` module):

### Test Coverage (19 Test Suites, 368 Tests)

**Core Combat**:
1. **StateTransitionTests** - State machine validation
2. **InputBufferingTests** - Hybrid responsive + snappy system
3. **HoldWindowTests** - Button state detection
4. **ParryDetectionTests** - Defender-side parry
5. **AttackExecutionTests** - ExecuteAttack vs ExecuteComboAttack
6. **PhasesVsWindowsTests** - Architectural separation

**Components**:
7. **TargetingComponentTests** - Soft-lock targeting
8. **WeaponComponentTests** - Hit detection, equip/holster
9. **HitReactionTests** - Damage, direction, i-frames, stun

**Systems**:
10. **DamageApplicationTests** - Damage flow, resistance
11. **DeathSystemTests** - Death flag, blocking, events
12. **CombatIntegrationTests** - Multi-component integration
13. **DebugVisualizationTests** - Debug HUD system
14. **MemorySafetyTests** - Null handling, edge cases

### Running Tests

**In Editor**:
- Window → Developer Tools → Session Frontend
- Automation tab → Filter: "KatanaCombat"

**Command Line**:
```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat" -NullRHI -NoSplash
```

**See** `Source/KatanaCombatTest/README.md` for complete test documentation.

---

---

## Combat System Quick Reference

### Input Queue (Last-Input-Wins)
```cpp
struct FInputEvent
{
    EInputType Type;         // LightPress, LightRelease, HeavyPress, etc.
    float Timestamp;         // When input occurred
    bool bConsumed;          // Processed flag
};
```

### Action Queue (Checkpoint-Based)
```cpp
struct FQueuedAction
{
    UAttackData* AttackData; // Attack to execute
    float ScheduledTime;     // When to execute (checkpoint time)
    EExecutionMode Mode;     // Snap, Responsive, Immediate
};
```

### Execution Modes
- **Snap**: Execute at Active phase end (input during Windup/Active)
- **Responsive**: Execute at Recovery phase end (input during Recovery)
- **Immediate**: Execute right now (input during Idle)

### Procedural Easing Types
```cpp
enum class EEasingType : uint8
{
    Linear,
    EaseInQuad, EaseOutQuad, EaseInOutQuad,
    EaseInCubic, EaseOutCubic, EaseInOutCubic,
    EaseInExpo, EaseOutExpo, EaseInOutExpo,
    EaseInSine, EaseOutSine, EaseInOutSine
};
```

### Hold Mechanics
**Light Attacks**: Procedural ease to slowdown (0.2x playrate default)
- Timer-based bidirectional easing (60Hz updates)
- Configurable HoldSlowdownRate, FreezePlayRate, EasingType
- Explicit ease direction tracking via bIsEasingOut flag

**Heavy Attacks**: Charge loop with time-based damage scaling
- ChargeLoopSection + ChargeReleaseSection montage navigation
- Configurable ChargeTime, MaxChargeDamageMultiplier
- Smooth blending between sections (ChargeLoopBlendTime/ChargeReleaseBlendTime)

### Universal Blending
All combo transitions support configurable blend times:
```cpp
// In AttackData:
ComboBlendOutTime:   0.1s  // Blend OUT when transitioning FROM this attack
ComboBlendInTime:    0.1s  // Blend IN when this attack is combo TARGET
```

Applies to:
- Light→Light, Light→Heavy, Heavy→Any
- Hold→Directional follow-ups
- Charge loop transitions

**Tuning Examples**:
- Fast/snappy: 0.05-0.1s
- Weighty/deliberate: 0.15-0.25s
- Mixed: Light fast (0.05s), Heavy slow (0.2s)

### Debug Visualization
Enable with `CombatSettings->bDebugDraw = true`

**Displays**:
- Color-coded phase indicators (Orange/Red/Yellow)
- Action queue state with scheduled times
- Visual checkpoint timeline with window overlays
- Hold state tracking (duration, ease direction)
- Execution statistics (snap vs responsive, cancellations)

### MontageUtilityLibrary Categories
1. **Montage Time Queries**: GetCurrentMontageTime, GetMontagePlayRate
2. **Procedural Easing**: EvaluateEasing, EaseLerp, CalculateTransitionPlayRate
3. **Hold Mechanics**: CalculateChargeLevel, GetMultiStageHoldPlayRate
4. **Section Navigation**: JumpToSectionWithBlend, GetMontageSections
5. **Window Queries**: GetActiveWindows, IsWindowActive, GetWindowTimeRemaining
6. **Blending**: CrossfadeMontage, BlendOutMontage
7. **Debug**: DrawCheckpointTimeline, LogCheckpoints

---

**See [ARCHITECTURE.md](ARCHITECTURE.md) for complete details.**
