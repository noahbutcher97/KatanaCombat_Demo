---
title: "[GAP-1.1] AI Attack Token System not implemented"
labels: ["gap", "priority: p1", "type: feature", "area: ai", "system: paired-animation", "status: pending"]
---

## Gap Overview
**No AI coordination system to prevent simultaneous attacks during player's paired animation vulnerability**

Currently, multiple AI enemies can attempt finishers or attacks on the player simultaneously, creating unfair situations and potential system conflicts when the player is locked in a paired animation sequence.

## Classification
**Category:** AI/ENEMY COORDINATION (Section 1)  
**Priority:** P1 - HIGH - Core gameplay fairness and system stability  
**Status:** Pending ⏳ Awaiting implementation

## Combat System Context
In **Ghost of Tsushima-inspired** melee combat, the **Attack Token System** ensures fair AI behavior by limiting how many enemies can attack simultaneously. This is critical during:

### Impact on KatanaCombat Systems
- **Paired Animations**: Player locked in finisher - defenseless against other enemies
- **Parry Windows**: Player timing parry - shouldn't be interrupted by off-screen attack
- **Counter Sequences**: Player executing counter - other enemies should wait
- **Combat Balance**: 1vN encounters remain challenging but fair
- **Player Agency**: Maintains sense of control even when outnumbered

### Related Combat Components
- AI Controllers (when implemented)
- `UCombatComponent::TryExecuteFinisher()` - Player vulnerability window
- `UCombatComponent::IsInPairedAnimation()` - Query for AI decision-making
- Future: `UCombatTokenSubsystem` (Phase 5b-5 in roadmap)

## Design Specification

### Token System Architecture

**Core Concept:**
- **Token Pool**: Limited number of "attack tokens" (typically 1-3)
- **Token Request**: AI must acquire token before attacking player
- **Token Release**: Returned after attack completes or is interrupted
- **Priority System**: Certain attack types (finishers) have priority

**Implementation Approach (UE5.6):**

```cpp
// New Subsystem
UCLASS()
class UCombatTokenSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
    
public:
    // Request permission to attack target
    UFUNCTION(BlueprintCallable)
    bool RequestAttackToken(AActor* Requester, AActor* Target, EAttackType AttackType);
    
    // Return token when attack completes
    UFUNCTION(BlueprintCallable)
    void ReleaseAttackToken(AActor* Requester);
    
    // Query if target is currently under attack
    UFUNCTION(BlueprintPure)
    int32 GetActiveAttackerCount(AActor* Target) const;
    
private:
    // Token allocation per target
    TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<AActor>>> ActiveAttackers;
    
    // Configuration
    UPROPERTY()
    int32 MaxSimultaneousAttackers = 2;  // Configurable via CombatSettings
};
```

### Integration Points

**AI Behavior Tree:**
```
[Decision Node: Can Attack Player?]
  ├─ Check: Token Available? (RequestAttackToken)
  ├─ Check: Player in Paired Animation? (IsInPairedAnimation)
  ├─ Check: Player in Iframe? (IsInvulnerable)
  └─ If ALL true → Proceed with Attack
```

**Player Paired Animation Flow:**
```cpp
void UCombatComponent::EnterPairedAnimationState()
{
    // Existing code...
    
    // NEW: Reserve all tokens to prevent interruption
    UCombatTokenSubsystem* TokenSys = GetWorld()->GetSubsystem<UCombatTokenSubsystem>();
    if (TokenSys)
    {
        TokenSys->LockTargetFromAttacks(GetOwner(), PairedAnimationDuration);
    }
}
```

## Ghost of Tsushima Reference

**How GoT Handles This:**
1. **Base Token Count**: 1 token during normal combat
2. **Stagger Attacks**: 0.5-1.0s delay between enemy attacks
3. **Off-Screen Priority**: On-screen enemies get token priority
4. **Stance Breaks**: During player vulnerability (stagger), token count increased
5. **Difficulty Scaling**: Hard mode increases tokens to 2-3

**KatanaCombat Adaptation:**
- **Easy**: 1 token, 2s delay
- **Normal**: 1-2 tokens, 1s delay
- **Hard**: 2-3 tokens, 0.5s delay
- **During Paired Anim**: 0 tokens (full protection)

## Implementation Plan

### Phase 1: Core Subsystem (Week 1)
- [ ] Create `UCombatTokenSubsystem` class
- [ ] Implement token request/release logic
- [ ] Add per-target token tracking
- [ ] Add configuration to `CombatSettings`

### Phase 2: AI Integration (Week 2)
- [ ] Create `BTTask_RequestAttackToken` 
- [ ] Create `BTDecorator_HasAttackToken`
- [ ] Update enemy AI behavior trees
- [ ] Test token acquisition flow

### Phase 3: Player Protection (Week 3)
- [ ] Hook into `EnterPairedAnimationState()`
- [ ] Hook into `ExitPairedAnimationState()`
- [ ] Lock tokens during finisher vulnerability
- [ ] Test multi-enemy scenarios

### Phase 4: Polish & Balance (Week 4)
- [ ] Add debug visualization (ShowDebug Combat)
- [ ] Tune token counts per difficulty
- [ ] Add priority system (finisher > heavy > light)
- [ ] Playtesting and iteration

## Related Gaps
- **Gap 1.2**: Interrupt finisher mechanic (relies on token system)
- **Gap 1.3**: AI awareness of paired state (uses token system queries)
- **Gap 1.4**: Execution prevention window (coordinated via tokens)
- **Gap 7.2**: Pre-sync invulnerability (protected by token lock)

## Combat Design Rationale

**Why Token System Matters:**
- **Player Skill Expression**: Fair windows to execute complex moves
- **Predictable Difficulty**: Player can learn enemy coordination patterns
- **Cinematic Feel**: Clean 1v1 moments during finishers
- **No Cheap Deaths**: Off-screen attacks prevented during vulnerability
- **Scalable Challenge**: Token count adjusts to difficulty

## Testing Strategy

### Unit Tests
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTokenBasicTest,
    "KatanaCombat.AI.AttackToken.BasicAllocation",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAttackTokenBasicTest::RunTest(const FString& Parameters)
{
    UWorld* World = CreateTestWorld();
    UCombatTokenSubsystem* TokenSys = World->GetSubsystem<UCombatTokenSubsystem>();
    
    AActor* Player = SpawnTestActor();
    AActor* Enemy1 = SpawnTestActor();
    AActor* Enemy2 = SpawnTestActor();
    
    // First enemy should get token
    bool Got1 = TokenSys->RequestAttackToken(Enemy1, Player, EAttackType::Light);
    TestTrue("Enemy1 acquires token", Got1);
    
    // Second enemy should be denied (max 1 token)
    bool Got2 = TokenSys->RequestAttackToken(Enemy2, Player, EAttackType::Light);
    TestFalse("Enemy2 denied token", Got2);
    
    // Release and retry
    TokenSys->ReleaseAttackToken(Enemy1);
    Got2 = TokenSys->RequestAttackToken(Enemy2, Player, EAttackType::Light);
    TestTrue("Enemy2 acquires after release", Got2);
    
    return true;
}
```

### Integration Tests
- Spawn 3 enemies around player
- Verify only N can attack simultaneously (N = token count)
- Enter finisher → verify all tokens locked
- Exit finisher → verify tokens released
- Test token priority (finisher > heavy > light)

## Acceptance Criteria
- [ ] UCombatTokenSubsystem implemented and tested
- [ ] AI successfully requests/releases tokens
- [ ] Player protected from attacks during paired animations
- [ ] Token count configurable per difficulty
- [ ] Debug visualization shows token allocation
- [ ] No performance impact (< 0.1ms per frame)
- [ ] Unit tests pass (>90% coverage)
- [ ] Integration tests pass
- [ ] Playtesting confirms fair combat feel
- [ ] Documentation complete
- [ ] Gap tracker updated to "Done"

## Documentation Links
- **Gap Tracker**: `docs/plans/gap-tracker.md` (Line 60)
- **Audit Report**: `docs/audits/AUDIT_SYNTHESIS_2026-02-03.md`
- **Architecture**: `docs/architecture/ARCHITECTURE.md` (Phase 5b-5)
- **Roadmap**: `docs/reference/ROADMAP.md` (AI Coordination Systems)
- **Combat Design**: `docs/guides/COMBAT_DESIGN_PRINCIPLES.md` (if exists)

## References
- Ghost of Tsushima GDC Talk: AI Combat Coordination
- Sekiro AI Behavior Analysis
- God of War (2018) Enemy AI Token System
