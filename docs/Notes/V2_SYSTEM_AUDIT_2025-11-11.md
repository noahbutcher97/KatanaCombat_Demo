# V2 Combat System Audit - 2025-11-11

## Executive Summary

**Status**: V2 core mechanics are functional but have critical architectural issues that create friction and hard-to-debug behavior. The system needs **aggressive proceduralization** to eliminate state management complexity and improve maintainability.

**Critical Issues Identified**: 3
**Medium Issues**: 5
**Proceduralization Opportunities**: 8

---

## Critical Issues

### 1. **Root Motion Disabled After Blend-Out from Hold** ⚠️ BLOCKING

**Symptom**: Pressing attack during ease-out from hold freeze causes root motion to stop working until next hold input.

**Root Cause**: Movement component state is not synchronized with montage blending state.

**Location**: `CombatComponentV2.cpp:976-988`, `1275-1287`

**Problem Flow**:
```cpp
OnHoldWindowStart() (Light Attack):
  └─ DisableMovement()  // Line 981 - DISABLES character movement

OnEaseTimerTick() (Ease-out complete):
  └─ SetMovementMode(MOVE_Walking)  // Line 1280 - RE-ENABLES movement
  └─ Queue follow-up attack  // Line 1260
```

**Issue**: If player presses attack DURING ease-out (before line 1280 executes):
1. New attack starts playing via `PlayAttackMontage()`
2. Movement is STILL disabled (line 981 never reverted)
3. New attack montage plays WITHOUT root motion
4. Movement stays disabled until hold window triggers again

**Why This Happens**:
- Movement disable/enable is tied to **hold lifecycle** (begin/end)
- Attack execution is **independent of hold lifecycle**
- No synchronization between the two systems

**Impact**: Severe - breaks fundamental combat feel

**Solution**: Movement control should be **procedural** based on montage state, not hold state.

---

### 2. **Blend State Entanglement with Phase State** ⚠️

**Problem**: Blending logic is interwoven with phase transitions, creating complex state dependencies.

**Location**: `CombatComponentV2.cpp:638-715` (PlayAttackMontage)

**Current Blending Flow**:
```cpp
PlayAttackMontage():
  1. Check if CurrentAttackData exists
  2. Extract BlendOutTime from PREVIOUS attack
  3. Extract BlendInTime from NEW attack
  4. Manually call Montage_Stop(BlendOutTime) on current montage
  5. Manually call Montage_PlayWithBlendSettings(BlendInTime) for new montage
  6. Update CurrentAttackData pointer
```

**Issues**:
- **Manual state management**: Developer must remember to update `CurrentAttackData`
- **Blend-out side effects**: Triggers `OnMontageBlendingOut` delegate (line 249)
- **Phase pollution**: Blend state affects phase transition logic
- **No blend cancellation**: If blend-out interrupted, no cleanup path

**Example Failure Case**:
```
Frame 1: Start Attack1, CurrentAttackData = Attack1
Frame 5: Start blending to Attack2 (0.2s blend-out)
  - Montage_Stop(0.2) called
  - OnMontageBlendingOut fires
  - CurrentAttackData STILL = Attack1
Frame 8: Player presses attack again during blend
  - CurrentAttackData = Attack1 (incorrect!)
  - Blend-out time uses Attack1's value (wrong!)
```

**Impact**: Medium-High - causes animation glitches and state corruption

---

### 3. **Hold State Persists Across Attacks** ⚠️

**Problem**: `HoldState` is a single persistent struct that carries state across combo chains.

**Location**: `ActionQueueTypes.h:271-310` (FHoldState struct)

**State That Persists**:
```cpp
struct FHoldState
{
    bool bIsHolding;              // Persists
    bool bIsEasing;               // Persists
    bool bIsEasingOut;            // Persists
    float CurrentPlayRate;        // Persists
    EInputType HeldInputType;     // Persists
    EAttackDirection HoldDirection; // Persists

    // 10+ more fields...
};
```

**Problem**: When starting new attack, old hold state can "leak" into new execution:
```cpp
// Scenario:
Hold Attack1 → Release early (easing out)
Press Attack2 DURING ease-out
  → HoldState.bIsEasing = true (from Attack1)
  → HoldState.bIsEasingOut = true (from Attack1)
  → Attack2 starts with corrupted hold state
```

**Why This Is Bad**:
- Hard to reason about: "Why is my new attack easing when I didn't hold?"
- Difficult to debug: State corruption is silent
- Fragile: Any code path that skips cleanup causes leaks

**Impact**: High - creates unpredictable behavior

---

## Medium Issues

### 4. **Timer-Based Easing is State-Heavy**

**Problem**: Ease transitions use persistent timer + 10+ state fields.

**Location**: `CombatComponentV2.cpp:972-1006` (OnHoldWindowStart), `1172-1323` (OnEaseTimerTick)

**State Required for Easing**:
- `FTimerHandle EaseTimerHandle` (component member)
- `HoldState.bIsEasing` (bool)
- `HoldState.bIsEasingOut` (bool)
- `HoldState.EaseStartTime` (float)
- `HoldState.EaseStartPlayRate` (float)
- `CurrentAttackData->HoldEaseInDuration` (data asset)
- `CurrentAttackData->HoldEaseInType` (data asset)
- `CurrentAttackData->HoldEaseOutDuration` (data asset)
- `CurrentAttackData->HoldEaseOutType` (data asset)
- `CurrentAttackData->HoldTargetPlayRate` (data asset)

**Problem**: 10+ pieces of state to track a simple lerp operation.

**Better Approach**: **Stateless procedural** - calculate playrate from montage time:
```cpp
// No timers, no persistent state, just pure calculation
float GetProceduralPlayRate(float MontageTime, const FHoldConfig& Config)
{
    // Pure function - no side effects
    if (MontageTime < Config.HoldStartTime) return 1.0f;
    if (MontageTime > Config.HoldEndTime) return 1.0f;

    float Alpha = (MontageTime - Config.HoldStartTime) / Config.HoldDuration;
    return FMath::Lerp(1.0f, Config.TargetPlayRate, EaseFunction(Alpha));
}
```

**Impact**: Medium - increases complexity, harder to debug

---

### 5. **Checkpoint Discovery is Manual**

**Problem**: Checkpoints are discovered via `DiscoverCheckpoints()` which scans AnimNotifyStates.

**Location**: `CombatComponentV2.cpp` (line 170-178 in header)

**Current Flow**:
```
PlayAttackMontage()
  → DiscoverCheckpoints(Montage)
    → Scan all AnimNotifyStates
    → Extract window timings
    → Store in Checkpoints array
```

**Issues**:
- **Manual invocation**: Developer must remember to call `DiscoverCheckpoints`
- **Stale data**: If montage changes at runtime, checkpoints are outdated
- **Performance**: Scans ALL notify states every attack

**Better Approach**: **Lazy evaluation** - query windows on-demand from montage:
```cpp
// No discovery, no caching, just query
bool IsInComboWindow(float MontageTime)
{
    // Direct query - always correct, no stale data
    return MontageUtilityLibrary::IsWindowActive(Character, EWindowType::Combo, MontageTime);
}
```

**Impact**: Medium - adds unnecessary complexity

---

### 6. **Action Queue Uses Explicit State Machine**

**Problem**: Each `FActionQueueEntry` has `EActionState` enum (Pending, Executing, Completed, Cancelled).

**Location**: `ActionQueueTypes.h:166-208` (FActionQueueEntry)

**State Transitions**:
```cpp
enum class EActionState : uint8
{
    Pending,      // Waiting for checkpoint
    Executing,    // Currently running
    Completed,    // Finished successfully
    Cancelled     // Interrupted
};
```

**Problem**: State must be manually managed:
```cpp
// Developer must remember to update state
Entry.State = EActionState::Executing;  // Manual
ExecuteAction(Entry);
Entry.State = EActionState::Completed;  // Manual
```

**Better Approach**: **Implicit state** - derive from context:
```cpp
// No enum, state is implicit
bool IsExecuting(const FQueuedAction& Action, float MontageTime)
{
    return MontageTime >= Action.ScheduledTime && !Action.bCompleted;
}
```

**Impact**: Medium - boilerplate code, easy to forget updates

---

### 7. **Blend Times Stored in AttackData**

**Problem**: Blend times are per-attack properties, creating data duplication.

**Location**: `AttackData.h:102-110`

**Current Design**:
```cpp
// EVERY AttackData has these fields:
UPROPERTY(EditAnywhere)
float ComboBlendOutTime = 0.1f;  // Duplicated across all attacks

UPROPERTY(EditAnywhere)
float ComboBlendInTime = 0.1f;   // Duplicated across all attacks

// If you want to change "all light attacks blend faster":
// → Must edit 20+ AttackData assets individually
```

**Better Approach**: **Global blend policy** + **attack-specific overrides**:
```cpp
// CombatSettings (global defaults):
BlendOutTime_Light = 0.05f
BlendOutTime_Heavy = 0.15f
BlendInTime_Default = 0.1f

// AttackData (override if needed):
bOverrideBlendOut = false  // Use global
BlendOutTimeOverride = 0.2f  // Only if bOverride = true
```

**Impact**: Low-Medium - data management burden

---

### 8. **Input Queue is Press/Release Pairs**

**Problem**: Input system manually matches press/release events.

**Location**: `CombatComponentV2.cpp:387-399` (ProcessInputPair)

**Current Design**:
```cpp
HeldInputs TMap<EInputType, float>  // Tracks press timestamp

OnInputEvent(Press):
  → HeldInputs.Add(Type, Timestamp)

OnInputEvent(Release):
  → HeldInputs.Remove(Type)
  → ProcessInputPair(Press, Release)
```

**Problem**: Manual bookkeeping, easy to desync:
- Forget to remove on release → "stuck" input
- Press during attack → might queue duplicate
- No validation of press/release ordering

**Better Approach**: **Query-based** - ask "is button down NOW?":
```cpp
// No tracking, just query engine state
bool IsButtonHeld(EInputType Type)
{
    return PlayerController->IsInputKeyDown(GetKeyForInput(Type));
}
```

**Impact**: Low-Medium - error-prone state management

---

## Proceduralization Opportunities

### 1. **Movement Control Should Be Montage-Aware** 🔧 HIGH PRIORITY

**Current**: Movement disabled/enabled by hold system manually.

**Proposed**: **Procedural movement policy** based on montage state:

```cpp
// Tick-based or event-driven
void UpdateMovementState()
{
    UAnimMontage* CurrentMontage = GetCurrentMontage();
    if (!CurrentMontage) {
        EnableMovement();  // Idle - always movable
        return;
    }

    float MontageTime = GetMontageTime();

    // Check if any "movement lock" window is active
    bool bShouldLockMovement = IsWindowActive(EWindowType::MovementLock, MontageTime)
                            || (IsHolding() && GetHoldPlayRate() < 0.5f);

    if (bShouldLockMovement) {
        DisableMovement();
    } else {
        EnableMovement();
    }
}
```

**Benefits**:
- **Automatic**: No manual enable/disable calls
- **Correct**: Always synced with montage state
- **Debuggable**: Single source of truth

**Implementation**:
- Add `AnimNotifyState_MovementLock` for designer control
- Or: Auto-detect based on playrate (< 0.5 = locked)
- Hook into TickComponent or phase transitions

---

### 2. **Blend System Should Be Declarative** 🔧 HIGH PRIORITY

**Current**: Manual `Montage_Stop` + `Montage_PlayWithBlendSettings` calls.

**Proposed**: **Declarative blend intent**:

```cpp
// Designer specifies INTENT, system handles HOW
UPROPERTY(EditAnywhere)
EBlendPolicy ComboBlendPolicy = EBlendPolicy::Crossfade;

enum class EBlendPolicy
{
    Instant,       // No blend (default attacks)
    Crossfade,     // Smooth blend (combos)
    BlendOut,      // Fade to idle (finishers)
    Override       // Custom blend times
};

// System automatically applies policy
void PlayAttackWithBlendPolicy(UAttackData* Attack)
{
    switch (Attack->ComboBlendPolicy) {
        case Crossfade:
            ApplyCrossfadeBlend(PreviousAttack, Attack);
            break;
        case Instant:
            InstantTransition(Attack);
            break;
        // ...
    }
}
```

**Benefits**:
- **Declarative**: Designer expresses WHAT, not HOW
- **Centralized**: Blend logic in one place
- **Extensible**: Easy to add new policies

---

### 3. **Hold State Should Be Stateless** 🔧 HIGH PRIORITY

**Current**: Persistent `FHoldState` struct with 10+ fields.

**Proposed**: **Query-based hold detection**:

```cpp
// No persistent state - just query montage + input at point in time
struct FHoldInfo
{
    static FHoldInfo Calculate(ACharacter* Character, float MontageTime)
    {
        FHoldInfo Info;

        // Is button currently held? (query controller)
        Info.bIsButtonHeld = IsButtonDown(Character, EInputType::LightAttack);

        // Is hold window active? (query montage)
        Info.bIsInHoldWindow = IsWindowActive(EWindowType::Hold, MontageTime);

        // Should hold be active?
        Info.bShouldHold = Info.bIsButtonHeld && Info.bIsInHoldWindow;

        // What playrate should we use? (procedural calculation)
        if (Info.bShouldHold) {
            float HoldDuration = MontageTime - Info.WindowStartTime;
            Info.TargetPlayRate = CalculateHoldPlayRate(HoldDuration);
        } else {
            Info.TargetPlayRate = 1.0f;
        }

        return Info;  // Pure calculation - no side effects
    }
};

// Usage in Tick:
void TickComponent(float DeltaTime)
{
    FHoldInfo HoldInfo = FHoldInfo::Calculate(Character, GetMontageTime());
    SetMontagePlayRate(HoldInfo.TargetPlayRate);
}
```

**Benefits**:
- **No state leaks**: Every frame recalculates from scratch
- **Debuggable**: Hover over variable = see current value
- **Testable**: Pure function, easy to unit test

---

### 4. **Easing Should Use Curves, Not Timers** 🔧 MEDIUM PRIORITY

**Current**: Timer-based easing with persistent state.

**Proposed**: **Curve-driven playrate**:

```cpp
// AttackData:
UPROPERTY(EditAnywhere)
UCurveFloat* PlayRateCurve;  // Designer-authored curve

// Runtime - NO timers, NO state:
float GetProceduralPlayRate()
{
    if (!PlayRateCurve) return 1.0f;

    float MontageTime = GetMontageTime();
    return PlayRateCurve->GetFloatValue(MontageTime);  // Sample curve
}

// Set playrate every tick (or on-demand):
SetMontagePlayRate(GetProceduralPlayRate());
```

**Curve Editor Benefits**:
- **Visual**: See playrate over time in editor
- **Flexible**: Ease-in, ease-out, multi-stage, bounce, etc.
- **Data-driven**: No code changes for new easing patterns
- **Stateless**: Curve sampling is a pure function

**Fallback**: If no curve, use procedural easing (current system).

---

### 5. **Checkpoint System Should Be Lazy** 🔧 MEDIUM PRIORITY

**Current**: Eager discovery via `DiscoverCheckpoints()`.

**Proposed**: **Lazy window queries**:

```cpp
// No checkpoint array, no discovery - just query
bool ShouldExecuteQueuedAction(const FQueuedAction& Action, float MontageTime)
{
    // Query window state on-demand from montage
    bool bInComboWindow = IsWindowActive(EWindowType::Combo, MontageTime);

    return (bInComboWindow && Action.ExecutionMode == Snap)
        || (CurrentPhase == Recovery && Action.ExecutionMode == Responsive);
}
```

**Benefits**:
- **Always correct**: No stale data
- **Less code**: No discovery, no caching
- **Flexible**: Works with runtime-modified montages

---

### 6. **Action Queue Should Use Implicit State** 🔧 LOW PRIORITY

**Current**: Explicit `EActionState` enum.

**Proposed**: **Derive state from context**:

```cpp
struct FQueuedAction
{
    UAttackData* Attack;
    float Timestamp;
    float ScheduledTime;
    bool bCompleted;  // Only flag needed

    // State is implicit:
    bool IsPending(float CurrentTime) const {
        return !bCompleted && CurrentTime < ScheduledTime;
    }

    bool IsExecuting(float CurrentTime) const {
        return !bCompleted && CurrentTime >= ScheduledTime;
    }
};
```

**Benefits**:
- **Less boilerplate**: No manual state updates
- **Correct**: State derived from facts

---

### 7. **Blend Times Should Be Policies** 🔧 LOW PRIORITY

**Current**: Per-attack blend time properties.

**Proposed**: **Global policies with overrides**:

```cpp
// CombatSettings (global):
UPROPERTY(EditAnywhere)
FBlendPolicy DefaultLightBlend;  // Fast (0.05s)

UPROPERTY(EditAnywhere)
FBlendPolicy DefaultHeavyBlend;  // Slow (0.15s)

// AttackData (override if needed):
UPROPERTY(EditAnywhere)
bool bOverrideBlendPolicy = false;

UPROPERTY(EditAnywhere, meta=(EditCondition="bOverrideBlendPolicy"))
FBlendPolicy CustomBlend;
```

**Benefits**:
- **Centralized defaults**: Change all light attacks at once
- **Override flexibility**: Special cases still supported
- **Less data duplication**: Most attacks use defaults

---

### 8. **Input System Should Query Controller State** 🔧 LOW PRIORITY

**Current**: Manual press/release tracking via `HeldInputs` map.

**Proposed**: **Query input state from controller**:

```cpp
bool IsButtonCurrentlyHeld(EInputType Type)
{
    APlayerController* PC = GetPlayerController();
    FKey Key = GetKeyForInputType(Type);
    return PC->IsInputKeyDown(Key);
}

// Usage in hold detection:
void OnHoldWindowStart(EInputType Type)
{
    if (IsButtonCurrentlyHeld(Type)) {
        ActivateHold();
    }
}
```

**Benefits**:
- **No bookkeeping**: Controller is source of truth
- **Always correct**: No desync possible
- **Simpler**: Remove HeldInputs map entirely

---

## Architectural Recommendations

### 1. **Embrace Stateless Procedural Systems**

**Philosophy**: Calculate state on-demand instead of tracking it.

**Apply To**:
- Movement control (query montage + hold state)
- Hold detection (query button + window)
- Easing (sample curve or calculate)
- Window queries (scan montage on-demand)

**Benefits**:
- **No state leaks**: Every frame recalculates
- **Easy debugging**: State = current values, not history
- **Testable**: Pure functions

---

### 2. **Separate Concerns: Blend vs. Execution vs. State**

**Current Problem**: `PlayAttackMontage` does everything:
- Blend management
- Montage playback
- State updates
- Checkpoint discovery

**Proposed**:
```cpp
// Clean separation:
BlendManager::TransitionTo(NewAttack);  // Handles blending
MontagePlayer::Play(Attack);            // Handles playback
PhaseTracker::Update(NewPhase);         // Handles state
```

**Benefits**:
- **Single responsibility**: Each system does one thing
- **Testable**: Mock dependencies easily
- **Maintainable**: Change blend logic without affecting execution

---

### 3. **Use Data-Driven Configuration Over Code**

**Examples**:
- Blend policies (not manual blend time values)
- Movement lock windows (not DisableMovement calls)
- Playrate curves (not timer-based easing)
- Window definitions (AnimNotifyStates, not hardcoded)

**Benefits**:
- **Designer empowerment**: Tune without programmer
- **Iteration speed**: No recompile for tweaks
- **Consistency**: Centralized rules

---

### 4. **Event-Driven Over Polling**

**Keep**:
- Phase transitions (OnPhaseTransition) ✅
- Montage events (OnMontageEnded) ✅

**Add**:
- Window events (OnWindowStart/End)
- Blend events (OnBlendComplete)

**Remove**:
- Tick-based checkpoint polling
- Timer-based easing updates

**Benefits**:
- **Performance**: No wasted checks
- **Correctness**: React immediately to changes
- **Clarity**: Flow is explicit

---

## Proposed Refactoring Plan

### Phase 1: Critical Fixes (1-2 days)
1. **Fix root motion bug**: Add procedural movement control
2. **Decouple blend from phase**: Extract blend logic into separate system
3. **Clear hold state on attack start**: Prevent state leaks

### Phase 2: Proceduralization (3-5 days)
1. **Replace timer easing with curve sampling**
2. **Make hold detection stateless** (query-based)
3. **Add lazy window queries**
4. **Remove manual checkpoint discovery**

### Phase 3: Policy-Driven Design (2-3 days)
1. **Implement blend policies** (Instant, Crossfade, Custom)
2. **Add global blend defaults** with per-attack overrides
3. **Create movement lock policy** (window-based)
4. **Centralize playrate calculation**

### Phase 4: Testing & Polish (2-3 days)
1. **Unit tests for stateless functions**
2. **Integration tests for blend transitions**
3. **Performance profiling** (procedural vs. cached)
4. **Documentation updates**

**Total Estimated Time**: 8-13 days for full refactor

---

## Immediate Action Items

### Priority 1: Fix Root Motion Bug (Blocking)
**File**: `CombatComponentV2.cpp`

**Change**:
```cpp
// Replace manual DisableMovement/SetMovementMode with:
void UpdateMovementFromMontageState()
{
    // Query if movement should be locked
    bool bShouldLock = IsHolding() && GetHoldPlayRate() < 0.5f;

    if (bShouldLock) {
        if (!bMovementCurrentlyDisabled) {
            DisableMovement();
            bMovementCurrentlyDisabled = true;
        }
    } else {
        if (bMovementCurrentlyDisabled) {
            SetMovementMode(MOVE_Walking);
            bMovementCurrentlyDisabled = false;
        }
    }
}

// Call from:
- TickComponent() (every frame check)
- OnEaseTimerTick() (during transitions)
- PlayAttackMontage() (on new attack start)
```

**Test**: Press attack during hold ease-out → root motion should work.

---

### Priority 2: Clear Hold State on New Attack
**File**: `CombatComponentV2.cpp:PlayAttackMontage`

**Change**:
```cpp
bool PlayAttackMontage(UAttackData* AttackData)
{
    // ... existing blend logic ...

    // CRITICAL: Clear hold state when starting new attack
    if (HoldState.bIsHolding || HoldState.bIsEasing)
    {
        // Cancel any active ease timer
        if (EaseTimerHandle.IsValid()) {
            GetWorld()->GetTimerManager().ClearTimer(EaseTimerHandle);
        }

        // Reset hold state
        HoldState.Deactivate();

        // Ensure movement re-enabled
        UpdateMovementFromMontageState();
    }

    // ... continue with montage playback ...
}
```

**Test**: Hold → press attack during ease → next attack should be clean.

---

### Priority 3: Add Movement Debug Visualization
**File**: `CombatComponentV2.cpp:DrawDebugInfo`

**Change**:
```cpp
void DrawDebugInfo() const
{
    // ... existing debug draws ...

    // Show movement state
    FString MovementState = bMovementCurrentlyDisabled
        ? "DISABLED"
        : "ENABLED";
    FColor MovementColor = bMovementCurrentlyDisabled
        ? FColor::Red
        : FColor::Green;

    DrawDebugString(World, DebugLocation + FVector(0, 0, 100),
        FString::Printf(TEXT("Movement: %s"), *MovementState),
        nullptr, MovementColor, 0.0f, true);
}
```

**Benefit**: See movement state at a glance during debugging.

---

## Testing Strategy

### Unit Tests (Isolated)
- `CalculateHoldPlayRate()` - pure function
- `IsWindowActive()` - query function
- `DetermineBlendPolicy()` - policy selection
- `CalculateProceduralEasing()` - easing math

### Integration Tests (In-Engine)
- Attack during hold ease-out → root motion works
- Rapid combo inputs → no state leaks
- Blend cancellation → no animation glitches
- Hold → release → new attack → movement restored

### Gameplay Tests (Manual)
- Mash light attack → smooth combos
- Hold light → release during ease → next attack clean
- Heavy charge → cancel → movement works
- Directional follow-ups → animation transitions smooth

---

## Metrics for Success

**Before Refactor**:
- Root motion bug: PRESENT
- State fields in HoldState: 15+
- Lines of code in OnEaseTimerTick: ~150
- Manual state updates: ~20 call sites

**After Refactor (Target)**:
- Root motion bug: FIXED
- State fields: < 5 (or zero for stateless)
- Lines of code for easing: ~30 (query + sample)
- Manual state updates: 0 (all procedural)

---

## Conclusion

The V2 system is **functionally complete** but suffers from **excessive state management** and **manual synchronization** between subsystems. The root motion bug is a symptom of this deeper architectural issue.

**Recommendation**: Prioritize **procedural design** over state tracking. Every persistent state field is a potential bug. Embrace:
- **Query-based logic** (ask "what is the current state?")
- **Curve-driven behavior** (sample curves, don't track timers)
- **Declarative policies** (specify intent, not implementation)
- **Event-driven updates** (react to changes, don't poll)

**Immediate Next Steps**:
1. Fix root motion bug (1 day)
2. Clear hold state on new attacks (0.5 days)
3. Prototype stateless hold detection (1 day)
4. Evaluate curve-based easing (1 day)

**Long-term Goal**: Make the system **"pit of success"** - hard to use wrong, easy to use right.
