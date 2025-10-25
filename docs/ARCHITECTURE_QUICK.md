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
Character
├── CombatComponent (~1000 lines)
│   ├── State machine
│   ├── Attack execution
│   ├── Phase & window tracking
│   ├── Input buffering
│   ├── Posture system
│   ├── Parry detection
│   └── Combo tracking
│
├── TargetingComponent (~300 lines)
│   ├── Cone-based targeting
│   ├── Direction conversion
│   └── Motion warp setup
│
├── WeaponComponent (~200 lines)
│   ├── Socket tracing
│   └── Hit detection
│
└── HitReactionComponent (~300 lines)
    ├── Damage application
    └── Hit reactions
```

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

**Minimum (Basic Attack)**:
1. `AnimNotifyState_AttackPhase` (Windup)
2. `AnimNotifyState_AttackPhase` (Active)
3. `AnimNotify_ToggleHitDetection` (Enable)
4. `AnimNotify_ToggleHitDetection` (Disable)
5. `AnimNotifyState_AttackPhase` (Recovery)

**Optional**:
- `AnimNotifyState_ComboWindow` - Combo chains
- `AnimNotifyState_ParryWindow` - Parryable attacks
- `AnimNotifyState_HoldWindow` - Hold-and-release
- `AnimNotifyState_CancelWindow` - Cancellable moves

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

### Posture
```cpp
MaxPosture:                   100.0f
PostureRegenRate_Attacking:   50.0f  // Fastest
PostureRegenRate_NotBlocking: 30.0f
PostureRegenRate_Idle:        20.0f
GuardBreakStunDuration:       2.0f
GuardBreakRecoveryPercent:    0.5f   // 50%
```

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
├── Core/
│   ├── CombatComponent.h
│   ├── TargetingComponent.h
│   ├── WeaponComponent.h
│   └── HitReactionComponent.h
├── Data/
│   ├── AttackData.h
│   └── CombatSettings.h
├── Animation/
│   ├── SamuraiAnimInstance.h
│   ├── AnimNotifyState_AttackPhase.h
│   ├── AnimNotifyState_ParryWindow.h
│   ├── AnimNotifyState_HoldWindow.h
│   ├── AnimNotifyState_ComboWindow.h
│   └── AnimNotify_ToggleHitDetection.h
└── Interfaces/
    ├── CombatInterface.h
    └── DamageableInterface.h
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
❌ Splitting CombatComponent into fragments

---

## Design Principles

1. **Pragmatic Over Perfect** - ~1000 line component is fine
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

### Test Coverage (7 Test Suites, 45+ Assertions)

1. **StateTransitionTests** - State machine validation
2. **InputBufferingTests** - Hybrid responsive + snappy system
3. **HoldWindowTests** - Button state detection
4. **ParryDetectionTests** - Defender-side parry
5. **AttackExecutionTests** - ExecuteAttack vs ExecuteComboAttack
6. **MemorySafetyTests** - Null handling, edge cases
7. **PhasesVsWindowsTests** - Architectural separation

### Running Tests

**In Editor**:
- Window → Developer Tools → Session Frontend
- Automation tab → Filter: "KatanaCombat"

**Command Line**:
```bash
UnrealEditor.exe "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat"
```

**See** `Source/KatanaCombatTest/README.md` for complete test documentation.

---

**See [ARCHITECTURE.md](ARCHITECTURE.md) for complete details.**