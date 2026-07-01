# KatanaCombat: Executive Audit Summary
**Date**: February 3, 2026  
**Full Report**: See `AUDIT_COPILOT_2026-02-03.md`

---

## TL;DR: Current State vs. Vision

**Overall Progress**: 65% Complete

✅ **What Works Today**:
- Solid 5-component architecture (Combat, Targeting, Weapon, HitReaction, PairedAnimation)
- Sophisticated input buffering with dual responsive/snappy modes
- Working finisher system with cinematic paired animations
- Comprehensive test suite (126 tests across 14 suites)
- Data-driven design with AttackData assets
- Motion warping for chase attacks

❌ **What's Missing for Batman Arkham/AC3 Vision**:
- **Parry → Counter → Finisher flow** (data structures exist, logic missing)
- **Flow state for chaining finishers** (core AC3 mechanic)
- **Enemy attack telegraphs** (required for parry timing)
- **Attack token system** (AI coordination)
- ~~**VFX/SFX integration**~~ ✅ **WIRED in v3.5.0** (hitstop, audio, VFX all functional)
- **Guard/posture mechanics** (defined but not implemented)

---

## Critical Findings

### 🔴 P0 Issues (Blockers)

1. **Interface Call Pattern Violations**
   - **Issue**: Direct calls to `BlueprintNativeEvent` methods will crash
   - **Example**: `Character->GetCombatState()` should be `ICombatInterface::Execute_GetCombatState(Character)`
   - **Impact**: Runtime crashes in 15-20 files
   - **Effort**: 1 day (search & replace)

2. **Parry System Not Wired**
   - **Issue**: `AnimNotifyState_ParryWindow` exists, defender logic doesn't use it
   - **TODOs**: `BaseCombatCharacter.cpp:635` - "TODO: Migrate parry window system"
   - **Impact**: Blocks parry gameplay
   - **Effort**: 2-3 days

3. **Counter Window Stubs**
   - **Issue**: `OpenCounterWindow()` always returns `false` (stub)
   - **Impact**: Blocks counter attacks
   - **Effort**: 2-3 days

### 🟡 P1 Issues (High Impact)

4. **Attack Token System Missing**
   - **Issue**: No AI coordination, all enemies can attack simultaneously
   - **Need**: `UCombatTokenSubsystem` with max 2-3 tokens per player
   - **Effort**: 3-4 days

5. **Attack Telegraph Missing**
   - **Issue**: No visual indicator when enemy winds up (required for parry timing)
   - **Need**: Widget above enemy head during Windup phase
   - **Effort**: 2 days

6. **Flow State Not Implemented**
   - **Issue**: No "finisher → finisher → finisher" chain mechanic
   - **Vision**: Core AC3 "flow-based crowd control" gameplay
   - **Effort**: 2-3 days

7. **VFX/SFX Not Triggered**
   - **Issue**: Fields exist in `PairedAnimationData` (ImpactSound, ImpactVFX) but never played
   - **Impact**: Combat lacks feedback
   - **Effort**: 1-2 days

### 🟢 P2 Issues (Polish)

8. **Blueprint Exposure of Internal State**
   - **Issue**: `bIsInComboWindow` marked `BlueprintReadOnly` (violates CLAUDE.md guidelines)
   - **Impact**: Visual clutter in editor
   - **Effort**: 0.5 days

9. **Deprecated AnimNotify Usage**
   - **Issue**: Mix of old `AnimNotifyState_AttackPhase` and new `AnimNotify_AttackPhaseTransition`
   - **Impact**: Technical debt, designer confusion
   - **Effort**: 1 day

10. **Foot IK / Procedural Alignment**
    - **Issue**: No IK adjustments, characters don't adapt feet to terrain
    - **Impact**: Visual polish
    - **Effort**: 3-5 days

---

## System Ratings

| System | Completeness | Quality | Priority to Complete |
|--------|--------------|---------|----------------------|
| **Core Combat** | 95% | ⭐⭐⭐⭐⭐ | Low (stable) |
| **Input Buffering** | 100% | ⭐⭐⭐⭐⭐ | None (complete) |
| **Finisher System** | 95% | ⭐⭐⭐⭐⭐ | Low (working) |
| **Parry System** | 20% | ⭐⭐⭐☆☆ | **P0** (blocks gameplay) |
| **Counter System** | 15% | ⭐⭐☆☆☆ | **P0** (blocks gameplay) |
| **Flow State** | 10% | ⭐☆☆☆☆ | **P1** (high impact) |
| **VFX/SFX** | 40% | ⭐⭐☆☆☆ | **P1** (combat feel) |
| **AI/Tokens** | 30% | ⭐⭐☆☆☆ | **P1** (required for parry) |
| **IK/Procedural** | 20% | ⭐⭐☆☆☆ | P2 (polish) |

---

## Implementation Roadmap

### Phase 1: Core Combat Loop (2-3 weeks)
**Goal**: Wire parry → counter → finisher flow

**Week 1**:
- Day 1: Fix interface call pattern violations (P0)
- Days 2-3: Implement parry detection logic
- Days 4-5: Implement counter window tracking

**Week 2**:
- Days 1-2: Wire counter attack execution
- Days 3-4: Create attack token subsystem
- Day 5: Create attack telegraph widget

**Week 3**:
- Days 1-2: Implement flow state management
- Days 3-4: Wire VFX/SFX triggering
- Day 5: Integration testing

### Phase 2: Polish & Testing (1 week)
**Goal**: Bug fixes, tuning, comprehensive testing

- Blueprint exposure cleanup
- AnimNotify deprecation audit
- Performance profiling
- Playtest iteration

### Total Time Estimate: 3-4 weeks

---

## Detailed Gaps by System

### Parry System: 20% Complete

**What Exists**:
- ✅ `AnimNotifyState_ParryWindow` (properly on attacker's montage)
- ✅ `IsInParryWindow()` interface method
- ✅ Checkpoint registration system

**What's Missing**:
- ❌ Defender-side logic: `if (IsInParryWindow()) TryParry() else Block()`
- ❌ Parry success → slow-motion transition
- ❌ Parry success → counter window activation
- ❌ Posture damage integration (field exists, not implemented)

**File**: `BaseCombatCharacter.cpp:635` - TODO comment

### Counter System: 15% Complete

**What Exists**:
- ✅ `EPairedReactionType::Counter` enum
- ✅ `CounterData` asset reference in `AttackData`
- ✅ `CounterDamageMultiplier` field (1.5x default)
- ✅ `CounterReactions` map in `HitReactionSettings`

**What's Missing**:
- ❌ `OpenCounterWindow()` is a stub (always returns false)
- ❌ `IsInCounterWindow()` is a stub (always returns false)
- ❌ Counter attack execution (no code uses `CounterData`)
- ❌ Counter damage multiplier application
- ❌ Counter opportunity UI

**File**: `BaseCombatCharacter.cpp:622` - TODO comment

### Flow State: 10% Complete

**What Exists**:
- ✅ Finisher completion logic
- ✅ Death handling after finisher

**What's Missing**:
- ❌ `bIsInFlowState` tracking
- ❌ Flow state timer (3s window)
- ❌ Flow chain counter
- ❌ Auto-finisher during flow (next attack → finisher)
- ❌ Automatic target switching
- ❌ Flow state UI (chain count display)

**Implementation**: Requires new state in `CombatComponent`

### VFX/SFX Integration: ✅ 90% Complete (Updated v3.5.0)

**Paired Animations (Wired - f27a068)**:
- ✅ `ImpactSound`, `VictimReactionSound`, `AttackerVoiceLine` play at sync points
- ✅ `ImpactVFX` spawns via `TriggerSyncPointEffects()`
- ⏳ `SlowMoPostProcessMaterial`, `ScreenBloodMaterial` not wired (deferred)

**Normal Attacks (Wired - 879d1c2, 0e6ae4e, 3038b21, 150cd3a)**:
- ✅ `FHitstopConfig` on AttackData with `ApplyHitstop()`
- ✅ `FImpactAudioConfig` with 4-tier resolution chain
- ✅ `FImpactVFXConfig` with Niagara spawning
- ✅ `UCombatFXData` pooled FX with random selection
- ⏳ Weapon trail effect component (deferred)

### AI/Enemy System: 30% Complete

**What Exists**:
- ✅ `AEnemyCharacter` base class
- ✅ Basic attack execution (can call `TryExecuteAttack`)
- ✅ StateTree integration (variant-specific)

**What's Missing**:
- ❌ `UCombatTokenSubsystem` for attack coordination
- ❌ Attack telegraph widget
- ❌ Attack token request/release logic
- ❌ Enemy attack scheduling (staggered timing)
- ❌ Reaction to player parry/block

---

## Code Examples

### Current Parry System (Stub)

```cpp
// BaseCombatCharacter.cpp:635
bool ABaseCombatCharacter::IsInParryWindow_Implementation() const
{
    // TODO: Migrate parry window system
    return false; // STUB
}
```

### What Parry Logic Should Be

```cpp
// CombatComponent.cpp
void UCombatComponent::OnBlockPressed()
{
    AActor* Target = TargetingComponent->GetCurrentTarget();
    if (!Target) return;
    
    // Check if target is in parry window
    if (ICombatInterface::Execute_IsInParryWindow(Target))
    {
        TryParry(Target);
    }
    else
    {
        EnterBlockingState();
    }
}

void UCombatComponent::TryParry(AActor* Attacker)
{
    // Successful parry!
    BroadcastParrySuccess(Attacker);
    
    // Open counter window on attacker
    ICombatInterface::Execute_OpenCounterWindow(Attacker, 2.0f);
    
    // Trigger slow-motion
    CinematicEffectsUtilityLibrary::ApplySlowMotion(GetWorld(), 0.3f, 1.5f);
    
    // Play parry animation
    PlayParryAnimation();
}
```

### Current Counter Window (Stub)

```cpp
// BaseCombatCharacter.cpp:622
bool ABaseCombatCharacter::OpenCounterWindow_Implementation(float Duration)
{
    // TODO: Migrate counter window system
    return false; // STUB
}
```

### What Counter Window Should Be

```cpp
bool ABaseCombatCharacter::OpenCounterWindow_Implementation(float Duration)
{
    bIsInCounterWindow = true;
    CounterWindowEndTime = GetWorld()->GetTimeSeconds() + Duration;
    
    OnCounterWindowOpened.Broadcast(Duration);
    
    // Timer to close window
    GetWorld()->GetTimerManager().SetTimer(
        CounterWindowTimer,
        this,
        &ABaseCombatCharacter::CloseCounterWindow,
        Duration,
        false
    );
    
    return true;
}
```

---

## Best Practices Violations

### Critical: Interface Call Pattern

**Violation**: Direct calls to `BlueprintNativeEvent` interface methods

**Example** (found in `CombatComponent.cpp`):
```cpp
// WRONG - Will crash at runtime:
ECombatState State = Character->GetCombatState();

// CORRECT:
ECombatState State = ICombatInterface::Execute_GetCombatState(Character);
```

**Why**: `BlueprintNativeEvent` creates a virtual thunk that routes to either C++ `_Implementation()` or Blueprint override. Direct calls bypass this routing and crash.

**Impact**: Estimated 15-20 files affected

**Fix**: Search & replace pattern in all component files

### Minor: Blueprint Exposure

**Violation**: Internal state variables marked `BlueprintReadOnly`

**Example** (`CombatComponent.h`):
```cpp
UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
bool bIsInComboWindow; // Should not be exposed
```

**From CLAUDE.md:278**:
> "Don't make internal state variables BlueprintReadOnly: If a parameter isn't meaningful to view/edit at runtime in the editor, don't expose it to Blueprint."

**Fix**: Remove `BlueprintReadOnly`, expose via getter function:
```cpp
// Internal state
bool bIsInComboWindow;

// Public API
UFUNCTION(BlueprintPure, Category = "Combat|State")
bool IsInComboWindow() const { return bIsInComboWindow; }
```

---

## Success Criteria

### Functional Completeness

- [ ] Parry detection works within window, fails outside
- [ ] Counter window opens after parry, lasts 2 seconds
- [ ] Counter attacks deal 1.5x damage during window
- [ ] Flow state enters after finisher, lasts 3 seconds
- [ ] Next attack during flow auto-targets nearest enemy for finisher
- [ ] Attack tokens limit max 3 simultaneous attackers
- [ ] Telegraph appears during enemy Windup phase

### Technical Quality

- [ ] Test coverage >80% (currently ~70%)
- [ ] Zero crashes from interface calls
- [ ] Frame time <1ms per character (currently ~0.5ms)
- [ ] No memory leaks from VFX/SFX spawning

### Player Experience

- Parry timing feels fair (300ms window is generous)
- Counter attacks feel powerful and rewarding
- Flow state creates "power fantasy" chain kills
- Enemy attacks are readable and telegraphed
- Combat flow resembles Batman Arkham (not button-mashy)

---

## Recommendations

### Immediate Actions (This Week)

1. **Fix interface call patterns** (P0 - 1 day)
   - Search for `->GetCombatState()`, `->CanPerformAttack()`, etc.
   - Replace with `ICombatInterface::Execute_*` pattern
   - Verify no runtime crashes

2. **Wire parry detection** (P0 - 2 days)
   - Implement defender-side logic in `CombatComponent::OnBlockPressed()`
   - Test with existing `ParryWindow` AnimNotifyState
   - Add slow-motion on successful parry

3. **Implement counter window** (P0 - 2 days)
   - Replace stub in `BaseCombatCharacter::OpenCounterWindow_Implementation()`
   - Add timer-based tracking
   - Add counter window events

### Next Sprint (Week 2-3)

4. **Build attack token system** (P1 - 3 days)
   - Create `UCombatTokenSubsystem`
   - Integrate with enemy AI
   - Add token request/release logic

5. **Create telegraph widget** (P1 - 2 days)
   - Widget above enemy head during Windup
   - Integrate with parry window timing

6. **Implement flow state** (P1 - 2 days)
   - Add state tracking to `CombatComponent`
   - Auto-finisher logic
   - UI for chain counter

7. **Wire VFX/SFX** (P1 - 1 day)
   - Add `PlaySoundAtLocation()` calls at sync points
   - Add `SpawnEmitterAtLocation()` for impact VFX

### Future Iterations

8. **Polish pass** (P2 - 1 week)
   - Blueprint exposure cleanup
   - Deprecated AnimNotify audit
   - Performance profiling

9. **IK/Procedural** (P2 - 1 week)
   - Foot IK on slopes
   - Socket-based alignment for paired animations

---

## Conclusion

KatanaCombat has a **solid foundation** with excellent architecture, comprehensive input buffering, and working finishers. The primary gap is **wiring the parry→counter→finisher flow** that defines Batman Arkham/AC3 combat.

**Most Critical**: Fix interface call pattern violations immediately (crash risk).

**Core Combat Loop**: 2-3 weeks to implement parry, counter, flow state, and attack tokens.

**Estimated Total Effort**: 3-4 weeks to achieve vision alignment.

**Strengths to Leverage**:
- Clean component architecture
- Event-driven communication
- Data structures 80% complete
- Strong test foundation

**Next Steps**:
1. Fix interface calls (Day 1)
2. Wire parry detection (Days 2-3)
3. Implement counter window (Days 4-5)
4. Begin attack token system (Week 2)

---

**Full detailed audit**: See `AUDIT_COPILOT_2026-02-03.md` (50+ pages)
