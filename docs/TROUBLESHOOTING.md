# KatanaCombat Troubleshooting Guide

Complete troubleshooting reference for the KatanaCombat system, covering common compilation errors, runtime issues, animation problems, and debugging strategies.

---

## Table of Contents

- [Compilation Errors](#compilation-errors)
- [Runtime Issues](#runtime-issues)
  - [Attacks Not Executing](#attacks-not-executing)
  - [Combos Not Chaining](#combos-not-chaining)
  - [Hits Not Detecting](#hits-not-detecting)
  - [Motion Warping Issues](#motion-warping-issues)
  - [Parry System Issues](#parry-system-issues)
  - [Hold Mechanics Issues](#hold-mechanics-issues)
- [Animation Problems](#animation-problems)
- [Performance Issues](#performance-issues)
- [Debug Visualization](#debug-visualization)
- [Common Mistakes](#common-mistakes)
- [Console Commands](#console-commands)
- [Log Categories](#log-categories)

---

## Compilation Errors

### Error: Delegate Redefinition

**Symptom**:
```
error C2371: 'FOnCombatStateChanged': redefinition; different basic types
```

**Cause**: Delegate declared both in CombatTypes.h and CombatComponent.h.

**Fix**:
1. Keep delegate declarations ONLY in CombatTypes.h:
```cpp
// CombatTypes.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, ECombatState, NewState);
```

2. Remove duplicate declarations from component headers
3. Clean and rebuild project

### Error: Missing Module Dependencies

**Symptom**:
```
error: 'UMotionWarpingComponent' undefined
error: cannot open include file 'MotionWarpingComponent.h'
```

**Cause**: Missing module dependency in Build.cs file.

**Fix**:
Add required modules to `KatanaCombat.Build.cs`:
```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "MotionWarping",  // Add this
    "EnhancedInput"   // If using Enhanced Input
});
```

### Error: Forward Declaration Not Found

**Symptom**:
```
error: 'UAttackData' was not declared in this scope
```

**Cause**: Missing forward declaration or include.

**Fix**:
Add forward declaration at top of header:
```cpp
// Forward declarations
class UAttackData;
class UCombatSettings;
class ACharacter;
```

Or include the header in .cpp file:
```cpp
#include "Data/AttackData.h"
```

### Error: BlueprintCallable on Private Function

**Symptom**:
```
error: BlueprintCallable functions must be public or protected
```

**Cause**: UFUNCTION marked BlueprintCallable in private section.

**Fix**:
Move function to public or protected section, or remove BlueprintCallable:
```cpp
public:
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ExecuteAttack(UAttackData* AttackData);

private:
    // Non-blueprint functions
    void InternalHelper();
```

---

## Runtime Issues

### Attacks Not Executing

#### Symptom: ExecuteAttack() Returns False

**Check 1: CanAttack() State**
```cpp
// In code
if (!CombatComponent->CanAttack())
{
    UE_LOG(LogCombat, Warning, TEXT("Cannot attack - Current state: %d"),
        static_cast<int32>(CombatComponent->GetCombatState()));
}
```

**Common Causes**:
- Still in previous attack state (Attacking, Recovery)
- Character is blocking or stunned
- Guard broken state not recovered

**Fix**:
- Wait for current attack to complete
- Ensure state transitions properly to Idle
- Check OnAttackPhaseEnd callbacks are firing

**Check 2: Missing AttackData**
```cpp
if (!DefaultLightAttack)
{
    UE_LOG(LogCombat, Error, TEXT("DefaultLightAttack is NULL!"));
}
```

**Fix**: Assign AttackData asset in Blueprint or C++:
```cpp
// In Blueprint: Select CombatComponent → Details → Default Light Attack
// In C++:
CombatComponent->DefaultLightAttack = LoadObject<UAttackData>(nullptr,
    TEXT("/Game/Data/Attacks/DA_LightAttack1.DA_LightAttack1"));
```

**Check 3: Missing CombatSettings**
```cpp
if (!CombatComponent->CombatSettings)
{
    UE_LOG(LogCombat, Error, TEXT("CombatSettings is NULL! Timing windows will not work."));
}
```

**Fix**: Assign CombatSettings asset:
```cpp
// In Blueprint: CombatComponent → Details → Combat Settings
// Create asset: Content Browser → Right-click → Miscellaneous → Data Asset → CombatSettings
```

**Check 4: AnimInstance Missing**

**Symptom**: ExecuteAttack returns true but no animation plays.

**Fix**:
- Ensure character has SkeletalMeshComponent
- Mesh has valid AnimInstance assigned
- AnimInstance is of correct class (inherits from base)

#### Symptom: Attack Starts But State Stuck

**Cause**: AnimNotifyStates not firing correctly.

**Debug**:
```cpp
// Add to CombatComponent.cpp
void UCombatComponent::OnAttackPhaseBegin(EAttackPhase Phase)
{
    UE_LOG(LogCombat, Warning, TEXT("Phase BEGIN: %d"), static_cast<int32>(Phase));
    CurrentPhase = Phase;
    // ... rest of code
}

void UCombatComponent::OnAttackPhaseEnd(EAttackPhase Phase)
{
    UE_LOG(LogCombat, Warning, TEXT("Phase END: %d"), static_cast<int32>(Phase));
    // ... rest of code
}
```

**Check in logs**: Do you see "Phase BEGIN" and "Phase END" messages?

**If not**:
1. Open attack montage in Animation Editor
2. Check AnimNotifyState_AttackPhase notifies exist
3. Verify Phase property is set correctly on each notify
4. Ensure notifies don't overlap
5. Check AnimInstance routes callbacks to CombatComponent

### Combos Not Chaining

#### Symptom: Pressing Light During Recovery Does Nothing

**Check 1: Combo Window Not Opening**
```cpp
// Add to OpenComboWindow
void UCombatComponent::OpenComboWindow(float Duration)
{
    bCanCombo = true;
    UE_LOG(LogCombat, Warning, TEXT("Combo window OPENED for %.2f seconds"), Duration);
    // ... rest of code
}
```

**Cause**: AnimNotifyState_ComboWindow missing or not placed correctly.

**Fix**:
1. Open attack montage
2. Add AnimNotifyState_ComboWindow during Recovery phase
3. Position at start of recovery (typically first 60% of recovery)
4. Set duration to 0.6s (or CombatSettings->ComboInputWindow)

**Check 2: NextComboAttack Not Set**
```cpp
// In OnLightAttackPressed during combo
if (bCanCombo && CurrentAttackData)
{
    if (!CurrentAttackData->NextComboAttack)
    {
        UE_LOG(LogCombat, Error, TEXT("NextComboAttack is NULL on %s!"),
            *CurrentAttackData->GetName());
    }
}
```

**Fix**: Open AttackData asset and set NextComboAttack reference:
1. Content Browser → Find AttackData asset
2. Double-click to open
3. Combos section → Next Combo Attack → Select next attack in chain
4. Save

**Check 3: Input Buffered But Not Processed**

**Debug**: Add logging to ProcessQueuedCombo:
```cpp
void UCombatComponent::ProcessQueuedCombo()
{
    UE_LOG(LogCombat, Warning, TEXT("Processing combo queue, buffer size: %d"),
        ComboInputBuffer.Num());

    if (ComboInputBuffer.Num() > 0)
    {
        // ... existing code
    }
    else
    {
        UE_LOG(LogCombat, Warning, TEXT("No combo queued, returning to idle"));
    }
}
```

**Common Issues**:
- OnAttackPhaseEnd(Recovery) not being called
- State not transitioning from Attacking to Recovery
- Timer issue with combo window closure

**Fix**: Ensure Recovery phase AnimNotifyState exists and ends correctly.

#### Symptom: Combo Resets Too Quickly

**Cause**: ComboResetTimer firing too fast.

**Check**:
```cpp
// In CombatSettings
float ComboResetDelay = 2.0f;  // Should be ~2 seconds
```

**Fix**: Increase ComboResetDelay in CombatSettings asset.

#### Symptom: Directional Follow-Up Infinite Loop (V2 SYSTEM) ⚠️

**Status**: ❌ **KNOWN BUG** - Attempted fix in Phase 1 did NOT resolve issue (2025-11-12)

**Symptom**:
- User holds directional input (Forward) and presses Light/Heavy repeatedly
- Same directional follow-up attack plays infinitely
- Different directional attacks loop if direction is changed while spamming
- Only affects V2 combat system (`CombatSettings->bUseV2System = true`)

**Root Cause**:
Action queue captures `LastDirectionalInput` at **queue time**, not execution time. When multiple inputs are queued before first directional attack executes, they ALL capture the same stale direction value:

```
Timeline:
T=0.0s: Hold Forward + Press Light → Queued with Direction=Forward
T=0.1s: Press Light again → Queued with Direction=Forward (STALE)
T=0.2s: Press Light again → Queued with Direction=Forward (STALE)
T=0.5s: First action executes → Directional follow-up plays
T=1.0s: Second action executes → STILL has Forward in queue entry
T=1.5s: Third action executes → STILL has Forward in queue entry
Result: Infinite directional loop
```

**Attempted Fix (Phase 1 - Failed)**:
- Added `bShouldClearDirectionalInput` flag to `FAttackResolutionResult`
- Cleared `LastDirectionalInput` in `GetAttackForInput()` when directional resolved
- **Why it failed**: Clearing happens during resolution, but already-queued actions retain stale direction data

**Recommended Fixes** (see CLAUDE.md "Attempted Fix Analysis & Next Steps" section for full details):

1. **Option 1: Per-Action Direction Storage (RECOMMENDED)**
   - Store `CapturedDirection` in each `FActionQueueEntry`
   - Clear ALL pending actions' directions when directional follow-up executes
   - Effort: ~2 hours
   - Files: `ActionQueueTypes.h`, `CombatComponentV2.cpp` (QueueAction, ExecuteAction)

2. **Option 2: Single-Use Directional Flag**
   - Add `bDirectionalInputConsumed` boolean to component
   - Mark as consumed when directional follow-up resolves
   - Check flag in `GetAttackForInput()` before using direction
   - Effort: ~1 hour
   - Files: `CombatComponentV2.h`, `CombatComponentV2.cpp` (OnInputEvent, GetAttackForInput)

3. **Option 3: Immediate Queue Filtering**
   - Remove/invalidate directional actions from queue after execution
   - More complex, requires determining which actions are "directional"
   - Effort: ~3 hours

**Temporary Workaround**:
Disable V2 system and use V1 (does not have this bug):
```cpp
// In CombatSettings asset or Blueprint
CombatSettings->bUseV2System = false;
```

**Debug Logging** (add to track issue):
```cpp
// CombatComponentV2.cpp - QueueAction()
UE_LOG(LogCombat, Warning, TEXT("[QUEUE] Captured Direction=%s at queue time"),
    *UEnum::GetValueAsString(LastDirectionalInput));

// CombatComponentV2.cpp - ExecuteAction()
UE_LOG(LogCombat, Warning, TEXT("[EXECUTE] Using Direction=%s (queued earlier)"),
    *UEnum::GetValueAsString(Action.CapturedDirection));
```

**See Also**:
- CLAUDE.md → "Attempted Fix Analysis & Next Steps" (lines 100-348)
- Source files: `CombatComponentV2.cpp:1958-2040` (resolution logic)
- Related: `ActionQueueTypes.h` (FActionQueueEntry struct)

---

### Hits Not Detecting

#### Symptom: Weapon Passes Through Enemies

**Check 1: Hit Detection Not Enabled**
```cpp
// Add to WeaponComponent::EnableHitDetection
void UWeaponComponent::EnableHitDetection()
{
    bHitDetectionEnabled = true;
    bFirstTrace = true;
    UE_LOG(LogWeapon, Warning, TEXT("Hit detection ENABLED"));
}
```

**Cause**: AnimNotify_ToggleHitDetection missing or not calling WeaponComponent.

**Fix**:
1. Open attack montage
2. Add AnimNotify_ToggleHitDetection at start of Active phase (bEnable = true)
3. Add AnimNotify_ToggleHitDetection at end of Active phase (bEnable = false)

**Alternative**: Use AnimNotifyState_AttackPhase with Phase = Active (automatically enables/disables).

**Check 2: Weapon Sockets Missing**
```cpp
// In WeaponComponent::GetSocketLocation
FVector UWeaponComponent::GetSocketLocation(FName SocketName) const
{
    if (OwnerMesh && OwnerMesh->DoesSocketExist(SocketName))
    {
        return OwnerMesh->GetSocketLocation(SocketName);
    }

    UE_LOG(LogWeapon, Error, TEXT("Socket '%s' does not exist!"), *SocketName.ToString());
    return OwnerCharacter ? OwnerCharacter->GetActorLocation() : FVector::ZeroVector;
}
```

**Fix**: Create weapon sockets on skeletal mesh:
1. Open character skeletal mesh
2. Skeleton Tree panel → Right-click on weapon bone
3. Add Socket → Name it "weapon_start" (at base/handle)
4. Add Socket → Name it "weapon_end" (at tip/blade end)
5. Save

**Check 3: Wrong Trace Channel**
```cpp
// WeaponComponent settings
TraceChannel = ECC_Pawn;  // Must hit pawns
```

**Debug**: Enable debug draw:
```cpp
// In WeaponComponent
bDebugDraw = true;
```

You should see:
- Green line = trace path
- Red sphere = hit detected
- Blue line = no hit

**Fix**:
- Set TraceChannel to channel that hits enemies (usually ECC_Pawn)
- Ensure enemy collision is enabled for that channel
- Check enemy has collision mesh

**Check 4: Trace Radius Too Small**
```cpp
// WeaponComponent settings
TraceRadius = 5.0f;  // Try increasing to 15.0f
```

Small weapons need smaller radius, large weapons need larger.

**Check 5: Hit Actor Already Hit**
```cpp
// WeaponComponent tracks hits to prevent double-hitting
if (WasActorAlreadyHit(HitActor))
{
    UE_LOG(LogWeapon, Warning, TEXT("Actor %s already hit this attack"),
        *HitActor->GetName());
    return;
}
```

**This is expected behavior**. ResetHitActors() is called at start of new attack.

**If hits still not registering**:
- Check ResetHitActors() is being called
- Verify it's a different attack (not same continuous attack)

### Motion Warping Issues

#### Symptom: Character Doesn't Chase Target

**Check 1: Motion Warping Component Missing**
```cpp
// In CombatComponent::BeginPlay
if (!MotionWarpingComponent)
{
    UE_LOG(LogCombat, Error, TEXT("MotionWarpingComponent is NULL! Motion warping will not work."));
}
```

**Fix**: Add UMotionWarpingComponent to character:
```cpp
// In character constructor
MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));
```

Or in Blueprint: Add Component → Motion Warping

**Check 2: No Target Found**
```cpp
// In SetupMotionWarping
AActor* Target = TargetingComponent->FindTarget(WorldInput);
if (!Target)
{
    UE_LOG(LogCombat, Warning, TEXT("No target found for motion warping"));
    return;
}
```

**Debug**: Enable TargetingComponent debug draw:
```cpp
TargetingComponent->bDebugDraw = true;
```

You should see:
- Yellow cone = search direction
- Green sphere = valid targets
- Red sphere = selected target

**Causes**:
- No enemies in range (MaxTargetDistance = 1000.0f)
- Enemy outside directional cone (DirectionalConeAngle = 60.0f)
- Line of sight blocked (bRequireLineOfSight = true)

**Fix**:
- Increase MaxTargetDistance
- Increase DirectionalConeAngle
- Set bRequireLineOfSight = false for testing

**Check 3: Warp Distance Constraints**
```cpp
// In AttackData → Motion Warping Config
MinWarpDistance = 50.0f;   // Too close, don't warp
MaxWarpDistance = 400.0f;  // Too far, don't warp
```

**Fix**: Adjust distances in AttackData asset.

**Check 4: Wrong Warp Target Name**

**Symptom**: Warp target set but animation doesn't use it.

**Cause**: Name mismatch between AttackData and animation.

**Fix**:
1. Open AttackData → Motion Warping Config → Motion Warping Target Name (e.g., "AttackTarget")
2. Open attack animation
3. Add Motion Warping notify if missing
4. Set Warp Target Name to match exactly (case-sensitive!)

**Check 5: bUseMotionWarping = False**

**Fix**: In AttackData → Motion Warping Config → Use Motion Warping = true

### Parry System Issues

#### Symptom: Perfect Parry Never Works

**Check 1: Parry Window Placement**

**Correct setup** (on enemy attack):
```
[──Windup──][──Active──][──Recovery──]
    ▲▲▲▲
Parry Window (AnimNotifyState_ParryWindow on WINDUP phase)
```

**Common mistake**: Placing parry window during Active or Recovery phase.

**Fix**:
1. Open enemy attack montage
2. Ensure parry window notify is during Windup phase ONLY
3. Duration should match CombatSettings->ParryWindow (default 0.3s)

**Check 2: IsInParryWindow Not Set**

**Debug**:
```cpp
// When player presses block during enemy windup
if (Enemy->IsInParryWindow())
{
    UE_LOG(LogCombat, Log, TEXT("PARRY SUCCESS!"));
}
else
{
    UE_LOG(LogCombat, Warning, TEXT("Not in parry window"));
}
```

**Cause**: Enemy AnimNotifyState_ParryWindow not calling OpenParryWindow.

**Fix**: Ensure AnimNotifyState routes to enemy CombatComponent.

**Check 3: Wrong Parry Timing**

**Players often press block too late** (during Active phase).

**Solution**:
- Increase parry window duration in CombatSettings (0.3s → 0.5s)
- Add visual telegraph to enemy windup (VFX, audio cue)
- Tune enemy attack windup timing

### Hold Mechanics Issues

#### Symptom: Can't Hold Light Attack

**Check 1: Button State Not Tracked**
```cpp
// OnLightAttackPressed should set:
bLightAttackHeld = true;

// OnLightAttackReleased should set:
bLightAttackHeld = false;
```

**Cause**: Input not calling both pressed AND released functions.

**Fix**: Bind both in SetupPlayerInputComponent:
```cpp
InputComponent->BindAction("LightAttack", IE_Pressed, this, &AMyCharacter::OnLightAttackPressed);
InputComponent->BindAction("LightAttack", IE_Released, this, &AMyCharacter::OnLightAttackReleased);
```

**Check 2: bCanHoldAtEnd = False**
```cpp
// In AttackData
bCanHoldAtEnd = true;  // Must be true for holds
```

**Fix**: Set to true in AttackData asset (Light Attack section).

**Check 3: HoldWindow Phase Missing**
```cpp
// Attack must have HoldWindow phase AFTER Active phase
[──Windup──][──Active──][──HoldWindow──][──Recovery──]
                             ▲▲▲▲▲▲▲
                         Check if still holding
```

**Fix**:
1. Open attack montage
2. Add AnimNotifyState_AttackPhase with Phase = HoldWindow
3. Place after Active phase ends
4. Duration ~0.2-0.5s (time to check if button still held)

**Check 4: Button Released Too Early**

**Behavior**: If button released before HoldWindow phase begins, hold doesn't trigger.

**This is by design**. Player must CONTINUE holding from attack start through to HoldWindow phase.

**Solution**: Communicate to players they must hold button throughout animation.

#### Symptom: Directional Follow-Up Not Executing

**Check 1: DirectionalFollowUps Not Set**
```cpp
// In AttackData
DirectionalFollowUps.Add(EAttackDirection::Forward, ForwardFollowUpAttack);
DirectionalFollowUps.Add(EAttackDirection::Left, LeftFollowUpAttack);
// etc.
```

**Fix**: Set in AttackData asset:
1. Combos → Directional Follow Ups
2. Add entries for each direction
3. Assign AttackData for each direction

**Check 2: Movement Input Not Updated**
```cpp
// In Tick or input handling
CombatComponent->SetMovementInput(FVector2D(RightAxis, ForwardAxis));
```

**Cause**: Movement input not being passed to CombatComponent.

**Fix**: Call SetMovementInput every frame with current input values.

**Check 3: Input Neutralized at Release**

**Common mistake**: Player releases stick before releasing attack button.

**Debug**:
```cpp
// In ReleaseHeldLight
UE_LOG(LogCombat, Warning, TEXT("Stored movement input: X=%.2f Y=%.2f"),
    StoredMovementInput.X, StoredMovementInput.Y);
```

**If (0, 0)**: Player released stick too early.

**Solution**: Read movement input AT MOMENT OF BUTTON RELEASE, not during hold.

---

## Animation Problems

### Montage Not Playing

**Check 1: AnimInstance NULL**
```cpp
if (!AnimInstance)
{
    UE_LOG(LogCombat, Error, TEXT("AnimInstance is NULL!"));
}
```

**Fix**: Ensure character has SkeletalMeshComponent with AnimInstance assigned.

**Check 2: Montage Asset NULL**
```cpp
if (!AttackData->AttackMontage)
{
    UE_LOG(LogCombat, Error, TEXT("AttackMontage is NULL in %s"), *AttackData->GetName());
}
```

**Fix**: Assign montage in AttackData asset.

**Check 3: Montage Playing But Not Visible**

**Cause**: Animation Blueprint overriding montage slot.

**Fix**:
1. Open Animation Blueprint
2. AnimGraph → Ensure "Slot 'DefaultSlot'" node exists
3. Connect to Final Animation Pose
4. Ensure montage uses same slot name

**Check 4: Montage Interrupted Immediately**

**Cause**: Another animation system taking priority.

**Debug**: Check montage is actually playing:
```cpp
if (AnimInstance->Montage_IsPlaying(AttackData->AttackMontage))
{
    UE_LOG(LogCombat, Log, TEXT("Montage is playing"));
}
else
{
    UE_LOG(LogCombat, Warning, TEXT("Montage stopped unexpectedly"));
}
```

**Fix**:
- Disable other animation systems during attacks
- Increase montage blend-in weight
- Check for conflicting state machine transitions

### AnimNotifies Not Firing

**Check 1: Notify Placement**

Notifies must be WITHIN montage timeline, not outside.

**Check 2: Notify Not Implemented**

For custom notifies, ensure Notify() or NotifyBegin()/NotifyEnd() are implemented and route to correct component.

**Example**:
```cpp
// In AnimNotify_ToggleHitDetection::Notify
void UAnimNotify_ToggleHitDetection::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp || !MeshComp->GetOwner())
    {
        UE_LOG(LogAnimation, Error, TEXT("MeshComp or Owner is NULL"));
        return;
    }

    ICombatInterface* CombatInterface = Cast<ICombatInterface>(MeshComp->GetOwner());
    if (CombatInterface)
    {
        if (bEnable)
        {
            CombatInterface->Execute_OnEnableHitDetection(MeshComp->GetOwner());
        }
        else
        {
            CombatInterface->Execute_OnDisableHitDetection(MeshComp->GetOwner());
        }
    }
    else
    {
        UE_LOG(LogAnimation, Error, TEXT("Owner does not implement ICombatInterface"));
    }
}
```

**Check 3: AnimNotifyState Overlap**

**Problem**: Multiple AnimNotifyState_AttackPhase overlapping.

**Symptom**: Phases begin/end in wrong order, state machine confusion.

**Fix**: Ensure phases don't overlap:
```
[──Windup──][──Active──][──Recovery──]
     ████       ████        ████████
    (no gaps or overlaps)
```

### Root Motion Not Working

**Check 1: Root Motion Enabled**

In Animation Montage:
- Asset Details → Root Motion → Root Motion Root Lock = Animation First Frame
- Enable Root Motion = true

**Check 2: Character Movement Component**

In Character Blueprint:
- Character Movement → Movement Mode = Walking
- Character Movement → Root Motion Mode = Root Motion From Montages Only

**Check 3: Animation Root Bone**

Animation must have root bone with motion data.

**Test**: Play animation in viewport - does character move?
- Yes: Root motion exists, issue is in game setup
- No: Animation doesn't have root motion data

---

## Performance Issues

### Frame Drops During Combat

**Common Causes**:

**1. Debug Drawing Enabled**
```cpp
// Disable in production
CombatComponent->bDebugDraw = false;
WeaponComponent->bDebugDraw = false;
TargetingComponent->bDebugDraw = false;
```

**2. Too Many Swept Traces**
```cpp
// In WeaponComponent
TraceRadius = 5.0f;  // Larger radius = more expensive

// Solution: Use smallest effective radius
```

**3. Redundant Component Searches**

Bad (every frame):
```cpp
void Tick(float DeltaTime)
{
    UCombatComponent* Combat = GetOwner()->FindComponentByClass<UCombatComponent>();
}
```

Good (cache in BeginPlay):
```cpp
void BeginPlay()
{
    CombatComponent = GetOwner()->FindComponentByClass<UCombatComponent>();
}

void Tick(float DeltaTime)
{
    if (CombatComponent) { /* use cached */ }
}
```

**4. Excessive Logging**

Remove verbose logging in shipping builds:
```cpp
#if !UE_BUILD_SHIPPING
    UE_LOG(LogCombat, Verbose, TEXT("Debug info"));
#endif
```

### Memory Leaks

**Timers Not Cleared**

Always clear timers when resetting:
```cpp
void UCombatComponent::ResetCombo()
{
    // Clear timers
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboResetTimer);
        GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimer);
    }

    // Clear data
    ComboCount = 0;
    CurrentAttackData = nullptr;
}
```

---

## Debug Visualization

### Enabling Debug Draw

```cpp
// In Blueprint or C++
CombatComponent->bDebugDraw = true;
TargetingComponent->bDebugDraw = true;
WeaponComponent->bDebugDraw = true;
```

### CombatComponent Debug Visualization

**What it shows**:
- State changes (printed to screen)
- Timing windows (combo, parry, counter)
- Input buffering status
- Combo count

**Console**: `showdebug COMBAT` (if implemented)

### TargetingComponent Debug Visualization

**What it shows**:
- **Yellow cone**: Search direction and cone angle
- **Green spheres**: Valid targets in range and cone
- **Red sphere**: Selected target (nearest)
- **Blue lines**: Line of sight checks
- **Orange text**: Distance and angle to targets

**How to read**:
- If cone not pointing right direction → check GetDirectionVector logic
- If no green spheres → no targets in range/cone
- If green spheres but no red → check targeting filters

### WeaponComponent Debug Visualization

**What it shows**:
- **Green line**: Sweep trace path (weapon_start to weapon_end)
- **Red sphere**: Hit point
- **Blue line**: Trace with no hit
- **Duration**: Controlled by DebugDrawDuration property

**How to read**:
- If line very short → sockets too close together
- If line in wrong place → sockets on wrong bones
- If blue lines through enemies → check trace channel

### Animation Debugging

**Montage Debug**:
```cpp
// In console
showdebug ANIMATION

// Shows:
// - Currently playing montages
// - Active anim notifies
// - Root motion
```

**Notify Timing Debug**:

Add print statements in AnimNotify callbacks:
```cpp
void UCombatComponent::OnAttackPhaseBegin(EAttackPhase Phase)
{
    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
        FString::Printf(TEXT("Phase BEGIN: %s"), *UEnum::GetValueAsString(Phase)));
}
```

---

## Common Mistakes

### 1. Not Checking for NULL

**Bad**:
```cpp
CombatComponent->ExecuteAttack(AttackData);  // Crash if CombatComponent is NULL
```

**Good**:
```cpp
if (CombatComponent && AttackData)
{
    CombatComponent->ExecuteAttack(AttackData);
}
```

### 2. Forgetting to Assign Data Assets

**Symptom**: Everything compiles but nothing works at runtime.

**Cause**: AttackData, CombatSettings not assigned in Blueprint.

**Fix**: Always assign data assets in Character Blueprint:
- CombatComponent → Combat Settings
- CombatComponent → Default Light Attack
- CombatComponent → Default Heavy Attack

### 3. AnimNotify Routing Issues

**Mistake**: Creating custom AnimNotify but not routing to component.

**Fix**: Always implement routing in AnimInstance or use interface:
```cpp
// Option 1: In AnimInstance
void UMyAnimInstance::AnimNotify_EnableHitDetection(const UAnimNotify* Notify)
{
    if (ACharacter* Character = Cast<ACharacter>(GetOwningActor()))
    {
        if (UWeaponComponent* Weapon = Character->FindComponentByClass<UWeaponComponent>())
        {
            Weapon->EnableHitDetection();
        }
    }
}

// Option 2: Use ICombatInterface (recommended)
void UAnimNotify_ToggleHitDetection::Notify(...)
{
    ICombatInterface* CombatInterface = Cast<ICombatInterface>(MeshComp->GetOwner());
    if (CombatInterface)
    {
        CombatInterface->Execute_OnEnableHitDetection(MeshComp->GetOwner());
    }
}
```

### 4. State Machine Logic Errors

**Mistake**: Allowing invalid state transitions.

**Example**: Attacking while already attacking → animation bugs.

**Fix**: Always check CanAttack() before executing:
```cpp
if (CombatComponent->CanAttack())
{
    CombatComponent->ExecuteAttack(AttackData);
}
```

### 5. Forgetting Input Release Handling

**Mistake**: Only binding IE_Pressed, not IE_Released.

**Result**: Hold mechanics don't work, charge attacks stuck.

**Fix**: Bind both:
```cpp
InputComponent->BindAction("LightAttack", IE_Pressed, this, &AMyCharacter::OnLightAttackPressed);
InputComponent->BindAction("LightAttack", IE_Released, this, &AMyCharacter::OnLightAttackReleased);
```

### 6. Incorrect Combo Chain Setup

**Mistake**: Circular combo chains (A → B → A).

**Result**: Infinite combo.

**Fix**: Design linear or branching chains:
```
Light1 → Light2 → Light3 → Finisher
         ↓
      Heavy1 → Heavy2
```

### 7. Not Resetting HitActors

**Mistake**: Forgetting to call ResetHitActors() at start of new attack.

**Result**: Hit detection stops working after first attack.

**Fix**: Call in ExecuteAttack():
```cpp
bool UCombatComponent::ExecuteAttack(UAttackData* AttackData)
{
    // Reset hit tracking
    if (WeaponComponent)
    {
        WeaponComponent->ResetHitActors();
    }

    // ... rest of code
}
```

### 8. Motion Warping Without Target

**Mistake**: Enabling motion warping but no targets available.

**Result**: Character warps to (0,0,0) or strange locations.

**Fix**: Check target exists before setting up warp:
```cpp
AActor* Target = TargetingComponent->FindTarget();
if (Target && AttackData->MotionWarpingConfig.bUseMotionWarping)
{
    SetupMotionWarping(AttackData);
}
```

---

## Console Commands

### Built-in Unreal Commands

```
// Show debug info
showdebug ANIMATION
showdebug COLLISION

// Slow motion (for testing timing windows)
slomo 0.5  // Half speed
slomo 1.0  // Normal speed
slomo 2.0  // Double speed

// Pause game
pause

// Toggle AI
ToggleAILogging
DisableAllScreenMessages
```

### Custom Commands (if implemented)

```cpp
// Example: Add to PlayerController
UFUNCTION(Exec)
void DebugCombat(bool bEnable)
{
    if (UCombatComponent* Combat = GetPawn()->FindComponentByClass<UCombatComponent>())
    {
        Combat->bDebugDraw = bEnable;
    }
}

// Usage in console:
DebugCombat true
DebugCombat false
```

### Stat Commands

```
stat FPS        // Show framerate
stat Unit       // Show frame time breakdown
stat Game       // Show game thread stats
stat RHI        // Show rendering stats
```

---

## Log Categories

### Where to Look in Logs

**LogCombat**: Combat system events
```
LogCombat: Warning: Combat window OPENED for 0.60 seconds
LogCombat: Warning: Queued combo input 1 during combo window
LogCombat: Warning: Processing combo queue, buffer size: 1
```

**LogWeapon**: Hit detection events
```
LogWeapon: Warning: Hit detection ENABLED
LogWeapon: Warning: HIT: Enemy_BP (Damage: 25.0)
LogWeapon: Warning: Hit detection DISABLED
```

**LogAnimation**: Animation system events
```
LogAnimation: Warning: Playing montage: AM_Attack_Light1
LogAnimation: Warning: AnimNotify fired: Attack Phase (Windup)
```

**LogTemp**: General debug messages
```
LogTemp: Warning: CombatComponent: State 0 -> 1
LogTemp: Warning: Executing combo attack DA_LightAttack2 (Count: 2)
```

### Enabling Verbose Logging

In `DefaultEngine.ini`:
```ini
[Core.Log]
LogCombat=Verbose
LogWeapon=Verbose
LogAnimation=Log
```

Or in code:
```cpp
UE_LOG(LogCombat, Verbose, TEXT("Detailed debug info"));
```

### Filtering Logs

In Output Log window:
- Type category name to filter: `LogCombat`
- Use "Verbosity" dropdown for log level
- Right-click → Clear to remove old logs

---

## Debugging Workflow

### Step-by-Step Debugging Process

**1. Identify the Problem**
- What is supposed to happen?
- What actually happens?
- When does it happen (always, sometimes, specific conditions)?

**2. Enable Debug Visualization**
```cpp
CombatComponent->bDebugDraw = true;
TargetingComponent->bDebugDraw = true;
WeaponComponent->bDebugDraw = true;
```

**3. Add Logging**
```cpp
UE_LOG(LogCombat, Warning, TEXT("Attack executing: %s"),
    *AttackData->GetName());
```

**4. Use Breakpoints**
- Open CombatComponent.cpp in Visual Studio
- Click left margin to add breakpoint (red dot)
- Press F5 to debug
- Step through code with F10 (step over) and F11 (step into)

**5. Check Data Assets**
- Are all references assigned?
- Are values configured correctly?
- Are AnimNotifyStates placed correctly in montages?

**6. Test in Isolation**
- Create minimal test scene
- Single attack, no combos
- Verify basic functionality works
- Add complexity incrementally

**7. Compare to Working Example**
- Check ATTACK_CREATION.md guide
- Compare AttackData setup
- Compare AnimMontage setup
- Verify AnimNotifyState placement matches examples

---

## Quick Reference Checklist

### Attack Not Working Checklist

- [ ] CombatSettings assigned to CombatComponent?
- [ ] DefaultLightAttack assigned?
- [ ] AttackData has AttackMontage assigned?
- [ ] AttackMontage has AnimNotifyState_AttackPhase notifies?
- [ ] AnimNotifyStates have correct Phase set (Windup/Active/Recovery)?
- [ ] Character has AnimInstance?
- [ ] AnimInstance routes notifies to CombatComponent?
- [ ] CanAttack() returns true?
- [ ] No compilation errors?

### Combo Not Working Checklist

- [ ] NextComboAttack assigned in AttackData?
- [ ] AnimNotifyState_ComboWindow placed during Recovery?
- [ ] ComboInputWindow duration ~0.6s?
- [ ] OpenComboWindow being called? (check logs)
- [ ] ProcessQueuedCombo being called? (check logs)
- [ ] OnAttackPhaseEnd(Recovery) firing?
- [ ] ComboResetDelay long enough (~2s)?

### Hit Detection Not Working Checklist

- [ ] WeaponComponent added to character?
- [ ] Weapon sockets created (weapon_start, weapon_end)?
- [ ] Socket names match WeaponComponent settings?
- [ ] AnimNotify_ToggleHitDetection placed correctly?
- [ ] TraceChannel set to channel that hits enemies?
- [ ] Enemy has collision enabled?
- [ ] EnableHitDetection being called? (check logs)
- [ ] bDebugDraw enabled to visualize traces?
- [ ] TraceRadius appropriate for weapon size?

---

## Getting Help

If problems persist after checking this guide:

1. **Check logs**: Look for errors/warnings in Output Log
2. **Enable debug draw**: Visual feedback often reveals issues
3. **Simplify**: Test with minimal setup (single attack, no combos)
4. **Compare**: Use working examples from ATTACK_CREATION.md
5. **Ask with details**: Include logs, screenshots of debug draw, and steps to reproduce

Common info needed for debugging:
- Unreal Engine version (5.5, 5.6, etc.)
- Full error message from logs
- Screenshot of debug visualization
- AttackData and CombatSettings configuration
- Steps to reproduce the issue