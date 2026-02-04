# KatanaCombat: Quick Reference Action Checklist
**Based on**: Comprehensive Audit 2026-02-03  
**Purpose**: Quick checklist for development team

---

## 🚨 CRITICAL - Fix Immediately (P0)

### 1. Interface Call Pattern Violations (CRASH RISK)
**Estimated**: 1 day | **Risk**: High - Runtime crashes

**Files to Search**:
```bash
grep -r "->GetCombatState()" Source/KatanaCombat/Private/
grep -r "->CanPerformAttack()" Source/KatanaCombat/Private/
grep -r "->IsInParryWindow()" Source/KatanaCombat/Private/
grep -r "->IsInCounterWindow()" Source/KatanaCombat/Private/
grep -r "->GetHealth()" Source/KatanaCombat/Private/
```

**Pattern to Replace**:
```cpp
// ❌ WRONG (crashes):
ECombatState State = Character->GetCombatState();

// ✅ CORRECT:
ECombatState State = ICombatInterface::Execute_GetCombatState(Character);
```

**Checklist**:
- [ ] Search all files for direct interface method calls
- [ ] Replace with `Execute_` pattern (estimated 15-20 files)
- [ ] Compile and verify no crashes
- [ ] Run full test suite
- [ ] Update CLAUDE.md with examples if needed

---

## 🔴 HIGH PRIORITY - Core Combat Loop (P0)

### 2. Wire Parry Detection Logic
**Estimated**: 2-3 days | **Blocks**: Parry gameplay

**File**: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`

**Current State**: Stub at `BaseCombatCharacter.cpp:635`
```cpp
// TODO: Migrate parry window system
bool ABaseCombatCharacter::IsInParryWindow_Implementation() const
{
    return false; // STUB
}
```

**Implementation Checklist**:
- [ ] Add `OnBlockPressed()` handler in `CombatComponent`
- [ ] Check if `TargetingComponent->GetCurrentTarget()->IsInParryWindow()`
- [ ] If true → `TryParry(Target)`, else → `EnterBlockingState()`
- [ ] In `TryParry()`: Broadcast parry success event
- [ ] In `TryParry()`: Call `ICombatInterface::Execute_OpenCounterWindow(Target, 2.0f)`
- [ ] In `TryParry()`: Trigger slow-motion via `CinematicEffectsUtilityLibrary::ApplySlowMotion()`
- [ ] Play parry animation montage
- [ ] Add test file `ParryExecutionTests.cpp`
- [ ] Test: Parry success within window
- [ ] Test: Parry failure outside window
- [ ] Test: Slow-motion triggering
- [ ] Test: Counter window activation

### 3. Implement Counter Window Tracking
**Estimated**: 2-3 days | **Blocks**: Counter attacks

**File**: `Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp`

**Current State**: Stub at line 622
```cpp
// TODO: Migrate counter window system
bool ABaseCombatCharacter::OpenCounterWindow_Implementation(float Duration)
{
    return false; // STUB
}
```

**Implementation Checklist**:
- [ ] Add to `BaseCombatCharacter.h`: `bool bIsInCounterWindow`, `float CounterWindowEndTime`, `FTimerHandle CounterWindowTimer`
- [ ] Implement `OpenCounterWindow_Implementation()`: Set flag, start timer
- [ ] Implement `CloseCounterWindow()`: Clear flag, broadcast event
- [ ] Implement `IsInCounterWindow_Implementation()`: Check flag and time
- [ ] Add delegates: `OnCounterWindowOpened`, `OnCounterWindowClosed`
- [ ] Add test file `CounterExecutionTests.cpp`
- [ ] Test: Counter window opens after parry
- [ ] Test: Counter window closes after duration (2s)
- [ ] Test: IsInCounterWindow() returns correct state

### 4. Wire Counter Attack Execution
**Estimated**: 2 days | **Blocks**: Counter gameplay

**File**: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`

**Implementation Checklist**:
- [ ] Modify `TryExecuteAttack()`: Check if `Target->IsInCounterWindow()`
- [ ] If counter window open: `ExecuteCounterAttack(Target, AttackData)`
- [ ] Use `AttackData->CounterData` if available, fallback to normal attack
- [ ] Apply damage with `CounterDamageMultiplier` (1.5x)
- [ ] Close counter window on hit
- [ ] Test: Counter damage is 1.5x normal
- [ ] Test: Counter uses paired animation if `CounterData` set
- [ ] Test: Counter window closes after counter attack

---

## 🟡 MEDIUM PRIORITY - AI & Flow (P1)

### 5. Build Attack Token Subsystem
**Estimated**: 3-4 days | **Blocks**: AI coordination

**New Files**:
- `Source/KatanaCombat/Public/Subsystems/CombatTokenSubsystem.h`
- `Source/KatanaCombat/Private/Subsystems/CombatTokenSubsystem.cpp`

**Implementation Checklist**:
- [ ] Create `UCombatTokenSubsystem` inheriting `UWorldSubsystem`
- [ ] Add `TMap<TWeakObjectPtr<AActor>, FTokenPool> TokenPools`
- [ ] Implement `RequestAttackToken(Attacker, Target, Duration)` → returns bool
- [ ] Implement `ReleaseAttackToken(Attacker, Target)`
- [ ] Implement `HasAttackToken(Attacker, Target)`
- [ ] Add cleanup for invalid actors
- [ ] Set `MaxTokensPerTarget = 3` (configurable)
- [ ] Integrate with `EnemyCharacter::TryInitiateAttack()`
- [ ] Add test file `AttackTokenTests.cpp`
- [ ] Test: Max tokens enforced (3 per target)
- [ ] Test: Token release on attack completion
- [ ] Test: Token release on death
- [ ] Test: Multiple targets have independent pools

### 6. Create Attack Telegraph Widget
**Estimated**: 2 days | **Blocks**: Parry timing visibility

**New Files**:
- `Source/KatanaCombat/Public/UI/AttackTelegraphWidget.h`
- `Source/KatanaCombat/Private/UI/AttackTelegraphWidget.cpp`
- Blueprint: `Content/UI/WBP_AttackTelegraph.uasset`

**Implementation Checklist**:
- [ ] Create `UAttackTelegraphWidget` inheriting `UUserWidget`
- [ ] Add `ShowTelegraph(float WindupDuration)` function
- [ ] Add `HideTelegraph()` function
- [ ] Add widget components: `ParryPromptIcon`, `WindupProgressBar`
- [ ] Create Blueprint with pulse animation
- [ ] Attach to `EnemyCharacter` component
- [ ] Wire to `OnAttackPhaseChanged()` event
- [ ] Show during `Windup` phase
- [ ] Hide during `Active` phase
- [ ] Test: Widget visibility timing
- [ ] Test: Multiple enemies show telegraphs simultaneously

### 7. Implement Flow State System
**Estimated**: 2-3 days | **Blocks**: Kill chain mechanic

**File**: `Source/KatanaCombat/Private/Core/CombatComponent.cpp`

**Implementation Checklist**:
- [ ] Add to `CombatComponent.h`: `bool bIsInFlowState`, `float FlowStateTimeRemaining`, `int32 FlowChainCount`
- [ ] Add timer: `FTimerHandle FlowStateTimer`
- [ ] Add config: `float FlowStateDuration = 3.0f`
- [ ] Add delegates: `OnFlowStateEntered`, `OnFlowStateExited`, `OnFlowChainIncremented`
- [ ] Implement `EnterFlowState()`: Set flag, start timer, increment chain
- [ ] Implement `ExitFlowState()`: Clear flag, reset chain
- [ ] Call `EnterFlowState()` in `CompletePairedAnimation()` after lethal finisher
- [ ] Modify `TryExecuteAttack()`: Check if in flow state
- [ ] If in flow: Find nearest enemy in direction → `TryExecuteFinisher()`
- [ ] If no enemy in range: Exit flow state
- [ ] Implement `FindNearestEnemyInDirection(Direction)`
- [ ] Add test file `FlowStateTests.cpp`
- [ ] Test: Flow entry after finisher
- [ ] Test: Chain counter increment
- [ ] Test: Auto-finisher during flow
- [ ] Test: Flow timeout after 3s
- [ ] Test: Flow exit if no enemy in range

### 8. Wire VFX/SFX Triggering
**Estimated**: 1-2 days | **Impact**: Combat feedback

**Files**:
- `Source/KatanaCombat/Private/Animation/AnimNotifyState_PairedAnimationSync.cpp`
- `Source/KatanaCombat/Private/Core/WeaponComponent.cpp`

**Paired Animation Effects Checklist**:
- [ ] In `AnimNotifyState_PairedAnimationSync::NotifyBegin()`:
- [ ] Get `PairedAnimationData` from `CombatComponent`
- [ ] If `ImpactSound` exists: `PlaySoundAtLocation()`
- [ ] If `VictimReactionSound` exists: Play on victim actor
- [ ] If `AttackerVoiceLine` exists: Play on attacker actor
- [ ] If `ImpactVFX` exists: `SpawnSystemAtLocation()`
- [ ] If `SlowMoPostProcessMaterial` exists: Apply to camera
- [ ] If `bSpawnBloodDecals`: `SpawnDecalAtLocation()`

**Normal Attack Effects Checklist**:
- [ ] Add to `AttackData.h`: `SwingSound`, `ImpactSound`, `SwingVFX`, `ImpactVFX`
- [ ] In `AnimNotify_AttackPhaseTransition`: Play `SwingSound` on Active phase
- [ ] In `WeaponComponent::OnWeaponHit()`: Play `ImpactSound` and `ImpactVFX`
- [ ] Manual test: Verify audio plays
- [ ] Manual test: Verify VFX spawns
- [ ] Profile: Check for memory leaks

---

## 🟢 LOW PRIORITY - Polish (P2)

### 9. Blueprint Exposure Cleanup
**Estimated**: 0.5 days | **Impact**: Code quality

**File**: `Source/KatanaCombat/Public/Core/CombatComponent.h`

**Checklist**:
- [ ] Find all `UPROPERTY(BlueprintReadOnly)` internal state variables
- [ ] Remove `BlueprintReadOnly` specifier
- [ ] Create getter functions with `UFUNCTION(BlueprintPure)`
- [ ] Examples: `bIsInComboWindow` → `IsInComboWindow()`
- [ ] Examples: `CurrentChargeTime` → `GetChargePercent()`
- [ ] Compile and verify Blueprint references updated

### 10. Deprecated AnimNotify Audit
**Estimated**: 1 day | **Impact**: Technical debt

**Checklist**:
- [ ] Search all montages for `AnimNotifyState_AttackPhase` usage
- [ ] Replace with `AnimNotify_AttackPhaseTransition` (point notify)
- [ ] Search for `AnimNotify_ToggleHitDetection` usage
- [ ] Replace with automatic hit detection during Active phase
- [ ] Update documentation in `ATTACK_CREATION.md`
- [ ] Test: Verify phase transitions still work

---

## 📋 Testing Checklist

### New Test Files to Create
- [ ] `ParryExecutionTests.cpp`
- [ ] `CounterExecutionTests.cpp`
- [ ] `FlowStateTests.cpp`
- [ ] `AttackTokenTests.cpp`
- [ ] `AttackTelegraphTests.cpp` (UI test, manual)
- [ ] `EffectsIntegrationTests.cpp` (manual)

### Existing Tests to Update
- [ ] `StateTransitionTests.cpp`: Add `FlowState` transitions
- [ ] `IntegrationTests.cpp`: Add full parry→counter→finisher→flow test

### Manual Testing Scenarios
- [ ] Parry an enemy during Windup phase
- [ ] Execute counter attack during counter window
- [ ] Chain 3-5 finishers in flow state
- [ ] Verify telegraph appears during enemy windup
- [ ] Verify audio/VFX play at correct times
- [ ] Test with 3-5 enemies attacking simultaneously

---

## 📊 Progress Tracking Template

Copy this to track progress:

```markdown
## Week 1: Core Combat Loop
- [ ] Day 1: Interface call pattern fix (P0)
- [ ] Day 2-3: Parry detection logic (P0)
- [ ] Day 4-5: Counter window tracking (P0)

## Week 2: AI & Counter Execution
- [ ] Day 1-2: Counter attack execution (P0)
- [ ] Day 3-4: Attack token subsystem (P1)
- [ ] Day 5: Attack telegraph widget (P1)

## Week 3: Flow State & Effects
- [ ] Day 1-2: Flow state management (P1)
- [ ] Day 3-4: VFX/SFX triggering (P1)
- [ ] Day 5: Integration testing

## Week 4: Polish & Testing
- [ ] Blueprint exposure cleanup (P2)
- [ ] AnimNotify deprecation audit (P2)
- [ ] Comprehensive test suite
- [ ] Performance profiling
```

---

## 🎯 Success Metrics

**Definition of Done**:
- [ ] All P0 issues resolved (no crashes, parry works, counter works)
- [ ] Test coverage >80%
- [ ] Full parry→counter→finisher→flow chain functional
- [ ] Attack tokens limit simultaneous enemies to 3
- [ ] Telegraph visible during all enemy windups
- [ ] VFX/SFX play at correct timing
- [ ] No memory leaks (profiled)
- [ ] Frame time <1ms per character

**Playtest Validation**:
- [ ] Parry timing feels fair (300ms window)
- [ ] Counter attacks feel powerful (1.5x damage, cinematic)
- [ ] Flow state creates "power fantasy" moments
- [ ] Enemy attacks are readable and telegraphed
- [ ] Combat flow resembles Batman Arkham (not button-mashy)

---

## Reference Documents

- **Unified Synthesis**: `docs/audits/AUDIT_SYNTHESIS_2026-02-03.md` (cross-referenced findings)
- **Claude Audit**: `docs/audits/AUDIT_CLAUDE_2026-02-03.md` (code-level analysis)
- **Copilot Audit**: `docs/audits/AUDIT_COPILOT_2026-02-03.md` (system-level analysis)
- **Architecture**: `docs/architecture/ARCHITECTURE.md`
- **Best Practices**: `CLAUDE.md`
- **Troubleshooting**: `docs/guides/TROUBLESHOOTING.md`

---

## 🆘 Quick Help

**Interface Call Crashes?**
→ Use `ICombatInterface::Execute_GetCombatState(Actor)` pattern

**Parry Not Triggering?**
→ Check `AnimNotifyState_ParryWindow` on ATTACKER's montage

**Counter Window Not Opening?**
→ Verify `OpenCounterWindow()` is not a stub

**Flow State Not Working?**
→ Check `bIsInFlowState` flag in CombatComponent

**Effects Not Playing?**
→ Verify fields in PairedAnimationData are set

**Need Help?**
→ Check `docs/guides/TROUBLESHOOTING.md` or `docs/audits/AUDIT_SYNTHESIS_2026-02-03.md`

---

**Last Updated**: 2026-02-03  
**Estimated Completion**: 3-4 weeks  
**Priority**: P0 issues MUST be fixed first (crash risk)
