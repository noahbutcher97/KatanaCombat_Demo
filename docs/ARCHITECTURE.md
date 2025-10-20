# KatanaCombat Architecture

**Complete technical deep dive into the combat system**

This document provides comprehensive technical details on the KatanaCombat system architecture, design patterns, and implementation details.

---

## Table of Contents

1. [Core Philosophy](#core-philosophy)
2. [Component Architecture](#component-architecture)
3. [Attack Phase System](#attack-phase-system)
4. [Window System](#window-system)
5. [State Machine](#state-machine)
6. [Input Buffering](#input-buffering)
7. [Combo System](#combo-system)
8. [Posture System](#posture-system)
9. [Parry & Counter System](#parry--counter-system)
10. [Hit Detection](#hit-detection)
11. [Damage Flow](#damage-flow)
12. [Targeting System](#targeting-system)
13. [Motion Warping](#motion-warping)
14. [Animation Integration](#animation-integration)
15. [Data Structures](#data-structures)
16. [Editor Tools](#editor-tools)
17. [Design Patterns](#design-patterns)

---

## Core Philosophy

### Pragmatic Over Perfect
The system prioritizes **functional consolidation** over architectural purity:
- CombatComponent is intentionally ~1000 lines - combat flow logic is tightly coupled
- 4 core components instead of 7+ fragmented ones
- Complexity where it matters, simplicity everywhere else

### Phases vs Windows (CRITICAL DISTINCTION)
**Phases** (Exclusive - only one at a time):
- Windup
- Active
- Recovery

**Windows** (Independent - can overlap phases):
- Parry Window (attacker vulnerable to being parried)
- Hold Window (animation manipulation zone)
- Combo Window (early execution allowed)
- Cancel Window (can interrupt with specific inputs)

### Input Always Buffered
- Input is **ALWAYS** buffered during attacks
- Combo window modifies **WHEN** execution happens, not **WHETHER**
- Snappy path: Combo window open → cancel recovery, execute immediately
- Responsive path: Combo window closed → wait for recovery end

### Parry is Contextual Block
- Parry is NOT a separate input action
- Block pressed while enemy in parry window = Parry action
- Defender-side detection: `ICombatInterface::IsInParryWindow()` check on nearby attackers
- Parry window is on **ATTACKER's montage**, not defender's

### Data-Driven Tuning
All values configurable via Data Assets:
- AttackData: Per-attack configuration
- CombatSettings: Global tuning values
- No hardcoded magic numbers in C++

---

## Component Architecture

### Overview
```
Character (ASamuraiCharacter / ABaseCharacter)
├── CombatComponent (~1000 lines)
│   ├── State machine (10 combat states)
│   ├── Attack execution and timing
│   ├── Phase & window tracking
│   ├── Input buffering (responsive path)
│   ├── Combo queuing (snappy path)
│   ├── Posture system
│   ├── Parry detection
│   └── Charge/hold mechanics
│
├── TargetingComponent (~300 lines)
│   ├── Cone-based directional targeting
│   ├── Direction enum → world vector conversion
│   ├── Motion warp setup
│   └── Optional target locking
│
├── WeaponComponent (~200 lines)
│   ├── Socket-based swept sphere tracing
│   ├── Hit detection enable/disable
│   ├── Hit actor tracking (prevent double-hits)
│   └── OnWeaponHit event broadcasting
│
└── HitReactionComponent (~300 lines)
    ├── Damage application
    ├── Damage modifiers (armor, resistance)
    ├── Posture damage processing
    ├── Hit reaction playback
    └── Hitstun management
```

### Component Dependencies
```
ASamuraiCharacter (coordinator)
  ├─ UCombatComponent
  │    ├─ Uses → UTargetingComponent (find targets)
  │    ├─ Uses → UWeaponComponent (hit detection control)
  │    └─ Uses → UMotionWarpingComponent (setup warps)
  ├─ UTargetingComponent
  │    └─ Uses → UMotionWarpingComponent
  ├─ UWeaponComponent
  │    └─ Broadcasts → OnWeaponHit → Character
  └─ UHitReactionComponent
       └─ Uses → UCombatComponent (posture queries)
```

### Why Consolidation in CombatComponent?
Combat flow is inherently sequential and stateful:
1. Player presses attack button
2. Check state machine (can attack?)
3. Find appropriate attack (combo? hold follow-up? default?)
4. Setup motion warp
5. Play montage
6. Track phase transitions
7. Handle input buffering
8. Process combo windows
9. Manage posture regen
10. Detect parry opportunities

Splitting this into 5+ components would require constant inter-component messaging and duplicate state tracking. The ~1000 line CombatComponent keeps related logic together.

---

## Attack Phase System

### Phase Definitions
```cpp
enum class EAttackPhase : uint8
{
    None,        // Not attacking
    Windup,      // Startup/telegraph
    Active,      // Damage frames
    Recovery     // End lag
    // NOTE: Hold, HoldWindow, CancelWindow are NOT phases!
};
```

### Phase Timeline
```
Attack Execution:

Phases (Exclusive):
┌──────────┬────────┬──────────┐
│  Windup  │ Active │ Recovery │
└──────────┴────────┴──────────┘
0.0s      0.3s    0.5s       1.0s

Properties:
- Only ONE phase active at a time
- Sequential progression (no skipping)
- Defined by AnimNotifyState_AttackPhase in montage
```

### Phase Callbacks
```cpp
// Called by AnimNotifyState_AttackPhase
void UCombatComponent::OnAttackPhaseBegin(EAttackPhase Phase)
{
    CurrentPhase = Phase;

    switch(Phase)
    {
        case EAttackPhase::Windup:
            // Motion warping active
            // Attacker is vulnerable to parry (if parry window also active)
            break;

        case EAttackPhase::Active:
            // Enable hit detection
            if (WeaponComponent)
                WeaponComponent->EnableHitDetection();
            break;

        case EAttackPhase::Recovery:
            // Open combo window (if configured)
            // Process buffered inputs
            // Check for early execution (snappy path)
            break;
    }
}

void UCombatComponent::OnAttackPhaseEnd(EAttackPhase Phase)
{
    switch(Phase)
    {
        case EAttackPhase::Active:
            // Disable hit detection
            if (WeaponComponent)
                WeaponComponent->DisableHitDetection();
            break;

        case EAttackPhase::Recovery:
            // Process queued combo (if any)
            // Or execute buffered input (responsive path)
            ProcessRecoveryComplete();
            break;
    }

    CurrentPhase = EAttackPhase::None;
}
```

---

## Window System

### Window Types
Windows are **independent** boolean flags that can overlap with phases:

```cpp
class UCombatComponent
{
    // Windows (independent, can overlap)
    bool bIsInParryWindow;      // Attacker vulnerable to parry
    bool bIsInHoldWindow;       // Animation manipulation zone
    bool bIsInComboWindow;      // Early execution allowed
    bool bIsInCancelWindow;     // Can interrupt
    int32 AllowedCancelInputs;  // Bitmask of allowed inputs
};
```

### Window Timeline (Example)
```
Attack Execution:

Phases:
┌──────────┬────────┬──────────┐
│  Windup  │ Active │ Recovery │
└──────────┴────────┴──────────┘

Windows (can overlap):
     ┌─────┤        │          Parry Window
     └─────┤        │          (0.0-0.3s)
┌──────────┤        │          Cancel Window
└──────────┤        │          (0.0-0.3s)
         ┌─┴────────┴──────┐   Combo Window
         └─────────────────┘   (0.3-0.9s)
                    ┌──────┐   Hold Window
                    └──────┘   (0.5-0.7s)
```

### Parry Window
**Purpose**: Marks when attacker is vulnerable to being parried

**Implementation**:
```cpp
// Set via AnimNotifyState_ParryWindow in attacker's montage
void UCombatComponent::OpenParryWindow(float Duration)
{
    bIsInParryWindow = true;

    GetWorld()->GetTimerManager().SetTimer(
        ParryWindowTimer,
        this,
        &UCombatComponent::CloseParryWindow,
        Duration,
        false
    );
}

// Defender checks this via ICombatInterface
bool UCombatComponent::IsInParryWindow() const
{
    return bIsInParryWindow;
}
```

**Defender-Side Detection**:
```cpp
void UCombatComponent::OnBlockPressed()
{
    // Find nearby attackers
    TArray<AActor*> NearbyEnemies;
    TargetingComponent->GetAllTargetsInRange(NearbyEnemies);

    // Check if any are in parry window
    for (AActor* Enemy : NearbyEnemies)
    {
        if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Enemy))
        {
            if (CombatInterface->IsInParryWindow())
            {
                // PARRY ACTION
                TryParry();
                return;
            }
        }
    }

    // BLOCK ACTION (no parry opportunity)
    StartBlocking();
}
```

### Hold Window
**Purpose**: Pause animation for directional input (light attack hold-and-release)

**Implementation**:
```cpp
// Set via AnimNotifyState_HoldWindow in montage
void UCombatComponent::OpenHoldWindow(float Duration)
{
    bIsInHoldWindow = true;

    // Check if light attack button is STILL HELD
    if (bLightAttackHeld)
    {
        // Enter hold state
        SetCombatState(ECombatState::HoldingLightAttack);

        // Pause animation
        if (AnimInstance)
            AnimInstance->Montage_Pause();
    }

    GetWorld()->GetTimerManager().SetTimer(
        HoldWindowTimer,
        this,
        &UCombatComponent::CloseHoldWindow,
        Duration,
        false
    );
}

// On button release during hold
void UCombatComponent::OnLightAttackReleased()
{
    bLightAttackHeld = false;

    if (CurrentState == ECombatState::HoldingLightAttack)
    {
        // Resume animation
        if (AnimInstance)
            AnimInstance->Montage_Resume();

        // Get directional input
        EAttackDirection Direction = GetAttackDirectionFromWorldDirection(
            GetWorldSpaceMovementInput()
        );

        // Execute directional follow-up
        if (CurrentAttackData && CurrentAttackData->DirectionalFollowUps.Contains(Direction))
        {
            ExecuteDirectionalFollowUp(Direction);
        }
    }
}
```

### Combo Window
**Purpose**: Allow early execution of next attack (snappy path)

**Implementation**:
```cpp
// Set via AnimNotifyState_ComboWindow in montage
void UCombatComponent::OpenComboWindow(float Duration)
{
    bCanCombo = true;

    GetWorld()->GetTimerManager().SetTimer(
        ComboWindowTimer,
        this,
        &UCombatComponent::CloseComboWindow,
        Duration,
        false
    );

    // Check if player already buffered an input
    if (bLightAttackBuffered || bHeavyAttackBuffered)
    {
        // SNAPPY PATH: Cancel recovery, execute immediately
        CancelRecoveryAndExecuteCombo();
    }
}

void UCombatComponent::CloseComboWindow()
{
    bCanCombo = false;
}
```

### Cancel Window
**Purpose**: Allow interrupting attack with specific inputs (DMC-style cancels)

**Implementation**:
```cpp
// Bitmask for extensibility
enum class ECancelInputFlags : uint8
{
    None         = 0,
    LightAttack  = 1 << 0,  // 1
    HeavyAttack  = 1 << 1,  // 2
    Evade        = 1 << 2,  // 4
    Block        = 1 << 3,  // 8
    Special      = 1 << 4,  // 16
};

// Set via AnimNotifyState_CancelWindow in montage
void UCombatComponent::OpenCancelWindow(int32 AllowedInputs, float Duration)
{
    bIsInCancelWindow = true;
    AllowedCancelInputs = AllowedInputs;

    GetWorld()->GetTimerManager().SetTimer(
        CancelWindowTimer,
        this,
        &UCombatComponent::CloseCancelWindow,
        Duration,
        false
    );
}

bool UCombatComponent::CanCancelWith(ECancelInputFlags Input) const
{
    return bIsInCancelWindow && (AllowedCancelInputs & (int32)Input) != 0;
}

// Example usage
void UCombatComponent::OnEvadePressed()
{
    if (CanCancelWith(ECancelInputFlags::Evade))
    {
        // Cancel attack, execute evade immediately
        StopCurrentAttack();
        ExecuteEvade();
    }
    else
    {
        // Buffer evade for later
        bEvadeBuffered = true;
    }
}
```

---

## State Machine

### States
```cpp
enum class ECombatState : uint8
{
    Idle,                // Default, can initiate actions
    Attacking,           // Executing attack (any phase)
    HoldingLightAttack,  // Paused in hold window
    ChargingHeavyAttack, // Holding heavy button, charging
    Blocking,            // Actively blocking
    Parrying,            // Perfect parry animation playing
    GuardBroken,         // Posture depleted, stunned
    Finishing,           // Executing finisher on guard-broken enemy
    HitStunned,          // Taking hitstun from hit
    Evading,             // Dodge/roll active
    Dead                 // Character died
};
```

### State Transition Rules
```cpp
bool UCombatComponent::CanTransitionTo(ECombatState NewState) const
{
    // Dead is permanent
    if (CurrentState == ECombatState::Dead)
        return false;

    // Can always die
    if (NewState == ECombatState::Dead)
        return true;

    // Specific transition rules
    switch (CurrentState)
    {
        case ECombatState::Idle:
            return true; // Can transition anywhere from idle

        case ECombatState::Attacking:
            // Can cancel into evade if cancel window allows
            if (NewState == ECombatState::Evading && CanCancelWith(ECancelInputFlags::Evade))
                return true;
            // Can be interrupted by hit
            if (NewState == ECombatState::HitStunned)
                return true;
            // Can transition to hold state
            if (NewState == ECombatState::HoldingLightAttack)
                return true;
            // Otherwise must complete attack
            return false;

        case ECombatState::Blocking:
            // Can be parried
            if (NewState == ECombatState::Parrying)
                return true;
            // Can be guard broken
            if (NewState == ECombatState::GuardBroken)
                return true;
            // Can release block
            if (NewState == ECombatState::Idle)
                return true;
            return false;

        case ECombatState::GuardBroken:
            // Can be finished
            if (NewState == ECombatState::Dead || NewState == ECombatState::HitStunned)
                return true;
            // Otherwise must wait for recovery timer
            return false;

        case ECombatState::HitStunned:
            // Can be comboed
            if (NewState == ECombatState::HitStunned)
                return true;
            // Can recover to idle
            if (NewState == ECombatState::Idle)
                return true;
            return false;

        // ... other states
    }

    return false;
}
```

### State Diagram
```
              ┌─────────┐
         ┌────┤  IDLE   ├────┐
         │    └────┬────┘    │
         │         │         │
    ┌────▼───┐ ┌──▼──────┐ ┌▼────────┐
    │BLOCKING│ │ATTACKING│ │EVADING  │
    └────┬───┘ └──┬──────┘ └─────────┘
         │        │
    ┌────▼────┐ ┌▼──────────┐
    │PARRYING │ │HIT STUNNED│
    └─────────┘ └───────────┘
         │           │
    ┌────▼───────────▼─┐
    │  GUARD BROKEN    │
    └──────────────────┘
              │
         ┌────▼────┐
         │  DEAD   │
         └─────────┘
```

---

## Input Buffering

### Core Principle
**Input is ALWAYS buffered during attacks**. The combo window only affects **when** the buffered input executes, not **whether** it's stored.

### Buffering Flow
```
Player Input During Attack:
    ↓
ALWAYS store in buffer
    ↓
Combo Window Active?
├─ YES → SNAPPY PATH
│   └─ Cancel recovery at combo window start
│   └─ Execute buffered input immediately
│
└─ NO → RESPONSIVE PATH
    └─ Wait for recovery to complete
    └─ Execute buffered input at recovery end
```

### Implementation
```cpp
class UCombatComponent
{
private:
    // Responsive path buffers (simple flags)
    bool bLightAttackBuffered = false;
    bool bHeavyAttackBuffered = false;
    bool bEvadeBuffered = false;

    // Button hold state (for hold tracking)
    bool bLightAttackHeld = false;
    bool bHeavyAttackHeld = false;

    // Snappy path buffer (combo window)
    TArray<EInputType> ComboInputBuffer;
    bool bHasQueuedCombo = false;
};

// Input always buffered
void UCombatComponent::OnLightAttackPressed()
{
    bLightAttackHeld = true;

    if (CurrentState == ECombatState::Attacking)
    {
        // ALWAYS buffer
        bLightAttackBuffered = true;

        // If combo window is open, also queue for snappy execution
        if (bCanCombo)
        {
            QueueComboInput(EInputType::LightAttack);
        }
    }
    else if (CanAttack())
    {
        // Execute immediately
        ExecuteAttack(DefaultLightAttack);
    }
}

// Snappy path: Cancel recovery early
void UCombatComponent::CancelRecoveryAndExecuteCombo()
{
    if (!bHasQueuedCombo || ComboInputBuffer.Num() == 0)
        return;

    // Stop current montage at current position
    if (AnimInstance && AnimInstance->Montage_IsPlaying(nullptr))
    {
        AnimInstance->Montage_Stop(0.1f); // Fast blend out
    }

    // Clear recovery phase
    CurrentPhase = EAttackPhase::None;

    // Process queued combo immediately
    ProcessQueuedCombo();
}

// Responsive path: Wait for recovery end
void UCombatComponent::ProcessRecoveryComplete()
{
    // Check snappy path first
    if (bHasQueuedCombo)
    {
        ProcessQueuedCombo();
        return;
    }

    // Responsive path: Check buffered inputs
    ProcessBufferedInputs();
}

void UCombatComponent::ProcessBufferedInputs()
{
    if (bLightAttackBuffered)
    {
        bLightAttackBuffered = false;
        OnLightAttackPressed(); // Re-process (will execute now)
    }
    else if (bHeavyAttackBuffered)
    {
        bHeavyAttackBuffered = false;
        OnHeavyAttackPressed();
    }
    else if (bEvadeBuffered)
    {
        bEvadeBuffered = false;
        OnEvadePressed();
    }
    else
    {
        // No buffered input, return to idle
        SetCombatState(ECombatState::Idle);
    }
}
```

---

## Combo System

### Hybrid Design (Responsive + Snappy)
The system supports **both** combo paradigms simultaneously:

**Responsive Path** (God of War, Assassin's Creed):
- Input buffered during attack
- Waits for recovery to complete naturally
- Executes next attack on recovery end
- Feels weighty and deliberate

**Snappy Path** (Devil May Cry, Bayonetta):
- Input buffered during attack
- Combo window opens during recovery
- Buffered input **cancels recovery early**
- Executes next attack immediately
- Feels fast and fluid

### Combo Chain Structure
```cpp
class UAttackData
{
    // Linear combo chain (light attacks)
    UPROPERTY(EditAnywhere)
    TObjectPtr<UAttackData> NextComboAttack;

    // Heavy branch from this attack
    UPROPERTY(EditAnywhere)
    TObjectPtr<UAttackData> HeavyComboAttack;

    // Directional follow-ups after hold
    UPROPERTY(EditAnywhere)
    TMap<EAttackDirection, TObjectPtr<UAttackData>> DirectionalFollowUps;

    // Heavy directional follow-ups
    UPROPERTY(EditAnywhere)
    TMap<EAttackDirection, TObjectPtr<UAttackData>> HeavyDirectionalFollowUps;
};
```

### Example Combo Tree
```
Light1 (Default) ──→ Light2 ──→ Light3 ──→ Light4
  │                    │          │          │
  ↓                    ↓          ↓          ↓
Heavy1              Heavy2     Heavy3     Heavy4
                                  │
                                  ↓ (hold-and-release)
                    ┌─────────────┼─────────────┐
                    │             │             │
                Forward        Left          Right
              (SpinSlash) (LeftSweep)  (RightSweep)
```

### Combo Execution Logic
```cpp
void UCombatComponent::ProcessQueuedCombo()
{
    if (!CurrentAttackData || ComboInputBuffer.Num() == 0)
        return;

    EInputType InputType = ComboInputBuffer[0];
    ComboInputBuffer.RemoveAt(0);
    bHasQueuedCombo = false;

    // Find appropriate combo attack
    UAttackData* NextAttack = GetComboFromInput(InputType);

    if (NextAttack)
    {
        ComboCount++;
        ExecuteComboAttackWithHoldTracking(NextAttack, InputType);
    }
    else
    {
        // No valid combo, return to idle
        ResetCombo();
        SetCombatState(ECombatState::Idle);
    }
}

UAttackData* UCombatComponent::GetComboFromInput(EInputType InputType)
{
    if (!CurrentAttackData)
        return nullptr;

    switch (InputType)
    {
        case EInputType::LightAttack:
            return CurrentAttackData->NextComboAttack;

        case EInputType::HeavyAttack:
            return CurrentAttackData->HeavyComboAttack;

        default:
            return nullptr;
    }
}

void UCombatComponent::ExecuteComboAttackWithHoldTracking(UAttackData* NextAttack, EInputType InputType)
{
    // Track if button is held (for hold window detection)
    LastInputType = InputType;
    LastInputTime = GetWorld()->GetTimeSeconds();

    // Execute attack
    ExecuteAttack(NextAttack);
}
```

### Combo Reset
```cpp
void UCombatComponent::ResetCombo()
{
    ComboCount = 0;
    bCanCombo = false;
    bHasQueuedCombo = false;
    ComboInputBuffer.Empty();

    // Clear combo reset timer
    GetWorld()->GetTimerManager().ClearTimer(ComboResetTimer);
}

// Reset after timeout (no input for N seconds)
void UCombatComponent::ProcessRecoveryComplete()
{
    // ... buffered input processing ...

    // Start combo reset timer
    if (ComboCount > 0)
    {
        float ResetDelay = CombatSettings ? CombatSettings->ComboResetDelay : 2.0f;

        GetWorld()->GetTimerManager().SetTimer(
            ComboResetTimer,
            this,
            &UCombatComponent::ResetCombo,
            ResetDelay,
            false
        );
    }
}
```

---

## Posture System

### Design
Inspired by Sekiro, posture is a **defensive resource**:
- Depletes when blocking attacks
- Regenerates continuously based on state
- Reaching 0 → Guard Break (long stun, vulnerable)
- Perfect parry → Recover posture, enemy enters counter window

### Posture Values
```cpp
class UCombatSettings
{
    float MaxPosture = 100.0f;

    // Regeneration rates (rewards aggression)
    float PostureRegenRate_Attacking = 50.0f;    // Fastest
    float PostureRegenRate_NotBlocking = 30.0f;
    float PostureRegenRate_Idle = 20.0f;         // Slowest

    // Guard break parameters
    float GuardBreakStunDuration = 2.0f;
    float GuardBreakRecoveryPercent = 0.5f;      // 50%
};
```

### Regeneration
```cpp
void UCombatComponent::RegeneratePosture(float DeltaTime)
{
    if (CurrentPosture >= GetMaxPosture())
        return;

    float RegenRate = GetCurrentPostureRegenRate();
    CurrentPosture = FMath::Min(CurrentPosture + RegenRate * DeltaTime, GetMaxPosture());

    OnPostureChanged.Broadcast(CurrentPosture);
}

float UCombatComponent::GetCurrentPostureRegenRate() const
{
    if (!CombatSettings)
        return 20.0f;

    switch (CurrentState)
    {
        case ECombatState::Attacking:
        case ECombatState::ChargingHeavyAttack:
            return CombatSettings->PostureRegenRate_Attacking;

        case ECombatState::Blocking:
            return 0.0f; // No regen while blocking

        case ECombatState::Idle:
        case ECombatState::Evading:
            return CombatSettings->PostureRegenRate_Idle;

        default:
            return CombatSettings->PostureRegenRate_NotBlocking;
    }
}
```

### Posture Damage
```cpp
bool UCombatComponent::ApplyPostureDamage(float Amount)
{
    CurrentPosture = FMath::Max(0.0f, CurrentPosture - Amount);

    OnPostureChanged.Broadcast(CurrentPosture);

    // Check for guard break
    if (CurrentPosture <= 0.0f)
    {
        TriggerGuardBreak();
        return true;
    }

    return false;
}
```

### Guard Break
```cpp
void UCombatComponent::TriggerGuardBreak()
{
    SetCombatState(ECombatState::GuardBroken);

    // Play guard break animation
    if (AnimInstance && GuardBreakMontage)
    {
        AnimInstance->Montage_Play(GuardBreakMontage);
    }

    // Broadcast event (for UI, camera shake, etc.)
    OnGuardBroken.Broadcast();

    // Set recovery timer
    float StunDuration = CombatSettings ? CombatSettings->GuardBreakStunDuration : 2.0f;

    GetWorld()->GetTimerManager().SetTimer(
        GuardBreakRecoveryTimer,
        this,
        &UCombatComponent::RecoverFromGuardBreak,
        StunDuration,
        false
    );
}

void UCombatComponent::RecoverFromGuardBreak()
{
    // Recover to percentage of max posture
    float RecoveryPercent = CombatSettings ? CombatSettings->GuardBreakRecoveryPercent : 0.5f;
    CurrentPosture = GetMaxPosture() * RecoveryPercent;

    OnPostureChanged.Broadcast(CurrentPosture);

    // Return to idle
    SetCombatState(ECombatState::Idle);
}
```

---

## Parry & Counter System

### Parry Detection (Defender-Side)
```cpp
bool UCombatComponent::TryParry()
{
    // Find nearby attackers
    TArray<AActor*> NearbyEnemies;
    if (TargetingComponent)
    {
        TargetingComponent->GetAllTargetsInRange(NearbyEnemies);
    }

    // Check if any are in parry window
    for (AActor* Enemy : NearbyEnemies)
    {
        ICombatInterface* EnemyCombat = Cast<ICombatInterface>(Enemy);
        if (!EnemyCombat)
            continue;

        if (EnemyCombat->IsInParryWindow())
        {
            // SUCCESS: Perfect parry!
            SetCombatState(ECombatState::Parrying);

            // Play parry animation
            if (AnimInstance && ParryMontage)
            {
                AnimInstance->Montage_Play(ParryMontage);
            }

            // Recover posture
            CurrentPosture = GetMaxPosture();
            OnPostureChanged.Broadcast(CurrentPosture);

            // Open counter window on enemy
            if (UCombatComponent* EnemyCombatComp = Enemy->FindComponentByClass<UCombatComponent>())
            {
                float CounterDuration = CombatSettings ? CombatSettings->CounterWindowDuration : 1.5f;
                EnemyCombatComp->OpenCounterWindow(CounterDuration);

                // Apply posture damage to attacker
                float ParryPostureDamage = CombatSettings ? CombatSettings->ParryPostureDamage : 40.0f;
                EnemyCombatComp->ApplyPostureDamage(ParryPostureDamage);
            }

            // Broadcast success
            OnPerfectParry.Broadcast();

            return true;
        }
    }

    // FAIL: No parry opportunity, just block
    StartBlocking();
    return false;
}
```

### Counter Window
```cpp
void UCombatComponent::OpenCounterWindow(float Duration)
{
    bIsInCounterWindow = true;

    GetWorld()->GetTimerManager().SetTimer(
        CounterWindowTimer,
        this,
        &UCombatComponent::CloseCounterWindow,
        Duration,
        false
    );
}

void UCombatComponent::CloseCounterWindow()
{
    bIsInCounterWindow = false;
}

// Damage is multiplied in hit reaction component
void UHitReactionComponent::ApplyDamage(const FHitReactionInfo& HitInfo)
{
    float FinalDamage = HitInfo.Damage;

    // Counter damage multiplier
    if (CombatComponent && CombatComponent->IsInCounterWindow())
    {
        float Multiplier = CombatSettings ? CombatSettings->CounterDamageMultiplier : 1.5f;
        FinalDamage *= Multiplier;
    }

    // ... apply damage ...
}
```

---

## Hit Detection

### Weapon Component Tracing
```cpp
void UWeaponComponent::PerformWeaponTrace()
{
    if (!bHitDetectionEnabled || !OwnerMesh)
        return;

    // Get socket locations
    FVector StartLoc = OwnerMesh->GetSocketLocation(WeaponStartSocket);
    FVector EndLoc = OwnerMesh->GetSocketLocation(WeaponEndSocket);

    // First trace? Store positions and skip (no previous frame to sweep from)
    if (bFirstTrace)
    {
        PreviousStartLocation = StartLoc;
        PreviousTipLocation = EndLoc;
        bFirstTrace = false;
        return;
    }

    // Swept sphere trace from previous frame to current frame
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);
    QueryParams.bTraceComplex = false;

    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByChannel(
        Hits,
        PreviousStartLocation,
        StartLoc,
        FQuat::Identity,
        TraceChannel,
        FCollisionShape::MakeSphere(TraceRadius),
        QueryParams
    );

    // Process hits
    for (const FHitResult& Hit : Hits)
    {
        ProcessHit(Hit);
    }

    // Store current positions for next frame
    PreviousStartLocation = StartLoc;
    PreviousTipLocation = EndLoc;

    // Debug draw
    if (bDebugDraw)
    {
        DrawDebugTrace(PreviousStartLocation, StartLoc, Hits.Num() > 0, Hits.Num() > 0 ? Hits[0] : FHitResult());
    }
}

void UWeaponComponent::ProcessHit(const FHitResult& Hit)
{
    AActor* HitActor = Hit.GetActor();
    if (!HitActor || HitActor == OwnerCharacter)
        return;

    // Already hit this actor?
    if (WasActorAlreadyHit(HitActor))
        return;

    // Add to hit list
    AddHitActor(HitActor);

    // Get current attack data
    UAttackData* AttackData = GetCurrentAttackData();

    // Broadcast hit event
    OnWeaponHit.Broadcast(HitActor, Hit, AttackData);
}
```

### Hit Detection Control
```cpp
// Called by AnimNotify_ToggleHitDetection
void UWeaponComponent::EnableHitDetection()
{
    bHitDetectionEnabled = true;
    bFirstTrace = true; // Reset swept trace
}

void UWeaponComponent::DisableHitDetection()
{
    bHitDetectionEnabled = false;
}

void UWeaponComponent::ResetHitActors()
{
    HitActors.Empty();
}
```

---

## Damage Flow

### Complete Flow
```
1. WeaponComponent detects hit (swept sphere trace)
    ↓
2. OnWeaponHit event broadcast
    ↓
3. Character receives event
    ↓
4. Construct FHitReactionInfo
    - Attacker reference
    - Hit direction
    - Attack data
    - Damage amount
    - Stun duration
    - Counter flag
    ↓
5. Call IDamageableInterface::ApplyDamage() on victim
    ↓
6. HitReactionComponent processes damage
    - Check if blocking
      ├─ YES → Apply posture damage, check guard break
      └─ NO → Apply health damage, play hit reaction
    - Apply damage modifiers
    - Check counter window (multiply damage)
    - Play appropriate hit reaction animation
    - Apply hitstun
    ↓
7. Broadcast events (OnDamageReceived, etc.)
```

### Implementation
```cpp
// Character handles weapon hit
void ASamuraiCharacter::OnWeaponHitTarget(AActor* HitActor, const FHitResult& HitResult, UAttackData* AttackData)
{
    if (!HitActor || !AttackData)
        return;

    // Check if target is damageable
    IDamageableInterface* Damageable = Cast<IDamageableInterface>(HitActor);
    if (!Damageable)
        return;

    // Calculate hit direction
    FVector HitDirection = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();

    // Construct hit info
    FHitReactionInfo HitInfo;
    HitInfo.Attacker = this;
    HitInfo.HitDirection = HitDirection;
    HitInfo.AttackData = AttackData;
    HitInfo.Damage = AttackData->BaseDamage;
    HitInfo.StunDuration = AttackData->HitStunDuration;
    HitInfo.bWasCounter = CombatComponent ? CombatComponent->IsInCounterWindow() : false;
    HitInfo.ImpactPoint = HitResult.ImpactPoint;

    // Apply damage
    Damageable->ApplyDamage(HitInfo);

    // Broadcast attack hit event
    if (CombatComponent)
    {
        CombatComponent->OnAttackHit.Broadcast(HitActor, AttackData->BaseDamage);
    }
}

// HitReactionComponent processes damage
void UHitReactionComponent::ApplyDamage(const FHitReactionInfo& HitInfo)
{
    if (!OwnerCharacter)
        return;

    // Check if blocking
    if (CombatComponent && CombatComponent->IsBlocking())
    {
        // BLOCKING PATH

        // Apply posture damage
        float PostureDamage = HitInfo.AttackData ? HitInfo.AttackData->PostureDamage : 10.0f;
        bool bGuardBroken = CombatComponent->ApplyPostureDamage(PostureDamage);

        if (!bGuardBroken)
        {
            // Play block reaction
            if (AnimInstance && BlockReactionMontage)
            {
                AnimInstance->Montage_Play(BlockReactionMontage);
            }
        }
        // Guard broken state handled by CombatComponent
    }
    else
    {
        // HIT PATH

        // Calculate final damage
        float FinalDamage = HitInfo.Damage;

        // Counter damage multiplier
        if (CombatComponent && CombatComponent->IsInCounterWindow())
        {
            float Multiplier = 1.5f; // From CombatSettings
            FinalDamage *= Multiplier;
        }

        // Apply damage modifiers (armor, resistance, etc.)
        FinalDamage *= DamageResistance;

        // Subtract from health
        CurrentHealth = FMath::Max(0.0f, CurrentHealth - FinalDamage);

        // Broadcast event
        OnDamageReceived.Broadcast(FinalDamage, HitInfo.Attacker);

        // Play hit reaction
        PlayHitReaction(HitInfo);

        // Apply hitstun
        if (HitInfo.StunDuration > 0.0f && CombatComponent)
        {
            CombatComponent->SetCombatState(ECombatState::HitStunned);

            GetWorld()->GetTimerManager().SetTimer(
                HitStunRecoveryTimer,
                this,
                &UHitReactionComponent::RecoverFromHitStun,
                HitInfo.StunDuration,
                false
            );
        }

        // Check death
        if (CurrentHealth <= 0.0f)
        {
            Die();
        }
    }
}
```

---

## Targeting System

### Directional Cone Targeting
```cpp
AActor* UTargetingComponent::FindTarget(EAttackDirection Direction)
{
    // Convert direction enum to world space vector
    FVector SearchDirection = GetDirectionVector(Direction);

    // Find best target using filtering pipeline
    return FindBestTarget(SearchDirection);
}

AActor* UTargetingComponent::FindBestTarget(const FVector& Direction) const
{
    // Get all actors in range
    TArray<AActor*> Actors;
    GetActorsInRange(Actors);

    // Filter by targetable class
    FilterByTargetableClass(Actors);

    // Filter by directional cone
    FilterByCone(Actors, Direction);

    // Filter by line of sight
    if (bRequireLineOfSight)
        FilterByLineOfSight(Actors);

    // Sort by distance (nearest first)
    SortByDistance(Actors);

    // Return nearest valid target
    return Actors.Num() > 0 ? Actors[0] : nullptr;
}

void UTargetingComponent::FilterByCone(TArray<AActor*>& InOutActors, const FVector& Direction) const
{
    if (!OwnerCharacter)
        return;

    FVector OwnerLocation = OwnerCharacter->GetActorLocation();

    InOutActors.RemoveAll([&](AActor* Actor)
    {
        FVector ToTarget = (Actor->GetActorLocation() - OwnerLocation).GetSafeNormal();
        float DotProduct = FVector::DotProduct(Direction, ToTarget);
        float AngleRadians = FMath::Acos(DotProduct);
        float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);

        // Outside cone?
        return AngleDegrees > DirectionalConeAngle;
    });
}

FVector UTargetingComponent::GetDirectionVector(EAttackDirection Direction, bool bUseCamera) const
{
    if (!OwnerCharacter)
        return FVector::ForwardVector;

    FVector ForwardVector;
    FVector RightVector;

    if (bUseCamera && OwnerCharacter->GetController())
    {
        // Use camera/controller rotation
        FRotator ControlRotation = OwnerCharacter->GetController()->GetControlRotation();
        ForwardVector = ControlRotation.Vector();
        RightVector = FRotationMatrix(ControlRotation).GetScaledAxis(EAxis::Y);
    }
    else
    {
        // Use actor rotation
        ForwardVector = OwnerCharacter->GetActorForwardVector();
        RightVector = OwnerCharacter->GetActorRightVector();
    }

    switch (Direction)
    {
        case EAttackDirection::Forward:
            return ForwardVector;
        case EAttackDirection::Backward:
            return -ForwardVector;
        case EAttackDirection::Left:
            return -RightVector;
        case EAttackDirection::Right:
            return RightVector;
        case EAttackDirection::None:
        default:
            return ForwardVector;
    }
}
```

---

## Motion Warping

### Setup
```cpp
void UCombatComponent::SetupMotionWarping(UAttackData* AttackData)
{
    if (!AttackData || !AttackData->MotionWarpingConfig.bUseMotionWarping)
        return;

    if (!TargetingComponent || !MotionWarpingComponent)
        return;

    // Find target in attack direction
    AActor* Target = TargetingComponent->FindTarget(AttackData->Direction);
    if (!Target)
        return;

    // Setup motion warp
    FName WarpTargetName = AttackData->MotionWarpingConfig.MotionWarpingTargetName;
    float MaxDistance = AttackData->MotionWarpingConfig.MaxWarpDistance;

    TargetingComponent->SetupMotionWarp(Target, WarpTargetName, MaxDistance);
}

bool UTargetingComponent::SetupMotionWarp(AActor* Target, FName WarpTargetName, float MaxDistance)
{
    if (!Target || !MotionWarpingComponent || !OwnerCharacter)
        return false;

    // Calculate warp location (clamped to max distance)
    FVector WarpLocation = CalculateWarpLocation(Target, MaxDistance);

    // Calculate rotation to face target
    FVector ToTarget = (Target->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal();
    FRotator WarpRotation = ToTarget.Rotation();

    // Setup warp target
    FMotionWarpingTarget WarpTarget;
    WarpTarget.Name = WarpTargetName;
    WarpTarget.Location = WarpLocation;
    WarpTarget.Rotation = WarpRotation;

    MotionWarpingComponent->AddOrUpdateWarpTarget(WarpTarget);

    return true;
}

FVector UTargetingComponent::CalculateWarpLocation(AActor* Target, float MaxDistance) const
{
    if (!OwnerCharacter)
        return FVector::ZeroVector;

    FVector OwnerLocation = OwnerCharacter->GetActorLocation();
    FVector TargetLocation = Target->GetActorLocation();

    FVector ToTarget = TargetLocation - OwnerLocation;
    float Distance = ToTarget.Size();

    // Clamp to max distance
    if (Distance > MaxDistance)
    {
        ToTarget.Normalize();
        return OwnerLocation + ToTarget * MaxDistance;
    }

    return TargetLocation;
}
```

---

## Animation Integration

### SamuraiAnimInstance
Bridges combat system to Animation Blueprint:

```cpp
class USamuraiAnimInstance : public UAnimInstance
{
    // Combat state (read by AnimBP)
    UPROPERTY(BlueprintReadOnly)
    ECombatState CombatState;

    UPROPERTY(BlueprintReadOnly)
    EAttackPhase CurrentPhase;

    UPROPERTY(BlueprintReadOnly)
    bool bIsAttacking;

    UPROPERTY(BlueprintReadOnly)
    bool bIsBlocking;

    // Combo tracking
    UPROPERTY(BlueprintReadOnly)
    int32 ComboCount;

    UPROPERTY(BlueprintReadOnly)
    bool bCanCombo;

    // Posture
    UPROPERTY(BlueprintReadOnly)
    float PosturePercent;

    // Charging
    UPROPERTY(BlueprintReadOnly)
    bool bIsCharging;

    UPROPERTY(BlueprintReadOnly)
    float ChargePercent;

protected:
    virtual void NativeUpdateAnimation(float DeltaTime) override
    {
        Super::NativeUpdateAnimation(DeltaTime);

        ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
        if (!Character)
            return;

        // Read from CombatComponent
        if (UCombatComponent* CombatComp = Character->FindComponentByClass<UCombatComponent>())
        {
            CombatState = CombatComp->GetCombatState();
            CurrentPhase = CombatComp->GetCurrentPhase();
            bIsAttacking = CombatComp->IsAttacking();
            bIsBlocking = CombatComp->IsBlocking();
            ComboCount = CombatComp->GetComboCount();
            bCanCombo = CombatComp->CanCombo();
            PosturePercent = CombatComp->GetPosturePercent();
            // ... etc
        }
    }

    // Route AnimNotify callbacks to components
    UFUNCTION()
    void AnimNotify_AttackPhaseBegin(EAttackPhase Phase)
    {
        if (UCombatComponent* CombatComp = GetOwningComponent<UCombatComponent>())
            CombatComp->OnAttackPhaseBegin(Phase);
    }

    UFUNCTION()
    void AnimNotify_AttackPhaseEnd(EAttackPhase Phase)
    {
        if (UCombatComponent* CombatComp = GetOwningComponent<UCombatComponent>())
            CombatComp->OnAttackPhaseEnd(Phase);
    }

    UFUNCTION()
    void AnimNotify_EnableHitDetection()
    {
        if (UWeaponComponent* WeaponComp = GetOwningComponent<UWeaponComponent>())
            WeaponComp->EnableHitDetection();
    }

    UFUNCTION()
    void AnimNotify_DisableHitDetection()
    {
        if (UWeaponComponent* WeaponComp = GetOwningComponent<UWeaponComponent>())
            WeaponComp->DisableHitDetection();
    }
};
```

---

## Data Structures

### Key Enums (from CombatTypes.h)
```cpp
enum class ECombatState : uint8;          // 11 states
enum class EAttackType : uint8;           // None, Light, Heavy, Special
enum class EAttackPhase : uint8;          // None, Windup, Active, Recovery, Hold, HoldWindow, CancelWindow
enum class EAttackDirection : uint8;      // None, Forward, Backward, Left, Right
enum class EHitReactionType : uint8;      // 9 types (Flinch, Light, Medium, Heavy, etc.)
enum class EInputType : uint8;            // None, LightAttack, HeavyAttack, Block, Evade, Special
enum class ETimingFallbackMode : uint8;   // AutoCalculate, RequireManualOverride, etc.
```

### Key Structs
```cpp
struct FAttackPhaseTimingOverride
{
    float WindupDuration;
    float ActiveDuration;
    float RecoveryDuration;
    float HoldWindowStart;
    float HoldWindowDuration;
};

struct FBufferedInput
{
    EInputType Type;
    FVector2D Direction;
    float Timestamp;
    bool bConsumed;
};

struct FAttackPhaseTiming
{
    float WindupStart, WindupEnd;
    float ActiveStart, ActiveEnd;
    float RecoveryStart, RecoveryEnd;
    bool bHasHoldWindow;
    float HoldWindowStart, HoldWindowEnd;
    bool bHasCancelWindow;
    float CancelWindowStart, CancelWindowEnd;
};

struct FHitReactionInfo
{
    AActor* Attacker;
    FVector HitDirection;
    UAttackData* AttackData;
    float Damage;
    float StunDuration;
    bool bWasCounter;
    FVector ImpactPoint;
};

struct FMotionWarpingConfig
{
    bool bUseMotionWarping;
    FName MotionWarpingTargetName;
    float MinWarpDistance;
    float MaxWarpDistance;
    float WarpRotationSpeed;
    bool bWarpTranslation;
    bool bRequireLineOfSight;
};
```

### Key Delegates (from CombatTypes.h)
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttackHit, AActor*, HitActor, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPostureChanged, float, NewPosture);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGuardBroken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectParry, AActor*, ParriedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectEvade, AActor*, EvadedActor);
```

---

## Editor Tools

### AttackDataTools (Editor Module)
```cpp
class UAttackDataTools
{
public:
    // Timing automation
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static void AutoCalculateTiming(UAttackData* AttackData);

    // AnimNotify generation
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool GenerateAttackPhaseNotifies(UAttackData* AttackData);

    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool GenerateHitDetectionNotifies(UAttackData* AttackData);

    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool GenerateComboWindowNotify(UAttackData* AttackData);

    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool GenerateAllNotifies(UAttackData* AttackData);

    // Validation
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static bool ValidateMontageSection(UAttackData* AttackData, FText& OutErrorMessage);

    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static TArray<UAttackData*> FindSectionConflicts(UAttackData* AttackData);

    // Batch operations
    UFUNCTION(BlueprintCallable, Category = "Attack Data Tools")
    static void BatchGenerateNotifies(const TArray<UAttackData*>& AttackDataArray);
};
```

---

## Design Patterns

### Component Consolidation
**Problem**: Combat flow is sequential and stateful
**Solution**: Consolidate in ~1000 line CombatComponent rather than fragmenting

**Why**:
- Tight coupling between state, timing, combos, posture
- Fragmentation requires constant inter-component messaging
- Premature abstraction adds complexity without benefit

### Phases as Enum, Windows as Flags
**Problem**: Need exclusive phases AND overlapping windows
**Solution**: Phase is single enum value, windows are independent booleans

**Why**:
- Phases: Only one at a time (Windup OR Active OR Recovery)
- Windows: Can overlap (ComboWindow AND HoldWindow simultaneously)
- Clean separation of concerns

### Input Buffering + Combo Window
**Problem**: Need both responsive AND snappy combo feel
**Solution**: Always buffer input, combo window modifies execution timing

**Why**:
- Buffering ensures input is never dropped
- Combo window allows skill expression (early cancel)
- Fallback to responsive path if window missed

### Parry as Contextual Block
**Problem**: Separate parry input feels disconnected
**Solution**: Block during enemy parry window = parry action

**Why**:
- Reduces input complexity
- Rewards timing awareness
- Matches Sekiro's design (deflect = timed block)

### Data-Driven Delegates
**Problem**: Hardcoded delegates scattered across components
**Solution**: Declare ALL system delegates in CombatTypes.h, components use UPROPERTY

**Why**:
- Single source of truth
- Easy to find all system events
- No duplicate delegate declarations

### Bitmask for Cancel Inputs
**Problem**: Need extensible cancel system
**Solution**: Bitmask enum with flags

**Why**:
- Easy to add new input types
- Efficient storage (single int32)
- Simple querying: `(AllowedCancelInputs & (int32)Input) != 0`

---

## Common Workflows

### Adding a New Attack
1. Open montage in Animation Editor
2. Add AnimNotifyState_AttackPhase (Windup, Active, Recovery)
3. Add AnimNotify_ToggleHitDetection (Enable/Disable)
4. Create AttackData asset
5. Configure damage, posture, timing mode
6. Link to combo chain via NextComboAttack/HeavyComboAttack
7. Test and tune

### Creating Combo Chain
```
Light1.NextComboAttack = Light2
Light1.HeavyComboAttack = Heavy1
Light2.NextComboAttack = Light3
Light2.HeavyComboAttack = Heavy2
...
```

### Implementing Enemy
1. Create character class
2. Implement ICombatInterface, IDamageableInterface
3. Add 4 components (Combat, Targeting, Weapon, HitReaction)
4. Create AnimInstance extending SamuraiAnimInstance
5. Create AttackData assets
6. Assign to DefaultLightAttack/DefaultHeavyAttack
7. Configure hit reactions
8. AI uses ExecuteAttack() from Behavior Tree

### Debugging
```cpp
CombatComponent->bDebugDraw = true;
TargetingComponent->bDebugDraw = true;
WeaponComponent->bDebugDraw = true;
```

Check logs:
- LogCombat
- LogAnimation
- LogWeapon

---

## Performance Considerations

### Component Caching
All components cache references in BeginPlay():
```cpp
void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerCharacter = Cast<ACharacter>(GetOwner());
    AnimInstance = OwnerCharacter ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;
    TargetingComponent = OwnerCharacter ? OwnerCharacter->FindComponentByClass<UTargetingComponent>() : nullptr;
    WeaponComponent = OwnerCharacter ? OwnerCharacter->FindComponentByClass<UWeaponComponent>() : nullptr;
    MotionWarpingComponent = OwnerCharacter ? OwnerCharacter->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
}
```

### Hit Detection Optimization
- Only active during Active phase (not every frame)
- Swept sphere trace (single query per frame)
- Hit actor tracking prevents redundant damage application
- Single-channel collision queries

### Montage Section Reuse
- Multiple attacks share one montage
- Reduces animation memory footprint
- Editor tools help manage sections

---

## Future Expansion Points

### Systems to Add
- Finisher system for guard-broken enemies
- Special attacks with resource management
- Weapon switching with unique movesets
- Aerial combat (launchers, air combos)
- Frame-tight cancels into special moves
- Multiplayer (server-authoritative combat)

### Technical Improvements
- Custom Details Panel with visual timeline editor
- Hit pause (frame freeze on impact)
- Dynamic camera angles during attacks
- Training mode with frame data display

---

## Summary

**KatanaCombat** is a pragmatic, component-based melee combat system emphasizing:

1. **Hybrid combo system** (responsive + snappy paths)
2. **Posture-based defense** (guard breaks, parries, counters)
3. **Data-driven configuration** (AttackData, CombatSettings)
4. **Animation-driven timing** (AnimNotifyStates control phases)
5. **Modular components** (4 core components, reusable)

**Design Principles**:
- Pragmatic over perfect (~1000 line component is fine)
- Phases are exclusive, windows are independent
- Input always buffered, windows modify timing
- Parry is contextual block, not separate input
- Data-driven tuning, no hardcoded values

**Key Files**:
- CombatComponent.h/cpp (~1000 lines) - Main combat hub
- CombatTypes.h - ALL enums, structs, delegates
- AttackData.h - Per-attack configuration
- CombatSettings.h - Global tuning values

For API details, see [API_REFERENCE.md](API_REFERENCE.md).
For setup instructions, see [GETTING_STARTED.md](GETTING_STARTED.md).
For attack authoring, see [ATTACK_CREATION.md](ATTACK_CREATION.md).