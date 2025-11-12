# KatanaCombat API Reference

Complete API documentation for the KatanaCombat system components, data assets, and interfaces.

---

## Table of Contents

- [CombatComponent](#combatcomponent)
- [TargetingComponent](#targetingcomponent)
- [WeaponComponent](#weaponcomponent)
- [HitReactionComponent](#hitreactioncomponent)
- [AttackData](#attackdata)
- [CombatSettings](#combatsettings)
- [ICombatInterface](#icombatinterface)
- [IDamageableInterface](#idamageableinterface)
- [AnimNotifies](#animnotifies)
- [Delegates](#delegates)

---

## CombatComponent

**Class**: `UCombatComponent : UActorComponent`
**Header**: `KatanaCombat/Public/Core/CombatComponent.h`

Main combat component handling state machine, attacks, posture, combos, and parry/counter mechanics. This is the core of the combat system that consolidates tightly coupled combat flow logic.

### Configuration Properties

#### Combat Settings

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Settings")
TObjectPtr<UCombatSettings> CombatSettings;
```
Global combat settings (posture rates, timing windows, etc.). Must be assigned.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Settings")
TObjectPtr<UAttackData> DefaultLightAttack;
```
Default light attack to use when no combo is active.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Settings")
TObjectPtr<UAttackData> DefaultHeavyAttack;
```
Default heavy attack to use when no combo is active.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
bool bDebugDraw = false;
```
Enable debug visualization (state changes, timing windows, etc.).

### State Machine

#### GetCombatState
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|State")
ECombatState GetCombatState() const;
```
**Returns**: Current combat state (Idle, Attacking, Blocking, etc.)

**Example**:
```cpp
if (CombatComponent->GetCombatState() == ECombatState::Idle)
{
    // Character is ready for new actions
}
```

#### GetCurrentPhase
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|State")
EAttackPhase GetCurrentPhase() const;
```
**Returns**: Current attack phase (None, Windup, Active, Recovery, HoldWindow)

#### CanTransitionTo
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|State")
bool CanTransitionTo(ECombatState NewState) const;
```
**Parameters**:
- `NewState` - State to check

**Returns**: True if transition is valid

Check if can transition to a new state based on state machine rules.

#### SetCombatState
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|State")
void SetCombatState(ECombatState NewState);
```
**Parameters**:
- `NewState` - State to transition to

Force state change. Use carefully - prefer input functions like `OnLightAttackPressed()`. Broadcasts `OnCombatStateChanged` event.

#### IsAttacking
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|State")
bool IsAttacking() const;
```
**Returns**: True if currently in any attack state

### Attack Execution

#### ExecuteAttack
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
bool ExecuteAttack(UAttackData* AttackData);
```
**Parameters**:
- `AttackData` - Attack to perform

**Returns**: True if attack was successfully started

Execute an attack. Checks `CanAttack()` first. Sets up motion warping if configured and plays attack montage.

**Example**:
```cpp
if (CombatComponent->ExecuteAttack(CustomAttackData))
{
    UE_LOG(LogTemp, Log, TEXT("Attack started successfully"));
}
```

#### CanAttack
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Attack")
bool CanAttack() const;
```
**Returns**: True if able to attack (currently in Idle state)

#### GetCurrentAttack
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Attack")
UAttackData* GetCurrentAttack() const;
```
**Returns**: Current attack data, or nullptr if not attacking

#### StopCurrentAttack
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
void StopCurrentAttack();
```
Stop current attack immediately. Useful for interruptions.

### Combo System

#### GetComboCount
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Combo")
int32 GetComboCount() const;
```
**Returns**: Number of attacks in current combo chain

#### CanCombo
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Combo")
bool CanCombo() const;
```
**Returns**: True if in combo input window

Check if can input next combo. True during combo window (opened by AnimNotifyState_ComboWindow).

#### ResetCombo
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Combo")
void ResetCombo();
```
Reset combo chain to start. Clears combo count, input buffers, and broadcasts `OnComboCountChanged`.

#### OpenComboWindow
```cpp
void OpenComboWindow(float Duration);
```
**Parameters**:
- `Duration` - How long window stays open (seconds)

Open combo input window. Called by AnimNotifyState_ComboWindow. During this window, light/heavy inputs are queued.

#### CloseComboWindow
```cpp
void CloseComboWindow();
```
Close combo input window. Automatically called after duration expires.

### Posture System

#### GetCurrentPosture
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Posture")
float GetCurrentPosture() const;
```
**Returns**: Current posture value (0-100)

0 = guard broken, 100 = full posture.

#### GetMaxPosture
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Posture")
float GetMaxPosture() const;
```
**Returns**: Maximum posture value (from CombatSettings)

#### GetPosturePercent
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Posture")
float GetPosturePercent() const;
```
**Returns**: Posture as percentage (0-1)

0 = broken, 1 = full. Useful for UI bars.

#### ApplyPostureDamage
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Posture")
bool ApplyPostureDamage(float Amount);
```
**Parameters**:
- `Amount` - Posture to remove

**Returns**: True if guard was broken

Apply posture damage (when blocking). If posture reaches 0, triggers guard break state. Broadcasts `OnPostureChanged`.

#### IsGuardBroken
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Posture")
bool IsGuardBroken() const;
```
**Returns**: True if posture depleted (in GuardBroken state)

#### TriggerGuardBreak
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Posture")
void TriggerGuardBreak();
```
Trigger guard break state manually. Sets state to GuardBroken, broadcasts `OnGuardBroken`, and starts recovery timer.

### Blocking & Parry

#### IsBlocking
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Defense")
bool IsBlocking() const;
```
**Returns**: True if in blocking state

#### CanBlock
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Defense")
bool CanBlock() const;
```
**Returns**: True if able to block (based on current state)

#### StartBlocking
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
void StartBlocking();
```
Start blocking. Transitions to Blocking state.

#### StopBlocking
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
void StopBlocking();
```
Stop blocking. Returns to Idle state.

#### TryParry
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Defense")
bool TryParry();
```
**Returns**: True if parry was successful

Attempt perfect parry. Call during parry window for success. Broadcasts `OnPerfectParry` on success.

### Counter Windows

#### IsInCounterWindow
```cpp
UFUNCTION(BlueprintPure, Category = "Combat|Counter")
bool IsInCounterWindow() const;
```
**Returns**: True if vulnerable to counter attacks

Enemy is in counter window after being parried or perfect evaded.

#### OpenCounterWindow
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Counter")
void OpenCounterWindow(float Duration);
```
**Parameters**:
- `Duration` - How long window stays open (seconds)

Open counter window (enemy is vulnerable). Used after parry/evade.

#### CloseCounterWindow
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Counter")
void CloseCounterWindow();
```
Close counter window.

### Input Handling

#### SetMovementInput
```cpp
UFUNCTION(BlueprintCallable, Category = "Combat|Input")
void SetMovementInput(FVector2D Input);
```
**Parameters**:
- `Input` - 2D movement input (X = Right, Y = Forward)

Store movement input for directional attacks. Used for hold-and-release directional follow-ups.

**Example**:
```cpp
// In PlayerController
FVector2D MovementInput(RightValue, ForwardValue);
CombatComponent->SetMovementInput(MovementInput);
```

#### OnLightAttackPressed
```cpp
void OnLightAttackPressed();
```
Light attack button pressed. Executes default light attack if idle, or queues combo input during combo window.

#### OnLightAttackReleased
```cpp
void OnLightAttackReleased();
```
Light attack button released. Triggers hold-and-release directional follow-up if holding.

#### OnHeavyAttackPressed
```cpp
void OnHeavyAttackPressed();
```
Heavy attack button pressed. Starts charging heavy attack if idle, or queues heavy combo during combo window.

#### OnHeavyAttackReleased
```cpp
void OnHeavyAttackReleased();
```
Heavy attack button released. Releases charged heavy attack.

#### OnBlockPressed
```cpp
void OnBlockPressed();
```
Block button pressed. Starts blocking if idle.

#### OnBlockReleased
```cpp
void OnBlockReleased();
```
Block button released. Stops blocking.

#### OnEvadePressed
```cpp
void OnEvadePressed();
```
Evade button pressed. Executes evade if able, otherwise buffers input.

### Animation Callbacks

#### OnAttackPhaseBegin
```cpp
void OnAttackPhaseBegin(EAttackPhase Phase);
```
**Parameters**:
- `Phase` - Phase that is beginning

Called when attack phase begins (from AnimNotifyState_AttackPhase). Handles:
- **Active**: Enables hit detection
- **HoldWindow**: Checks if button still held, starts hold transition
- **Recovery**: Opens combo window for input

#### OnAttackPhaseEnd
```cpp
void OnAttackPhaseEnd(EAttackPhase Phase);
```
**Parameters**:
- `Phase` - Phase that is ending

Called when attack phase ends (from AnimNotifyState_AttackPhase). Handles:
- **Active**: Disables hit detection
- **HoldWindow**: Freezes animation if still holding
- **Recovery**: Processes queued combo inputs

### Events

#### OnCombatStateChanged
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
FOnCombatStateChanged OnCombatStateChanged;
```
**Parameters**:
- `NewState` - New combat state

Broadcast when combat state changes. Use for UI updates and animation changes.

#### OnPostureChanged
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPostureChanged, float, NewPosture);
UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
FOnPostureChanged OnPostureChanged;
```
**Parameters**:
- `NewPosture` - New posture value (0-100)

Broadcast when posture changes. Use for posture bar UI.

#### OnGuardBroken
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGuardBroken);
UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
FOnGuardBroken OnGuardBroken;
```
Broadcast when guard is broken. Spawn VFX, play sound, etc.

#### OnComboCountChanged
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboCountChanged, int32, NewCount, int32, MaxCombo);
UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
FOnComboCountChanged OnComboCountChanged;
```
**Parameters**:
- `NewCount` - New combo count
- `MaxCombo` - Maximum combo reached (currently always 0, reserved for future)

Broadcast when combo count changes. Use for combo counter UI.

#### OnPerfectParry
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPerfectParry);
UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
FOnPerfectParry OnPerfectParry;
```
Broadcast on successful perfect parry. Spawn slow-mo effect, VFX, etc.

#### OnPerfectEvade
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPerfectEvade);
UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
FOnPerfectEvade OnPerfectEvade;
```
Broadcast on successful perfect evade. Spawn slow-mo effect, VFX, etc.

---

## TargetingComponent

**Class**: `UTargetingComponent : UActorComponent`
**Header**: `KatanaCombat/Public/Core/TargetingComponent.h`

Handles directional cone-based targeting and motion warping setup. Reusable by player and AI for consistent targeting behavior.

### Configuration Properties

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
float DirectionalConeAngle = 60.0f;
```
Angle of directional cone for targeting (degrees, half-angle each side). Total cone = 120 degrees.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
float MaxTargetDistance = 1000.0f;
```
Maximum distance to search for targets.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
bool bRequireLineOfSight = true;
```
Require line of sight to target. Performs trace check.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
TEnumAsByte<ECollisionChannel> LineOfSightChannel = ECC_Visibility;
```
Trace channel for line of sight checks.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
TArray<TSubclassOf<AActor>> TargetableClasses;
```
Actor classes to consider as targets. Empty = all actors.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Debug")
bool bDebugDraw = false;
```
Enable debug visualization (cone, targets, distances).

### Targeting - Primary API

#### FindTarget
```cpp
UFUNCTION(BlueprintCallable, Category = "Targeting")
AActor* FindTarget(EAttackDirection Direction = EAttackDirection::None);
```
**Parameters**:
- `Direction` - Attack direction enum (None = use character forward)

**Returns**: Target actor, or nullptr if none found

Find nearest target in directional cone based on attack direction enum. Filters by distance, cone, line of sight, and targetable classes.

**Example**:
```cpp
AActor* Target = TargetingComponent->FindTarget(EAttackDirection::Forward);
if (Target)
{
    // Found target ahead
}
```

#### FindTargetInDirection
```cpp
UFUNCTION(BlueprintCallable, Category = "Targeting")
AActor* FindTargetInDirection(const FVector& DirectionVector);
```
**Parameters**:
- `DirectionVector` - World space direction (normalized)

**Returns**: Target actor, or nullptr if none found

Find nearest target using a specific world space direction vector. More flexible than enum-based version.

#### GetAllTargetsInRange
```cpp
UFUNCTION(BlueprintCallable, Category = "Targeting")
int32 GetAllTargetsInRange(TArray<AActor*>& OutTargets);
```
**Parameters**:
- `OutTargets` - Array to fill with found targets

**Returns**: Number of targets found

Get all potential targets in range (no direction filter). Useful for AOE attacks.

### Targeting - Utility Queries

#### IsTargetInCone
```cpp
UFUNCTION(BlueprintCallable, Category = "Targeting")
bool IsTargetInCone(AActor* Target, const FVector& Direction, float AngleTolerance = -1.0f) const;
```
**Parameters**:
- `Target` - Actor to check
- `Direction` - World space direction
- `AngleTolerance` - Cone half-angle (uses component default if <= 0)

**Returns**: True if target is in cone

Check if target is in directional cone.

#### HasLineOfSightTo
```cpp
UFUNCTION(BlueprintCallable, Category = "Targeting")
bool HasLineOfSightTo(AActor* Target) const;
```
**Parameters**:
- `Target` - Actor to check

**Returns**: True if can see target

Check if has line of sight to target using configured trace channel.

#### GetDirectionVector
```cpp
UFUNCTION(BlueprintCallable, Category = "Targeting")
FVector GetDirectionVector(EAttackDirection Direction, bool bUseCamera = false) const;
```
**Parameters**:
- `Direction` - Direction enum
- `bUseCamera` - Use camera forward instead of actor forward for None direction

**Returns**: World space direction vector (normalized)

Convert attack direction enum to world space vector.

#### GetAngleToTarget
```cpp
UFUNCTION(BlueprintPure, Category = "Targeting")
float GetAngleToTarget(AActor* Target) const;
```
**Parameters**:
- `Target` - Target actor

**Returns**: Angle in degrees (0 = directly ahead, positive = right, negative = left)

Get angle to target from facing direction.

#### GetDistanceToTarget
```cpp
UFUNCTION(BlueprintPure, Category = "Targeting")
float GetDistanceToTarget(AActor* Target) const;
```
**Parameters**:
- `Target` - Target actor

**Returns**: Distance in units

Get distance to target.

### Current Target Management (for lock-on systems)

#### GetCurrentTarget
```cpp
UFUNCTION(BlueprintPure, Category = "Targeting")
AActor* GetCurrentTarget() const;
```
**Returns**: Currently locked target, or nullptr

#### SetCurrentTarget
```cpp
UFUNCTION(BlueprintCallable, Category = "Targeting")
void SetCurrentTarget(AActor* NewTarget);
```
**Parameters**:
- `NewTarget` - Target to lock onto

Set current target for lock-on system.

#### ClearCurrentTarget
```cpp
UFUNCTION(BlueprintCallable, Category = "Targeting")
void ClearCurrentTarget();
```
Clear current target.

#### HasTarget
```cpp
UFUNCTION(BlueprintPure, Category = "Targeting")
bool HasTarget() const;
```
**Returns**: True if currently locked onto a target

### Motion Warping Integration

#### SetupMotionWarp
```cpp
UFUNCTION(BlueprintCallable, Category = "Targeting|Motion Warping")
bool SetupMotionWarp(AActor* Target, FName WarpTargetName = "AttackTarget", float MaxDistance = -1.0f);
```
**Parameters**:
- `Target` - Target to warp toward
- `WarpTargetName` - Name of warp target in animation (default: "AttackTarget")
- `MaxDistance` - Maximum warp distance (uses AttackData config if <= 0)

**Returns**: True if warp was set up successfully

Setup motion warping for attack toward target. Requires UMotionWarpingComponent on owner.

#### ClearMotionWarp
```cpp
UFUNCTION(BlueprintCallable, Category = "Targeting|Motion Warping")
void ClearMotionWarp(FName WarpTargetName = NAME_None);
```
**Parameters**:
- `WarpTargetName` - Name of warp target to clear (NAME_None = all)

Clear motion warp targets.

---

## WeaponComponent

**Class**: `UWeaponComponent : UActorComponent`
**Header**: `KatanaCombat/Public/Core/WeaponComponent.h`

Handles weapon-based hit detection via socket tracing. Tracks which actors have been hit to prevent multiple hits per attack.

### Configuration Properties

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
FName WeaponStartSocket = "weapon_start";
```
Socket name for weapon start (usually weapon base/handle).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
FName WeaponEndSocket = "weapon_end";
```
Socket name for weapon end (usually weapon tip).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
float TraceRadius = 5.0f;
```
Trace radius for swept sphere (adjust based on weapon size).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;
```
Trace channel for hit detection. Set to channel that hits enemies.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
bool bDebugDraw = false;
```
Enable debug visualization (trace lines, hit points).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
float DebugDrawDuration = 2.0f;
```
Debug draw duration (seconds).

### Hit Detection Control

#### EnableHitDetection
```cpp
UFUNCTION(BlueprintCallable, Category = "Weapon")
void EnableHitDetection();
```
Enable hit detection. Called by AnimNotifyState_AttackPhase (Active phase) or AnimNotify_ToggleHitDetection. Begins swept sphere tracing on tick.

#### DisableHitDetection
```cpp
UFUNCTION(BlueprintCallable, Category = "Weapon")
void DisableHitDetection();
```
Disable hit detection. Called at end of Active phase. Stops tracing and preserves hit actor list for current attack.

#### IsHitDetectionEnabled
```cpp
UFUNCTION(BlueprintPure, Category = "Weapon")
bool IsHitDetectionEnabled() const;
```
**Returns**: True if actively tracing for hits

#### ResetHitActors
```cpp
UFUNCTION(BlueprintCallable, Category = "Weapon")
void ResetHitActors();
```
Reset hit actors list. Called at start of new attack. Allows previously hit actors to be hit again by new attack.

### Socket Configuration

#### SetWeaponSockets
```cpp
UFUNCTION(BlueprintCallable, Category = "Weapon")
void SetWeaponSockets(FName StartSocket, FName EndSocket);
```
**Parameters**:
- `StartSocket` - Socket at weapon base
- `EndSocket` - Socket at weapon tip

Set weapon sockets for tracing. Useful for characters with multiple weapons.

#### GetSocketLocation
```cpp
UFUNCTION(BlueprintPure, Category = "Weapon")
FVector GetSocketLocation(FName SocketName) const;
```
**Parameters**:
- `SocketName` - Socket to query

**Returns**: Socket location in world space, or character location if socket not found

Get weapon socket location in world space.

### Hit Queries

#### WasActorAlreadyHit
```cpp
UFUNCTION(BlueprintPure, Category = "Weapon")
bool WasActorAlreadyHit(AActor* Actor) const;
```
**Parameters**:
- `Actor` - Actor to check

**Returns**: True if actor has been hit by current attack

Check if actor was already hit by current attack. Prevents double-hitting.

#### GetHitActors
```cpp
UFUNCTION(BlueprintPure, Category = "Weapon")
TArray<AActor*> GetHitActors() const;
```
**Returns**: Array of all actors hit by current attack

#### GetHitActorCount
```cpp
UFUNCTION(BlueprintPure, Category = "Weapon")
int32 GetHitActorCount() const;
```
**Returns**: Number of unique actors hit

### Events

#### OnWeaponHit
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponHit, AActor*, HitActor, const FHitResult&, HitResult, UAttackData*, AttackData);
UPROPERTY(BlueprintAssignable, Category = "Weapon")
FOnWeaponHit OnWeaponHit;
```
**Parameters**:
- `HitActor` - Actor that was hit
- `HitResult` - Full hit result from trace
- `AttackData` - Current attack data

Event broadcast when weapon hits something. Listeners can process hit, apply damage, spawn VFX, etc.

**Example Usage**:
```cpp
// In BeginPlay
WeaponComponent->OnWeaponHit.AddDynamic(this, &AMyCharacter::HandleWeaponHit);

void AMyCharacter::HandleWeaponHit(AActor* HitActor, const FHitResult& HitResult, UAttackData* AttackData)
{
    // Apply damage
    if (IDamageableInterface* Damageable = Cast<IDamageableInterface>(HitActor))
    {
        FHitReactionInfo HitInfo;
        HitInfo.Attacker = this;
        HitInfo.Damage = AttackData->BaseDamage;
        HitInfo.HitDirection = GetActorForwardVector();
        Damageable->Execute_ApplyDamage(HitActor, HitInfo);
    }

    // Spawn VFX at impact point
    UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitVFX, HitResult.ImpactPoint);
}
```

---

## HitReactionComponent

**Class**: `UHitReactionComponent : UActorComponent`
**Header**: `KatanaCombat/Public/Core/HitReactionComponent.h`

Handles receiving damage, playing hit reactions, and managing stun states. Shared by player and enemies for consistent hit response.

### Configuration - Hit Reaction Animations

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions|Light")
FHitReactionAnimSet LightHitReactions;
```
Light hit reactions (minimal stagger). Contains FrontHit, BackHit, LeftHit, RightHit montages.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions|Heavy")
FHitReactionAnimSet HeavyHitReactions;
```
Heavy hit reactions (full directional stagger).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions")
TObjectPtr<UAnimMontage> GuardBrokenMontage = nullptr;
```
Guard broken animation (posture depleted).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Reactions|Finishers")
TMap<FName, TObjectPtr<UAnimMontage>> FinisherVictimAnimations;
```
Finisher victim animations (paired with attacker finisher). Key = finisher name.

### Configuration - Damage Modifiers

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Modifiers")
bool bHasSuperArmor = false;
```
If true, attacks don't cause hitstun (still take damage). Useful for boss heavy attacks.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Modifiers")
bool bIsInvulnerable = false;
```
If true, completely immune to damage.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Modifiers", meta = (ClampMin = "0.0", ClampMax = "1.0"))
float DamageResistance = 1.0f;
```
Damage reduction multiplier (1.0 = normal, 0.5 = half damage, 0.0 = immune).

### Damage Application

#### ApplyDamage
```cpp
UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
float ApplyDamage(const FHitReactionInfo& HitInfo);
```
**Parameters**:
- `HitInfo` - Complete hit information (attacker, direction, damage, etc.)

**Returns**: Actual damage dealt (after resistances and modifiers)

Apply damage to this actor. Checks invulnerability, applies resistance, plays hit reaction, applies hitstun. Broadcasts `OnDamageReceived`.

#### PlayHitReaction
```cpp
UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
void PlayHitReaction(const FHitReactionInfo& HitInfo);
```
**Parameters**:
- `HitInfo` - Hit information including direction and intensity

Play appropriate hit reaction based on hit direction and intensity. Automatically selects correct animation from configured sets. Broadcasts `OnHitReactionStarted`.

#### ApplyHitStun
```cpp
UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
void ApplyHitStun(float Duration);
```
**Parameters**:
- `Duration` - How long to be stunned (seconds)

Apply hitstun to this actor. Broadcasts `OnStunBegin`.

#### PlayGuardBrokenReaction
```cpp
UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
void PlayGuardBrokenReaction();
```
Play guard broken reaction (when posture depletes to 0).

#### PlayFinisherVictimAnimation
```cpp
UFUNCTION(BlueprintCallable, Category = "Hit Reaction")
bool PlayFinisherVictimAnimation(FName FinisherName);
```
**Parameters**:
- `FinisherName` - Name of finisher animation to play

**Returns**: True if animation was found and played

Play finisher victim animation (paired with attacker).

### State Queries

#### IsStunned
```cpp
UFUNCTION(BlueprintPure, Category = "Hit Reaction")
bool IsStunned() const;
```
**Returns**: True if in hitstun state

#### GetRemainingStunTime
```cpp
UFUNCTION(BlueprintPure, Category = "Hit Reaction")
float GetRemainingStunTime() const;
```
**Returns**: Seconds of stun remaining

#### CanBeDamaged
```cpp
UFUNCTION(BlueprintPure, Category = "Hit Reaction")
bool CanBeDamaged() const;
```
**Returns**: True if vulnerable to damage

Checks invulnerability, super armor, and other conditions.

### Events

#### OnDamageReceived
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDamageReceived, const FHitReactionInfo&, HitInfo);
UPROPERTY(BlueprintAssignable, Category = "Hit Reaction")
FOnDamageReceived OnDamageReceived;
```
**Parameters**:
- `HitInfo` - Complete hit information

Event called when damage is received.

#### OnHitReactionStarted
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHitReactionStarted, EAttackDirection, Direction, bool, bIsHeavyHit);
UPROPERTY(BlueprintAssignable, Category = "Hit Reaction")
FOnHitReactionStarted OnHitReactionStarted;
```
**Parameters**:
- `Direction` - Hit direction (Front, Back, Left, Right)
- `bIsHeavyHit` - True if heavy hit reaction

Event called when hit reaction starts playing.

#### OnStunBegin
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStunBegin, float, Duration);
UPROPERTY(BlueprintAssignable, Category = "Hit Reaction")
FOnStunBegin OnStunBegin;
```
**Parameters**:
- `Duration` - Stun duration in seconds

Event called when stun begins.

#### OnStunEnd
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStunEnd);
UPROPERTY(BlueprintAssignable, Category = "Hit Reaction")
FOnStunEnd OnStunEnd;
```
Event called when stun ends.

---

## AttackData

**Class**: `UAttackData : UPrimaryDataAsset`
**Header**: `KatanaCombat/Public/Data/AttackData.h`

Defines a single attack's properties and behavior. Data asset that can be created in Content Browser.

### Basic Attack Properties

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Basic")
EAttackType AttackType = EAttackType::Light;
```
Attack type classification (None, Light, Heavy, Special).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Basic")
EAttackDirection Direction = EAttackDirection::None;
```
Attack direction (None, Forward, Backward, Left, Right).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Basic")
TObjectPtr<UAnimMontage> AttackMontage = nullptr;
```
Animation montage to play for this attack.

### Montage Section Support

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Montage Section")
FName MontageSection = NAME_None;
```
Which section of the montage to use for this attack (NAME_None = use entire montage). Allows multiple attacks to share one montage.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Montage Section")
bool bUseSectionOnly = false;
```
If true, only this section plays. If false, montage continues normally after section.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Montage Section")
bool bJumpToSectionStart = true;
```
If true, automatically jump to the section start when playing this attack.

### Damage & Posture

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
float BaseDamage = 25.0f;
```
Base damage dealt on hit.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
float PostureDamage = 10.0f;
```
Posture damage dealt when this attack is blocked (not parried).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
float CounterDamageMultiplier = 1.5f;
```
Multiplier applied during counter window (after successful parry/evade).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
float HitStunDuration = 0.0f;
```
Hitstun duration inflicted on hit (0 = no stun, can be countered immediately).

### Combo System

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos")
TObjectPtr<UAttackData> NextComboAttack = nullptr;
```
Next attack in light combo chain (tap light during recovery).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos")
TObjectPtr<UAttackData> HeavyComboAttack = nullptr;
```
Heavy attack branch from this attack (tap heavy during recovery).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos")
TMap<EAttackDirection, TObjectPtr<UAttackData>> DirectionalFollowUps;
```
Directional follow-ups after hold-and-release. Key = direction (Forward, Backward, Left, Right).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos")
TMap<EAttackDirection, TObjectPtr<UAttackData>> HeavyDirectionalFollowUps;
```
Heavy attacks available after directional follow-ups.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos")
float ComboInputWindow = 0.6f;
```
Time window for combo input (after this attack starts recovery).

### Combo Transition Blending (Added 2025-11-11)

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos|Blending")
float ComboBlendOutTime = 0.1f;
```
Blend-out time when transitioning FROM this attack to any combo follow-up (0 = instant).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos|Blending")
float ComboBlendInTime = 0.1f;
```
Blend-in time when this attack is the TARGET of a combo transition (0 = instant).

### Heavy Attack Charging

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack")
float MaxChargeTime = 2.0f;
```
Maximum charge time for heavy attacks.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack")
float ChargeTimeScale = 0.5f;
```
Animation playback speed during charge windup (< 1.0 = slower).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack")
float MaxChargeDamageMultiplier = 2.5f;
```
Damage multiplier at full charge.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack")
float ChargedPostureDamage = 40.0f;
```
Posture damage at full charge.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack")
FName ChargeLoopSection = NAME_None;
```
Montage section that loops during charge (NAME_None = use default animation).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack")
FName ChargeReleaseSection = NAME_None;
```
Montage section to play on release (the actual attack after charging). If NAME_None, blends to idle instead.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack")
float ChargeLoopBlendTime = 0.3f;
```
Blend time when transitioning from initial attack animation INTO charge loop section (0 = instant jump).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy Attack")
float ChargeReleaseBlendTime = 0.2f;
```
Blend time when transitioning OUT of charge loop to release section (0 = instant jump).

### Light Attack Hold & Follow-Up

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Attack")
bool bCanHoldAtEnd = true;
```
Can hold button at end of attack for directional follow-up?

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Attack")
bool bEnforceMaxHoldTime = false;
```
Enforce maximum hold time (auto-release if exceeded)?

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light Attack")
float MaxHoldTime = 1.5f;
```
Maximum hold time before auto-release.

### Timing System

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
bool bUseAnimNotifyTiming = true;
```
Primary: Use timing from AnimNotifyState_AttackPhase in montage. Recommended.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
ETimingFallbackMode TimingFallbackMode = ETimingFallbackMode::AutoCalculate;
```
What to do if AnimNotifyStates are missing from montage/section:
- **AutoCalculate**: Calculate reasonable timing from animation length
- **RequireManualOverride**: Use ManualTiming values
- **UseSafeDefaults**: Use safe default percentages
- **DisallowMontage**: Error if notifies missing

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
FAttackPhaseTimingOverride ManualTiming;
```
Manual timing values (used when bUseAnimNotifyTiming = false).

### Motion Warping

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
FMotionWarpingConfig MotionWarpingConfig;
```
Motion warping configuration for this attack. Contains:
- `bUseMotionWarping` - Enable motion warping?
- `MotionWarpingTargetName` - Warp target name (must match animation)
- `MinWarpDistance` - Minimum distance before warping
- `MaxWarpDistance` - Maximum chase distance
- `WarpRotationSpeed` - Rotation speed (degrees/sec)
- `bWarpTranslation` - Warp position?
- `bRequireLineOfSight` - Require LOS to target?

### Runtime Queries

#### GetSectionTimeRange
```cpp
UFUNCTION(BlueprintCallable, Category = "Attack Data")
void GetSectionTimeRange(float& OutStart, float& OutEnd) const;
```
**Parameters**:
- `OutStart` - Section start time
- `OutEnd` - Section end time

Get the time range of the specified montage section.

#### GetSectionLength
```cpp
UFUNCTION(BlueprintPure, Category = "Attack Data")
float GetSectionLength() const;
```
**Returns**: Length of target section (or entire montage if no section specified)

#### HasValidNotifyTimingInSection
```cpp
UFUNCTION(BlueprintPure, Category = "Attack Data")
bool HasValidNotifyTimingInSection() const;
```
**Returns**: True if attack has valid AnimNotifyStates in its target section/montage

Check if this attack has valid AnimNotifyStates for timing.

#### GetEffectiveTiming
```cpp
UFUNCTION(BlueprintCallable, Category = "Attack Data")
void GetEffectiveTiming(float& OutWindup, float& OutActive, float& OutRecovery) const;
```
**Parameters**:
- `OutWindup` - Windup duration
- `OutActive` - Active duration
- `OutRecovery` - Recovery duration

Get calculated timing based on current settings (notifies or fallback).

---

## CombatSettings

**Class**: `UCombatSettings : UPrimaryDataAsset`
**Header**: `KatanaCombat/Public/Data/CombatSettings.h`

Global combat tuning values. Data asset for configuring posture, timing, and other combat parameters.

### Posture System

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
float MaxPosture = 100.0f;
```
Maximum posture value.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
float PostureRegenRate_Attacking = 50.0f;
```
Posture regeneration while attacking (rewards aggression).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
float PostureRegenRate_NotBlocking = 30.0f;
```
Posture regeneration while not blocking (neutral stance).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
float PostureRegenRate_Idle = 20.0f;
```
Posture regeneration while idle (passive recovery).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
float GuardBreakStunDuration = 2.0f;
```
How long to stun when posture breaks.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
float GuardBreakRecoveryPercent = 0.5f;
```
Percentage of max posture recovered after guard break (0.5 = 50%).

### Timing Windows

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
float ComboInputWindow = 0.6f;
```
Time window to input next combo (from start of recovery).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
float ParryWindow = 0.3f;
```
Perfect parry timing window (during enemy windup).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
float PerfectEvadeWindow = 0.2f;
```
Perfect evade timing window (during enemy active phase).

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
float CounterWindowDuration = 1.5f;
```
How long counter window stays open after parry/evade.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
float CounterDamageMultiplier = 1.5f;
```
Damage multiplier during counter window.

### Motion Warping Defaults

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
float DefaultMaxWarpDistance = 400.0f;
```
Default maximum warp distance.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
float DefaultMinWarpDistance = 50.0f;
```
Default minimum warp distance.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
float DefaultDirectionalConeAngle = 60.0f;
```
Default directional cone angle.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion Warping")
float DefaultWarpRotationSpeed = 720.0f;
```
Default warp rotation speed (degrees/sec).

### Hit Reaction Defaults

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reactions")
float LightAttackStunDuration = 0.0f;
```
Default stun duration for light attacks.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reactions")
float HeavyAttackStunDuration = 0.5f;
```
Default stun duration for heavy attacks.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reactions")
float ChargedAttackStunDuration = 1.0f;
```
Default stun duration for charged attacks.

### Damage Defaults

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
float LightBaseDamage = 25.0f;
```
Default base damage for light attacks.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
float HeavyBaseDamage = 50.0f;
```
Default base damage for heavy attacks.

### Posture Damage Defaults (when blocked)

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture Damage")
float LightPostureDamage = 10.0f;
```
Default posture damage for light attacks.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture Damage")
float HeavyPostureDamage = 25.0f;
```
Default posture damage for heavy attacks.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture Damage")
float ChargedPostureDamage = 40.0f;
```
Default posture damage for charged attacks.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture Damage")
float ParryPostureDamage = 40.0f;
```
Posture damage dealt to attacker on successful parry.

---

## ICombatInterface

**Interface**: `ICombatInterface`
**Header**: `KatanaCombat/Public/Interfaces/CombatInterface.h`

Interface for actors that can perform attacks. Provides contract for combat actions and state queries.

### Methods

#### CanPerformAttack
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
bool CanPerformAttack() const;
```
**Returns**: True if able to attack

Can this actor perform an attack right now?

#### GetCombatState
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
ECombatState GetCombatState() const;
```
**Returns**: Current combat state

Get current combat state.

#### IsAttacking
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
bool IsAttacking() const;
```
**Returns**: True if attacking

Is currently executing an attack?

#### GetCurrentAttack
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
UAttackData* GetCurrentAttack() const;
```
**Returns**: Current attack data, or nullptr if not attacking

Get the current attack being performed.

#### GetCurrentPhase
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
EAttackPhase GetCurrentPhase() const;
```
**Returns**: Current phase, or None if not attacking

Get current attack phase.

#### OnEnableHitDetection
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
void OnEnableHitDetection();
```
Called when hit detection should be enabled (from AnimNotify_ToggleHitDetection).

#### OnDisableHitDetection
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
void OnDisableHitDetection();
```
Called when hit detection should be disabled (from AnimNotify_ToggleHitDetection).

#### OnAttackPhaseBegin
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
void OnAttackPhaseBegin(EAttackPhase Phase);
```
**Parameters**:
- `Phase` - Which phase is beginning

Called when an attack phase begins (from AnimNotifyState_AttackPhase).

#### OnAttackPhaseEnd
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
void OnAttackPhaseEnd(EAttackPhase Phase);
```
**Parameters**:
- `Phase` - Which phase is ending

Called when an attack phase ends (from AnimNotifyState_AttackPhase).

---

## IDamageableInterface

**Interface**: `IDamageableInterface`
**Header**: `KatanaCombat/Public/Interfaces/DamageableInterface.h`

Interface for actors that can receive damage and combat effects. Provides contract for damage application, finishers, parry reactions, etc.

### Methods

#### ApplyDamage
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
float ApplyDamage(const FHitReactionInfo& HitInfo);
```
**Parameters**:
- `HitInfo` - Complete information about the hit

**Returns**: Actual damage dealt (after resistances, etc.)

Apply damage to this actor.

#### ApplyPostureDamage
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
bool ApplyPostureDamage(float PostureDamage, AActor* Attacker);
```
**Parameters**:
- `PostureDamage` - Amount of posture to remove
- `Attacker` - Who is attacking

**Returns**: True if guard was broken (posture reached 0)

Apply posture damage when this actor blocks an attack.

#### CanBeDamaged
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
bool CanBeDamaged() const;
```
**Returns**: True if vulnerable to damage

Check if this actor can be damaged right now.

#### IsBlocking
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
bool IsBlocking() const;
```
**Returns**: True if actively blocking

Check if this actor is currently blocking.

#### IsGuardBroken
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
bool IsGuardBroken() const;
```
**Returns**: True if posture depleted

Check if this actor is in a guard broken state.

#### ExecuteFinisher
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
bool ExecuteFinisher(AActor* Attacker, UAttackData* FinisherData);
```
**Parameters**:
- `Attacker` - Who is performing the finisher
- `FinisherData` - Attack data for the finisher

**Returns**: True if finisher was successfully started

Execute a finisher on this actor (must be guard broken or stunned).

#### OnAttackParried
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
void OnAttackParried(AActor* Parrier);
```
**Parameters**:
- `Parrier` - Who parried the attack

React to a successful parry (this actor's attack was parried).

#### OpenCounterWindow
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
void OpenCounterWindow(float Duration);
```
**Parameters**:
- `Duration` - How long the counter window stays open

Open counter window (after being parried or perfect evaded).

#### GetCurrentPosture
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
float GetCurrentPosture() const;
```
**Returns**: Current posture (0-100, 0 = guard broken)

Get current posture value.

#### GetMaxPosture
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
float GetMaxPosture() const;
```
**Returns**: Max posture

Get maximum posture value.

#### IsInCounterWindow
```cpp
UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
bool IsInCounterWindow() const;
```
**Returns**: True if in counter window

Check if currently in a counter window (vulnerable to counter attacks).

---

## AnimNotifies

### AnimNotifyState_AttackPhase

**Class**: `UAnimNotifyState_AttackPhase : UAnimNotifyState`
**Header**: `KatanaCombat/Public/Animation/AnimNotifyState_AttackPhase.h`

AnimNotifyState that marks attack phases (Windup, Active, Recovery, Hold). Used by AttackData to read timing, and triggers callbacks in AnimInstance.

**Configuration**:
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Phase")
EAttackPhase Phase = EAttackPhase::Windup;
```
Which phase this notify represents.

**Usage**:
1. Add to attack montage in Animation Editor
2. Set Phase property (Windup/Active/Recovery/Hold)
3. Position at appropriate times in animation
4. NotifyBegin/End automatically route to CombatComponent

**Timeline**:
```
[──Windup──][──Active──][──Recovery──]
    30%         20%          50%
```

- **Windup**: Telegraphing, can be parried, motion warping active
- **Active**: Hit detection enabled, damage dealt
- **Recovery**: Vulnerable, combo input window opens
- **Hold**: Optional pause for directional input (light attacks)

### AnimNotifyState_ComboWindow

**Class**: `UAnimNotifyState_ComboWindow : UAnimNotifyState`
**Header**: `KatanaCombat/Public/Animation/AnimNotifyState_ComboWindow.h`

AnimNotifyState that opens the combo input window. Allows player to input next combo attack during this time.

**Usage**:
1. Add to attack montage during Recovery phase
2. Typically placed at start of recovery (first 60%)
3. Duration determines how long player has to input combo
4. Default window: 0.6s (configurable in CombatSettings)

**Timeline**:
```
[──Windup──][──Active──][──Recovery──]
                             ▲▲▲▲▲▲
                         Combo Window
```

During this window:
- Light attack input → Execute NextComboAttack
- Heavy attack input → Execute HeavyComboAttack
- No input → Chain breaks, return to idle

### AnimNotify_ToggleHitDetection

**Class**: `UAnimNotify_ToggleHitDetection : UAnimNotify`
**Header**: `KatanaCombat/Public/Animation/AnimNotify_ToggleHitDetection.h`

AnimNotify that toggles weapon hit detection on/off. Typically used in pairs: Enable at start of active phase, Disable at end.

**Configuration**:
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Detection")
bool bEnable = true;
```
Enable or disable hit detection.

**Usage**:
1. Place two instances in attack montage:
   - First with bEnable = true at start of Active phase
   - Second with bEnable = false at end of Active phase
2. Routes to WeaponComponent via ICombatInterface
3. WeaponComponent performs swept sphere traces while enabled

**Timeline**:
```
[──Windup──][──Active──][──Recovery──]
              ▼      ▲
           Enable  Disable
              ████████  <- Hit Detection Active
```

**Note**: Using AnimNotifyState_AttackPhase (Active) is preferred as it automatically handles enable/disable. Use this only for fine-tuned control.

---

## Delegates

All delegates are declared in `CombatTypes.h`.

### FOnCombatStateChanged
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
```
**Parameters**:
- `NewState` - New combat state

Broadcast when combat state changes.

### FOnAttackHit
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttackHit, AActor*, HitActor, float, Damage);
```
**Parameters**:
- `HitActor` - Actor that was hit
- `Damage` - Damage dealt

Broadcast when attack hits an actor.

### FOnPostureChanged
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPostureChanged, float, NewPosture);
```
**Parameters**:
- `NewPosture` - New posture value (0-100)

Broadcast when posture changes.

### FOnGuardBroken
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGuardBroken);
```
Broadcast when guard is broken.

### FOnPerfectParry
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectParry, AActor*, ParriedActor);
```
**Parameters**:
- `ParriedActor` - Actor that was parried

Broadcast on successful perfect parry.

### FOnPerfectEvade
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerfectEvade, AActor*, EvadedActor);
```
**Parameters**:
- `EvadedActor` - Actor that was evaded

Broadcast on successful perfect evade.

### FOnFinisherAvailable
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFinisherAvailable, AActor*, Target);
```
**Parameters**:
- `Target` - Target available for finisher

Broadcast when a finisher opportunity is available.

---

## Enums

### ECombatState
```cpp
enum class ECombatState : uint8
{
    Idle,
    Attacking,
    HoldingLightAttack,
    ChargingHeavyAttack,
    Blocking,
    Parrying,
    GuardBroken,
    Finishing,
    HitStunned,
    Evading,
    Dead
};
```

### EAttackType
```cpp
enum class EAttackType : uint8
{
    None,
    Light,
    Heavy,
    Special
};
```

### EAttackPhase
```cpp
enum class EAttackPhase : uint8
{
    None,
    Windup,
    Active,
    Hold,
    Recovery,
    HoldWindow,
    CancelWindow
};
```

### EAttackDirection
```cpp
enum class EAttackDirection : uint8
{
    None,
    Forward,
    Backward,
    Left,
    Right
};
```

### EInputType
```cpp
enum class EInputType : uint8
{
    None,
    LightAttack,
    HeavyAttack,
    Block,
    Evade,
    Special
};
```

---

## Structs

### FHitReactionInfo
```cpp
USTRUCT(BlueprintType)
struct FHitReactionInfo
{
    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<AActor> Attacker;

    UPROPERTY(BlueprintReadWrite)
    FVector HitDirection;

    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<UAttackData> AttackData;

    UPROPERTY(BlueprintReadWrite)
    float Damage;

    UPROPERTY(BlueprintReadWrite)
    float StunDuration;

    UPROPERTY(BlueprintReadWrite)
    bool bWasCounter;

    UPROPERTY(BlueprintReadWrite)
    FVector ImpactPoint;
};
```
Complete information about a hit, passed when applying damage.

### FHitReactionAnimSet
```cpp
USTRUCT(BlueprintType)
struct FHitReactionAnimSet
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UAnimMontage> FrontHit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UAnimMontage> BackHit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UAnimMontage> LeftHit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UAnimMontage> RightHit;
};
```
Set of directional hit reaction animations.

### FMotionWarpingConfig
```cpp
USTRUCT(BlueprintType)
struct FMotionWarpingConfig
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseMotionWarping = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName MotionWarpingTargetName = "AttackTarget";

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinWarpDistance = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxWarpDistance = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WarpRotationSpeed = 720.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bWarpTranslation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRequireLineOfSight = true;
};
```
Motion warping configuration for an attack.

---

## Usage Examples

### Creating a Character with Combat

```cpp
// In AMyCharacter.h
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
TObjectPtr<UCombatComponent> CombatComponent;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
TObjectPtr<UTargetingComponent> TargetingComponent;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
TObjectPtr<UWeaponComponent> WeaponComponent;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
TObjectPtr<UHitReactionComponent> HitReactionComponent;

// In AMyCharacter.cpp constructor
AMyCharacter::AMyCharacter()
{
    CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
    TargetingComponent = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComponent"));
    WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
    HitReactionComponent = CreateDefaultSubobject<UHitReactionComponent>(TEXT("HitReactionComponent"));
}

// Bind input in SetupPlayerInputComponent
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAction("LightAttack", IE_Pressed, this, &AMyCharacter::OnLightAttackPressed);
    PlayerInputComponent->BindAction("LightAttack", IE_Released, this, &AMyCharacter::OnLightAttackReleased);
}

void AMyCharacter::OnLightAttackPressed()
{
    if (CombatComponent)
    {
        CombatComponent->OnLightAttackPressed();
    }
}
```

### Handling Weapon Hits

```cpp
// Bind to weapon hit event in BeginPlay
void AMyCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (WeaponComponent)
    {
        WeaponComponent->OnWeaponHit.AddDynamic(this, &AMyCharacter::HandleWeaponHit);
    }
}

void AMyCharacter::HandleWeaponHit(AActor* HitActor, const FHitResult& HitResult, UAttackData* AttackData)
{
    // Apply damage through interface
    if (IDamageableInterface* Damageable = Cast<IDamageableInterface>(HitActor))
    {
        FHitReactionInfo HitInfo;
        HitInfo.Attacker = this;
        HitInfo.AttackData = AttackData;
        HitInfo.Damage = AttackData->BaseDamage;
        HitInfo.HitDirection = GetActorForwardVector();
        HitInfo.ImpactPoint = HitResult.ImpactPoint;

        Damageable->Execute_ApplyDamage(HitActor, HitInfo);
    }

    // Spawn VFX
    if (HitVFX)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitVFX, HitResult.ImpactPoint);
    }
}
```

---

## V2 Combat System
