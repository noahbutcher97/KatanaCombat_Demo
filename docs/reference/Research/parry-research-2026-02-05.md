# Parry/Deflect System Research - Design Patterns Analysis

## Research Summary

This document synthesizes parry/deflect mechanics from 5 major action combat games to inform KatanaCombat's implementation.

---

## 1. GAME-BY-GAME ANALYSIS

### 1.1 Ghost of Tsushima

**Source**: [GameWith Guide](https://gamewith.net/ghost-of-tsushima/article/show/20267), [Screen Rant](https://screenrant.com/ghost-tsushima-perfect-parry-easy-guide/), [Twinfinite](https://twinfinite.net/guides/ghost-of-tsushima-block-parry-how/)

| Aspect | Implementation |
|--------|---------------|
| **Input** | L1 (same button for block and parry) |
| **Block** | Hold L1 - reduces damage, takes posture damage |
| **Parry** | Tap L1 just before attack lands |
| **Perfect Parry** | Tap L1 at the *last possible moment* |
| **Timing Window** | ~250-500ms before attack lands (varies by enemy/weapon) |
| **Feedback (Perfect)** | Slow motion activates, counterattack opportunity |
| **Feedback (Regular)** | Enemy briefly staggered, no slow-mo |
| **Upgrades** | Charm of Mizu-no-Kami widens parry window; Sarugami Armor extends perfect parry window |

**Key Design Insight**: Single button handles both block (hold) and parry (tap). Perfect parry is a tighter window within the regular parry window. Rewards are tiered - regular parry staggers, perfect parry enables counter kill.

---

### 1.2 Sekiro: Shadows Die Twice

**Source**: [Fextralife Wiki](https://sekiroshadowsdietwice.wiki.fextralife.com/Deflection), [Steam Discussions](https://steamcommunity.com/app/814380/discussions/0/3270186319532361490/), [FromSoftware Manual](https://www.fromsoftware.jp/manual/sekiroshadowsdietwice/stadia/mechanics.html)

| Aspect | Implementation |
|--------|---------------|
| **Input** | Guard button (tap for deflect, hold for block) |
| **Deflect Window** | ~12 frames (200ms) base window |
| **Window Decay** | Spamming shrinks window; recovers after 0.5s |
| **Block vs Deflect** | Block takes MORE posture damage; deflect takes minimal |
| **Posture System** | Deflecting builds ENEMY posture; blocking builds YOUR posture |
| **Critical Mechanic** | Deflecting cannot break your posture even if bar is full |
| **Visual Feedback** | Bright orange spark + higher-pitched metallic clang |
| **Block Feedback** | Weak orange spark + low-volume clang |

**Key Design Insight**: The "graceful failure" design - if you tap too early, you just block instead of getting hit. Deflect window decay punishes spam. The posture system creates a rhythm game feel where aggressive deflecting is rewarded.

---

### 1.3 Dark Souls / Elden Ring

**Source**: [Fextralife Wiki](https://eldenring.wiki.fextralife.com/Parrying), [Dark Souls Wiki](http://darksouls.wikidot.com/parry), [ResetEra Analysis](https://www.resetera.com/threads/why-is-it-that-parrying-has-always-felt-off-to-some-people-in-souls-elden-ring.739596/)

| Aspect | Implementation |
|--------|---------------|
| **Input** | Dedicated parry button (shield skill or weapon art) |
| **Parry Window** | ~15 frames (250ms) with Buckler, varies by tool |
| **Startup Frames** | ~10 frames (167ms) at 60fps before active frames |
| **Active Frames** | Middle of animation (NOT frame 1) |
| **Failed Parry** | "Partial parry" - takes 50% damage, no stagger |
| **Success** | Enemy enters "riposte state" for critical attack |
| **Riposte Window** | ~2 seconds to execute critical |
| **Tool Variation** | Small shields = larger window; Medium shields = smaller window |

**Key Design Insight**: Evolution from DS1 (instant parry frames) to Elden Ring (startup frames) creates prediction-based timing. The "partial parry" mechanic means early attempts aren't fully punished. Different parry tools = different risk/reward profiles.

---

### 1.4 For Honor

**Source**: [The Gamer Guide](https://www.thegamer.com/for-honor-how-to-parry-guide/), [Steam Discussions](https://steamcommunity.com/app/304390/discussions/0/3647273545691576235/), [Prima Games](https://primagames.com/tips/for-honor-how-parry)

| Aspect | Implementation |
|--------|---------------|
| **Input** | Heavy attack button in correct guard direction |
| **Timing Window** | 300ms before attack lands, but NOT last 100ms |
| **Universal Timing** | Same parry window for ALL attacks |
| **Directional** | Must match guard stance to attack direction |
| **Visual Indicator** | Red flash on attack indicator when parryable |
| **Light vs Heavy** | Light attacks = shorter window (faster attacks); Heavy = longer window |
| **Success Reward** | Guaranteed punish varies by character |

**Key Design Insight**: The "last 100ms dead zone" prevents reaction parries - must be predictive. Universal timing across all attacks simplifies learning. Visual indicator (red flash) provides explicit parry timing cue.

---

### 1.5 God of War (2018/Ragnarok)

**Source**: [Push Square](https://www.pushsquare.com/guides/god-of-war-ragnarok-how-to-parry), [Videogamer](https://www.videogamer.com/guides/god-of-war-ragnarok-how-to-parry/), [GameFAQs](https://gamefaqs.gamespot.com/boards/300963-god-of-war-ragnarok/80290164)

| Aspect | Implementation |
|--------|---------------|
| **Input** | L1 (tap for parry, hold for block) |
| **Timing Window** | ~500ms (generous by design) |
| **Attack Type Colors** | Yellow = Parryable, Red = Unblockable (dodge), Blue = Shield Strike |
| **Shield Variation** | Dauntless Shield = larger parry window; Other shields vary |
| **Success Reward** | Enemy staggered, damage opportunity |
| **Shield Strike** | Double-tap L1 for blue attacks (different from parry) |

**Key Design Insight**: Color-coded system removes ambiguity about WHAT can be parried. Multiple defensive options (parry/dodge/shield strike) create decision space. Shield choice affects playstyle (larger windows = easier parry but less damage).

---

## 2. CROSS-GAME PATTERNS

### 2.1 Parry Windows: Explicit vs Implicit

| Game | Approach | Notes |
|------|----------|-------|
| Ghost of Tsushima | **Implicit** | Window based on attack timing, not explicit notify |
| Sekiro | **Implicit** | Any time during attack startup, with decay |
| Dark Souls/ER | **Explicit** | Parry tool determines active frame range |
| For Honor | **Implicit** | 300ms before impact, universal |
| God of War | **Implicit** | Before attack lands, color indicates parryability |

**Conclusion**: Most games use implicit windows based on attack timing. KatanaCombat's current `AnimNotifyState_ParryWindow` on the ATTACKER's montage is the correct approach - it defines when the attacker is vulnerable to being parried.

### 2.2 Block vs Parry (Held vs Tapped Defense)

| Game | Block | Parry |
|------|-------|-------|
| Ghost of Tsushima | Hold L1 | Tap L1 |
| Sekiro | Hold Guard | Tap Guard |
| Dark Souls/ER | Hold shield | Dedicated parry button/skill |
| For Honor | Hold guard direction | Heavy attack in guard direction |
| God of War | Hold L1 | Tap L1 |

**Conclusion**: 4/5 games use the same button for block (hold) and parry (tap). This is the recommended approach for KatanaCombat.

### 2.3 Timing Windows (Approximate)

| Game | Window Size | Notes |
|------|-------------|-------|
| Ghost of Tsushima | 250-500ms | Perfect parry is tighter subset |
| Sekiro | 200ms (12 frames) | Decays with spam |
| Dark Souls/ER | 167-250ms | Varies by tool, has startup |
| For Honor | 200ms (300ms - 100ms deadzone) | Universal |
| God of War | ~500ms | Generous by design |

**Conclusion**: 200-300ms is the standard "core" parry window. 500ms is generous/accessible. Perfect parry windows are typically half of regular windows.

### 2.4 Visual/Audio Feedback

| Element | Common Implementation |
|---------|----------------------|
| **Parry Success** | Bright spark, high-pitched metallic clang, screen flash |
| **Perfect Parry** | Slow motion, unique sound, larger VFX |
| **Block** | Dimmer spark, lower sound, posture damage indicator |
| **Parryable Attack** | Color indicator (GoW), red flash (For Honor), attacker animation telegraph |

**Conclusion**: Distinct audio/visual feedback is critical for player learning. The sound difference between block and parry should be immediately recognizable.

### 2.5 Success Outcomes

| Game | Regular Parry | Perfect Parry |
|------|--------------|---------------|
| Ghost of Tsushima | Enemy stagger | Slow-mo + counterattack |
| Sekiro | Posture damage to enemy | Same (no perfect variant) |
| Dark Souls/ER | Riposte opportunity | N/A (binary success) |
| For Honor | Guaranteed punish | N/A |
| God of War | Enemy stagger | N/A |

**Conclusion**: Tiered parry outcomes (regular vs perfect) add depth. Ghost of Tsushima's model fits KatanaCombat best: regular parry staggers, perfect parry enables cinematic counter.

### 2.6 Failed Parry Penalties

| Game | Failed Parry Consequence |
|------|-------------------------|
| Ghost of Tsushima | Just blocks (graceful failure) |
| Sekiro | Just blocks (graceful failure) |
| Dark Souls/ER | Partial parry (50% damage) or full hit |
| For Honor | Gets hit normally |
| God of War | Just blocks (graceful failure) |

**Conclusion**: "Graceful failure" (early parry becomes block) is player-friendly and recommended. Only punish for being TOO late (getting hit).

---

## 3. RECOMMENDATIONS FOR KATANACOMBAT

### 3.1 Core Design Decisions

| Decision | Recommendation | Rationale |
|----------|---------------|-----------|
| **Input Model** | Single button (L1/LB) - Hold=Block, Tap=Parry | Matches 4/5 reference games, intuitive |
| **Window Location** | On ATTACKER's montage (current design) | Correct - matches Sekiro's deflect model |
| **Parry Window Size** | 300ms (18 frames at 60fps) | Middle ground, similar to For Honor |
| **Perfect Parry Window** | 100ms (6 frames) at end of parry window | Rewards precision like Ghost of Tsushima |
| **Failed Early** | Graceful failure to block | Player-friendly, encourages attempts |
| **Failed Late** | Takes full hit | Must have timing consequence |

### 3.2 Proposed Window Timing

```
Attack Montage Timeline:
[────────Windup────────][─────Active─────][───Recovery───]
    ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲
    │    Parry Window (300ms)
    │    [──────Regular──────][Perfect]
    │         200ms            100ms
    │
    └─ AnimNotifyState_ParryWindow starts here
```

### 3.3 Outcome Table

| Scenario | Defender Action | Result |
|----------|-----------------|--------|
| Attacker in Parry Window | Tap Block (late, within 100ms of window end) | **Perfect Parry** - Slow-mo, Counter Window opens |
| Attacker in Parry Window | Tap Block (early, within first 200ms) | **Regular Parry** - Enemy staggered, small opening |
| Attacker in Parry Window | Hold Block | **Block** - Takes posture damage, no opening |
| Attacker NOT in Parry Window | Tap Block | **Failed Parry** - Falls through to Block |
| Attacker NOT in Parry Window | Hold Block | **Block** - Takes posture damage |
| Attacker in Active Phase | Tap Block | **Block** - Attack lands, blocked normally |

### 3.4 Data Structure Additions

```cpp
// In AttackData.h or CombatTypes.h
USTRUCT(BlueprintType)
struct FParryConfig
{
    GENERATED_BODY()

    // Window timing (relative to attack montage)
    UPROPERTY(EditAnywhere, Category = "Parry")
    float ParryWindowDuration = 0.3f;  // Total parry window

    UPROPERTY(EditAnywhere, Category = "Parry")
    float PerfectParryWindowRatio = 0.33f;  // Last 33% is "perfect" window

    // Outcomes
    UPROPERTY(EditAnywhere, Category = "Parry|Regular")
    float RegularParryStaggerDuration = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Parry|Perfect")
    float PerfectParrySlowMoScale = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Parry|Perfect")
    float PerfectParrySlowMoDuration = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Parry|Perfect")
    bool bOpenCounterWindow = true;  // Enable counter-attack on perfect parry

    // Feedback
    UPROPERTY(EditAnywhere, Category = "Parry|Feedback")
    USoundBase* RegularParrySound;

    UPROPERTY(EditAnywhere, Category = "Parry|Feedback")
    USoundBase* PerfectParrySound;

    UPROPERTY(EditAnywhere, Category = "Parry|Feedback")
    UNiagaraSystem* ParrySparkVFX;

    UPROPERTY(EditAnywhere, Category = "Parry|Feedback")
    TSubclassOf<UCameraShakeBase> PerfectParryCameraShake;
};
```

### 3.5 State Flow

```
Player Presses Block Button
    │
    ▼
Is Button HELD or TAPPED?
    │
    ├─ HELD → Enter Blocking state
    │         └─ Takes posture damage on hit
    │
    └─ TAPPED → Check nearby attackers
                │
                ▼
           Any attacker in Parry Window?
                │
                ├─ NO → Graceful failure → Enter Blocking state
                │
                └─ YES → Where in Parry Window?
                         │
                         ├─ Early (first 66%) → Regular Parry
                         │   └─ Stagger attacker
                         │   └─ Play parry VFX/SFX
                         │   └─ Brief attack opportunity
                         │
                         └─ Late (last 33%) → Perfect Parry
                             └─ Stagger attacker (longer)
                             └─ Apply slow motion
                             └─ Open Counter Window
                             └─ Play enhanced VFX/SFX
                             └─ Camera shake
```

### 3.6 Feedback Design

| Event | Visual | Audio |
|-------|--------|-------|
| Regular Parry | Orange spark at weapon contact, brief screen flash | Metallic clang (normal pitch) |
| Perfect Parry | Bright white spark, slow-mo kicks in, radial blur | High-pitched ring + dramatic bass hit |
| Block | Subtle spark | Low thud |
| Parry Failure | (Nothing special - becomes block) | (Block sound) |

### 3.7 Anti-Spam Considerations

Following Sekiro's model:
1. Track last parry attempt timestamp
2. If attempting parry within 500ms of previous attempt, shrink window
3. Window recovers to full size after 1s without parry attempts
4. This prevents "parry spam" and encourages reading attacks

```cpp
// In CombatComponent
float LastParryAttemptTime = 0.0f;
float ParryWindowMultiplier = 1.0f;  // 1.0 = full window, 0.5 = half window

void OnParryAttempt()
{
    float TimeSinceLastAttempt = CurrentTime - LastParryAttemptTime;
    if (TimeSinceLastAttempt < 0.5f)
    {
        ParryWindowMultiplier = FMath::Max(0.5f, ParryWindowMultiplier - 0.2f);
    }
    LastParryAttemptTime = CurrentTime;
}

void TickParryRecovery(float DeltaTime)
{
    float TimeSinceLastAttempt = CurrentTime - LastParryAttemptTime;
    if (TimeSinceLastAttempt > 1.0f)
    {
        ParryWindowMultiplier = FMath::Min(1.0f, ParryWindowMultiplier + DeltaTime);
    }
}
```

---

## 4. INTEGRATION WITH EXISTING SYSTEMS

### 4.1 Current KatanaCombat Architecture

The existing system has scaffolding in place:
- `ECombatState::Parrying` - State enum exists
- `AnimNotifyState_ParryWindow` - Notify state exists (on attacker montage)
- `IsInParryWindow()` - Interface method exists
- `EActionWindowType::Parry` - Window type enum exists

### 4.2 Missing Pieces

| Component | Status | Work Required |
|-----------|--------|---------------|
| Perfect Parry detection | Missing | Add timing check within window |
| Parry outcome handling | Missing | Add `TryParry()` with outcomes |
| Block state machine | Exists | Wire parry check into block input |
| Counter system | Scaffolded | Enable after perfect parry |
| VFX/SFX feedback | Scaffolded | Wire into parry outcomes |
| Window decay | Missing | Add anti-spam system |

### 4.3 Parry -> Counter -> Finisher Flow

```
Perfect Parry Success
    │
    ▼
Counter Window Opens (1.0s)
    │
    └─ Player presses Attack?
        │
        ├─ NO → Counter window expires → Return to combat
        │
        └─ YES → Execute Counter Attack
                 │
                 └─ If counter is lethal OR target low health
                    │
                    └─ Trigger Finisher (existing finisher flow)
```

This connects to the existing `TryExecuteFinisher()` system.

---

## 5. SOURCES

### Ghost of Tsushima
- [GameWith Guide](https://gamewith.net/ghost-of-tsushima/article/show/20267)
- [Screen Rant Perfect Parry](https://screenrant.com/ghost-tsushima-perfect-parry-easy-guide/)
- [Twinfinite Block & Parry](https://twinfinite.net/guides/ghost-of-tsushima-block-parry-how/)

### Sekiro: Shadows Die Twice
- [Fextralife Deflection Wiki](https://sekiroshadowsdietwice.wiki.fextralife.com/Deflection)
- [Fextralife Posture Wiki](https://sekiroshadowsdietwice.wiki.fextralife.com/Posture)
- [FromSoftware Official Manual](https://www.fromsoftware.jp/manual/sekiroshadowsdietwice/stadia/mechanics.html)

### Dark Souls / Elden Ring
- [Elden Ring Parrying Wiki](https://eldenring.wiki.fextralife.com/Parrying)
- [Dark Souls Parry Wiki](http://darksouls.wikidot.com/parry)
- [ResetEra Parry Analysis](https://www.resetera.com/threads/why-is-it-that-parrying-has-always-felt-off-to-some-people-in-souls-elden-ring.739596/)

### For Honor
- [The Gamer Parry Guide](https://www.thegamer.com/for-honor-how-to-parry-guide/)
- [Prima Games Guide](https://primagames.com/tips/for-honor-how-parry)

### God of War
- [Push Square Parry Guide](https://www.pushsquare.com/guides/god-of-war-ragnarok-how-to-parry)
- [Videogamer Guide](https://www.videogamer.com/guides/god-of-war-ragnarok-how-to-parry/)

---

## 6. SUMMARY

**Key Takeaways for KatanaCombat:**

1. **Keep the current architecture** - ParryWindow on attacker montage is correct (Sekiro model)
2. **Add tiered parry outcomes** - Regular parry (stagger) vs Perfect parry (slow-mo + counter)
3. **Use single button** - Hold = Block, Tap = Parry
4. **Graceful failure** - Early parry attempt becomes block, not punishment
5. **300ms window** - Standard timing, with perfect parry in last 100ms
6. **Distinct feedback** - Different VFX/SFX for regular vs perfect parry
7. **Anti-spam decay** - Shrink window on rapid attempts (Sekiro model)
8. **Connect to counters** - Perfect parry opens counter window, counter can chain to finisher
