# Hit State System Audit
**Date**: 2026-02-05
**Purpose**: Define terminology and mechanics for stun, stagger, hitstun, and finisher vulnerability

---

## Executive Summary

This audit researches industry best practices for hit states and maps them against the current KatanaCombat implementation. The goal is to establish clear definitions before implementing the counter/parry system.

**Key Finding**: The current system conflates multiple concepts under "stun" which leads to confusion and accidental finisher triggers.

---

## Design Philosophy (User-Defined Constraints)

**THIS IS NOT A FIGHTING GAME**. KatanaCombat is a single-player experience with player-first emphasis.

### Core Principles

1. **Generous Timing Windows** - Not frame-perfect reactions. Players should succeed when they "feel" the timing is right.

2. **Natural Flow** - Combat should feel rhythmic and satisfying, not punishing or demanding.

3. **Backend Perfection** - The system creates the *feeling* of perfect execution through:
   - Procedural animation selection
   - Contextual responses based on combat situation
   - Intelligent animation blending and timing adjustment

4. **Player Fantasy** - The goal is to make the player feel like a skilled samurai, not to test their reaction time.

### Design Anti-Patterns (AVOID)

- ❌ Frame-perfect timing requirements
- ❌ Punishing missed timing windows
- ❌ Rigid state machines that feel mechanical
- ❌ Visible "you failed" feedback
- ❌ **Multiple stat bars to manage** (NO Sekiro-style health + posture meters)
- ❌ **Whittling away at enemies** - this is not an attrition game
- ❌ **Meter management gameplay** - player shouldn't watch UI, should watch enemies

### Design Patterns (EMBRACE)

- ✅ Forgiving input windows that feel responsive
- ✅ Multiple valid responses to the same situation
- ✅ Animation variety through contextual selection
- ✅ Smooth transitions that hide imperfect player input
- ✅ **Player dominance** - the player is the predator, exerting will on the arena
- ✅ **Decisive action** - counter = instant kill (on basic enemies), not "deal posture damage"
- ✅ **Visual enemy states** - read enemy animations, not UI meters
- ✅ **Arkham/AC3 philosophy** - timing a counter IS the skill, not resource management

---

## Part 1: Industry Reference Research

### 1.1 Batman Arkham Series (Primary Influence)

**Freeflow Combat Philosophy**: Rhythm-based combat where player maintains flow state through continuous attacks and counters.

| Mechanic | Implementation |
|----------|----------------|
| **Counter Window** | Blue lightning icon above enemy head during attack windup |
| **Counter Result** | Varies by enemy type - instant takedown (regular), stun only (armored), or blocked |
| **Cape Stun** | Dedicated stun move that temporarily confuses enemy, enabling Beatdown combo |
| **Beatdown** | Rapid attack combo after Cape Stun, ends with instant takedown |
| **Combo Multiplier** | Builds with successful hits, unlocks Special Combo Moves at threshold |
| **Special Combo Takedown** | Instant one-hit KO available when combo meter glows (x5+) |

**Key Design Principle**: Counter is reactive (enemy attacks → player counters). Cape Stun is proactive (player initiates stun). Both lead to takedowns but through different flows.

**Sources**:
- [Arkham Wiki: Stun](https://arkhamcity.fandom.com/wiki/Stun)
- [Arkham Wiki: Special Combo Moves](https://arkhamcity.fandom.com/wiki/Special_Combo_Moves)
- [StrategyWiki: Arkham Combat](https://strategywiki.org/wiki/Batman:_Arkham_Asylum/Combat)

### 1.2 Assassin's Creed 3/4 (Primary Influence)

**Counter-Kill Philosophy**: Defensive timing leads to offensive spectacle. One button, one kill (on basic enemies).

| Mechanic | Implementation |
|----------|----------------|
| **Counter Window** | Red triangle icon appears above enemy head when they attack |
| **Counter Timing** | Hold counter button when triangle appears → time slows → choose response |
| **Counter Options** | Attack (counter-kill), Disarm, Throw, or Tool use |
| **Kill Streak** | After first kill, chain executions by highlighting next enemy + attack |
| **Streak Rules** | Counter kills don't START streaks but don't BREAK them either |
| **Enemy Types** | Some require specific counter responses (officers need disarm first) |

**Key Design Principle**: The red triangle is a clear "react now" signal. Kill streaks reward momentum and create cinematic flow.

**Sources**:
- [GamePressure: AC3 Combat Guide](https://guides.gamepressure.com/assassinscreediii/guide.asp?ID=16888)
- [GamerGuides: Counter Kills and Executions](https://www.gamerguides.com/assassins-creed-revelations/guide/general-tips-and-tricks/combat/counter-kills-and-executions)

### 1.3 Sekiro: Shadows Die Twice

**Posture System**: Dual-meter design where posture break enables Deathblow (finisher).

| Mechanic | Implementation |
|----------|----------------|
| **Vitality** | Health bar - traditional damage reduces this |
| **Posture** | Defense meter - fills from attacks AND deflections |
| **Posture Break** | When posture maxes out → enemy staggers → Deathblow available |
| **Recovery** | Posture recovers passively (slower at low health) |
| **Deflect** | Timed block that damages ENEMY posture while protecting yours |
| **Deathblow** | Finisher that removes one health pip (bosses have multiple pips) |

**Key Design Principle**: Player chooses strategy - whittle health (safer, slower) or break posture (riskier, faster). Deflection is rewarded as the "intended" playstyle.

**Sources**:
- [GameWith: Sekiro Posture System](https://gamewith.net/sekiro/article/show/8483)
- [What's in a Game: Sekiro's Genius Posture Mechanic](http://whats-in-a-game.com/sekiros-genius-posture-mechanic/)

### 1.4 Ghost of Tsushima

**Stance-Based Stagger**: Different stances are effective against different enemy types.

| Mechanic | Implementation |
|----------|----------------|
| **Stagger Meter** | White meter above enemy head |
| **Stance Advantage** | Stone vs Swordsmen, Water vs Shields, Wind vs Spears, Moon vs Brutes |
| **Stagger Effect** | When meter depletes, enemy stumbles - open for free hits |
| **Perfect Parry** | Timed block creates larger stagger opening |
| **Standoff** | Face-off mechanic - timing-based instant kill at battle start |

**Key Design Principle**: Combat rewards reading enemy types and switching stances. Stagger is earned through correct approach.

**Sources**:
- [Ghost Franchise Wiki: Combat](https://ghostfranchise.fandom.com/wiki/Combat)

### 1.5 God of War Ragnarok

**Stun Meter System**: Accumulated stun leads to finisher opportunity.

| Mechanic | Implementation |
|----------|----------------|
| **Stun Meter** | Builds as enemy takes damage and status effects |
| **Stun State** | When full, enemy is frozen - R3 prompt for Stun Grab |
| **Stun Grab** | Finisher that instantly kills or heavily damages enemy |
| **Status Effects** | Permafrost/Immolation accelerate stun buildup |

**Key Design Principle**: Stun is a resource that accumulates - not a binary state from a single hit.

**Sources**:
- [The Nerd Stash: How to Stun Enemies](https://thenerdstash.com/how-to-stun-enemies-in-god-of-war-ragnarok/)

### 1.6 Fighting Game Frame Data (Technical Reference)

**Hitstun**: The industry-standard term for "can't act after being hit".

| Term | Definition |
|------|------------|
| **Hitstun** | Frames during which character cannot act after being hit |
| **Blockstun** | Frames during which character cannot act after blocking |
| **Recovery** | Frames after attack completes before character can act again |
| **Frame Advantage** | Attacker's recovery vs defender's hitstun determines who acts first |

**Key Design Principle**: Combos exist because follow-up attacks connect within hitstun window.

**Sources**:
- [SmashWiki: Hitstun](https://www.ssbwiki.com/Hitstun)
- [CritPoints: Understanding Framedata](https://critpoints.net/2016/11/29/understanding-framedata-combos-traps-and-turns/)

---

## Part 2: Terminology Definitions for KatanaCombat

Based on research, we should distinguish these concepts:

### 2.1 Proposed Terminology

| Term | Duration | Effect | Triggers Finisher? |
|------|----------|--------|-------------------|
| **Hitstun** | 0.1-0.3s | Cannot act, plays hit reaction animation | No |
| **Stagger** | 0.3-0.8s | Stumbling animation, reduced defense, can be hit freely | No (unless accumulated) |
| **Stun** | 1.0s+ | Completely incapacitated, finisher prompt available | **Yes** |
| **Guard Break** | Until recovery | Posture depleted, special animation, finisher-eligible | **Yes** |

### 2.2 State Relationships

```
Normal Combat
    │
    ├─► Hit ─► Hitstun (brief) ─► Normal (recovered)
    │
    ├─► Heavy Hit ─► Stagger (longer) ─► Normal (recovered)
    │
    ├─► Counter Hit ─► Stun (vulnerable) ─► Finisher OR Recovery
    │
    └─► Posture Depleted ─► Guard Break (vulnerable) ─► Finisher OR Recovery
```

### 2.3 Finisher Eligibility Criteria

A target should be eligible for finisher execution when ANY of:
1. **Guard Break** - Posture meter depleted (Sekiro-style)
2. **Stun State** - From counter hits, guard break, or special attacks
3. **Low Health** - Below threshold (e.g., 25%) as a "mercy kill" option

NOT eligible just from:
- Normal hitstun (too brief, too common)
- Stagger state (unless it's a special "deep stagger")

---

## Part 3: Current KatanaCombat Implementation Analysis

### 3.1 Stun System (Current)

**Location**: `HitReactionComponent.h/.cpp`

```cpp
// State
bool bIsStunned = false;
float StunTimeRemaining = 0.0f;

// API
void ApplyHitStun(float Duration);
bool IsStunned() const { return bIsStunned; }
float GetRemainingStunTime() const { return StunTimeRemaining; }
```

**Issues Found**:
1. ❌ `FHitReactionEntry::StunDuration` defaulted to 0.3f - every hit was stunning (FIXED)
2. ❌ Single `bIsStunned` flag conflates hitstun and finisher-eligible stun
3. ❌ No stagger state exists - characters are either stunned or not
4. ❌ No stun accumulation - it's binary (stunned or not)

### 3.2 Posture/Guard System (Current)

**Location**: `DamageableInterface.h`, `BaseCombatCharacter.h/.cpp`

```cpp
// Interface contract
bool ApplyPostureDamage(float PostureDamage, AActor* Attacker);
bool IsGuardBroken() const;
float GetCurrentPosture() const;
float GetMaxPosture() const;
```

**Issues Found**:
1. ⚠️ Posture system exists but is marked "[NOT YET IMPLEMENTED]" in AttackData
2. ⚠️ Guard break triggers finisher via `IsVulnerableToFinisher()` but blocking isn't implemented
3. ✅ Interface is well-designed and ready for use

### 3.3 Finisher Vulnerability (Current)

**Location**: `HitReactionComponent.cpp` lines 1071-1120

```cpp
bool UHitReactionComponent::IsVulnerableToFinisher() const
{
    if (bIsFinisherTarget) return false;  // Already targeted
    return GetFinisherTriggerReason() != EFinisherTriggerReason::None;
}

EFinisherTriggerReason UHitReactionComponent::GetFinisherTriggerReason() const
{
    // Priority 1: Guard Broken (posture depleted)
    if (IDamageableInterface::Execute_IsGuardBroken(CombatChar))
        return EFinisherTriggerReason::GuardBroken;

    // Priority 2: Stunned
    if (IsStunned())
        return EFinisherTriggerReason::Stunned;

    // Priority 3: Low Health (25% threshold)
    if (HealthPercent <= 0.25f)
        return EFinisherTriggerReason::LowHealth;

    return EFinisherTriggerReason::None;
}
```

**Issues Found**:
1. ✅ Priority order is correct (Guard > Stun > Health)
2. ❌ `IsStunned()` check is too broad - any stun triggers finisher
3. ❌ No revocation window - once vulnerable, stays vulnerable until finisher or recovery
4. ⚠️ No "deep stagger" or accumulated stun concept

### 3.4 Test Coverage

**Location**: `HitReactionTests.cpp`

Tests exist for:
- ✅ Default stun state (not stunned)
- ✅ ApplyHitStun sets stun state
- ✅ Zero/negative stun rejected
- ✅ Stun time remaining tracking

Tests missing for:
- ❌ Stagger state (doesn't exist yet)
- ❌ Stun accumulation (doesn't exist yet)
- ❌ Finisher vulnerability revocation window

---

## Part 4: Gap Analysis (AC3/Arkham Philosophy)

### 4.1 Critical Gaps

| Gap | AC3/Arkham Standard | Current State | Priority |
|-----|---------------------|---------------|----------|
| **G-1** | Hitstun ≠ Finisher-Eligible Stun | Single `bIsStunned` for both | P0 |
| **G-2** | Counter Window Detection | No counter window on enemy attacks | P0 |
| **G-3** | Counter → Instant Kill | Counter checks `IsVulnerableToFinisher()` | P1 |
| **G-4** | Visual Counter Indicator | No enemy "about to attack" indicator | P1 |

### 4.2 Architecture Gaps

| Gap | Description | Files Affected |
|-----|-------------|----------------|
| **G-5** | No AnimNotifyState_CounterWindow | Animation/ |
| **G-6** | No counter input detection during enemy attack | CombatComponent |
| **G-7** | No contextual counter animation selection | CombatComponent |
| **G-8** | Finisher triggers on ANY stun (too easy) | HitReactionComponent |

---

## Part 5: Recommended System (AC3/Arkham Style)

### 5.1 Philosophy: Counter-Based, Not Meter-Based

**REJECT Sekiro model**: No dual health+posture meters. No "whittling down" stat bars.

**EMBRACE AC3/Arkham model**:
- Enemy attacks → Counter Window opens → Player counters → **Instant Kill**
- The skill IS the timing, not resource management
- Player dominates the arena through decisive action

### 5.2 Simple State Model

```cpp
// Enemy states (NOT player states)
UENUM(BlueprintType)
enum class EEnemyCombatState : uint8
{
    Normal,         // Can attack, can be hit
    Attacking,      // In attack animation, COUNTER WINDOW ACTIVE
    InHitstun,      // Just got hit, brief recovery animation
    BeingFinished,  // In finisher victim animation
    Dead
};
```

### 5.3 Counter Flow (AC3/Arkham)

```
Enemy in Normal
    │
    │ [Enemy decides to attack]
    ▼
Enemy in Attacking ──────────────────────────────────────►
    │                                                     │
    │ [Player presses Counter                            │ [Player doesn't counter]
    │  during Counter Window]                            │
    ▼                                                    ▼
COUNTER SUCCESS                                    Attack Lands
    │                                              (Player takes damage)
    │
    ├─► [Basic Enemy] ──► INSTANT KILL (counter-kill animation)
    │
    └─► [Elite Enemy] ──► Stagger + Follow-up Window
                              │
                              └─► Player attacks ──► Kill OR Damage
```

### 5.4 Finisher Eligibility (Simplified)

**Remove stun-based finisher trigger**. Finishers available when:

1. **Counter Success** - Player countered during enemy attack window
2. **Low Health** - Below 25% threshold (mercy kill)
3. **Special Conditions** - Scripted moments, boss phases, etc.

**NOT** from:
- ❌ Hitstun from normal hits (too common)
- ❌ Posture meters (rejected design)
- ❌ Stun accumulation (rejected design)

---

## Part 6: Recommendations (AC3/Arkham Aligned)

### 6.1 Immediate Actions (P0)

1. **Separate Hitstun from Finisher Vulnerability**
   - `bIsInHitstun` = brief animation lock after hit, NO finisher
   - `bIsCountered` = player successfully countered this enemy's attack, finisher eligible
   - Remove `IsStunned()` from `IsVulnerableToFinisher()` conditions

2. **Implement Counter Window on Enemy Attacks**
   - Add `AnimNotifyState_CounterWindow` to enemy attack montages
   - When active, player's counter input triggers counter-kill flow

### 6.2 Short-Term Actions (P1)

3. **Counter Input Detection**
   - During enemy's Counter Window, if player presses Counter → success
   - Generous window (0.4-0.6 seconds, not frame-perfect)
   - Visual indicator (red glow, icon) to signal window

4. **Contextual Counter Animation Selection**
   - Based on enemy attack type (overhead, sweep, thrust)
   - Based on relative positioning
   - System selects appropriate counter-kill animation automatically

### 6.3 What NOT To Implement

- ❌ Posture meters / guard meters
- ❌ Stun accumulation systems
- ❌ Multiple stat bars for enemies
- ❌ "Whittle down" gameplay loops
- ❌ Frame-perfect timing requirements

---

## Appendix A: Reference Links

### Batman Arkham
- https://arkhamcity.fandom.com/wiki/Stun
- https://arkhamcity.fandom.com/wiki/Special_Combo_Moves
- https://strategywiki.org/wiki/Batman:_Arkham_Asylum/Combat

### Assassin's Creed
- https://guides.gamepressure.com/assassinscreediii/guide.asp?ID=16888
- https://www.gamerguides.com/assassins-creed-revelations/guide/general-tips-and-tricks/combat/counter-kills-and-executions

### Sekiro
- https://gamewith.net/sekiro/article/show/8483
- http://whats-in-a-game.com/sekiros-genius-posture-mechanic/

### Ghost of Tsushima
- https://ghostfranchise.fandom.com/wiki/Combat

### God of War
- https://thenerdstash.com/how-to-stun-enemies-in-god-of-war-ragnarok/

### Fighting Games
- https://www.ssbwiki.com/Hitstun
- https://critpoints.net/2016/11/29/understanding-framedata-combos-traps-and-turns/

---

## Appendix B: Current Code Locations

| Concept | File | Line |
|---------|------|------|
| bIsStunned | HitReactionComponent.h | 434 |
| ApplyHitStun | HitReactionComponent.cpp | 285 |
| IsVulnerableToFinisher | HitReactionComponent.cpp | 1071 |
| GetFinisherTriggerReason | HitReactionComponent.cpp | 1083 |
| EFinisherTriggerReason | CombatTypes.h | 174 |
| FHitReactionEntry | CombatTypes.h | ~550 |
| StunDuration default | CombatTypes.h | 600 (NOW 0.0f) |
| IDamageableInterface | DamageableInterface.h | 23 |
