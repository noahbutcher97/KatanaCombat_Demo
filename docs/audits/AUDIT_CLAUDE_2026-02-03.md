# Comprehensive Combat System Audit

> **Date**: 2026-02-03
> **Scope**: Full codebase audit, gap verification, best practices research, documentation alignment, architecture health
> **Method**: 5 parallel audit agents with cross-referenced findings

---

## Executive Summary

| Metric | Value |
|--------|-------|
| **Architecture Health Score** | 6.5 / 10 |
| **Gap Tracker Accuracy** | 89% (28 confirmed, 5 with caveats, 3 partial, 1 inconsistency) |
| **New Gaps Discovered** | 14 |
| **Critical Code Issues (P0)** | 6 |
| **High Code Issues (P1)** | 7 |
| **Documentation Misalignments** | 21 (3 critical, 12 stale, 2 missing, 4 minor) |
| **TODO/FIXME Items** | 37 |
| **Deprecated Code Items** | 12 |
| **Total Source Lines (Core)** | ~25,000 |
| **Test Coverage (CombatComponent)** | 4.5% (178 test lines / 3,946 code lines) |

### Top 5 Highest-Priority Actions

1. **Fix P0 null dereferences** in `GetMesh()` chains (5 crash vectors in CombatComponent.cpp)
2. **Implement per-hit hitstop** -- single highest-impact game-feel improvement (low effort, massive impact)
3. **Wire BaseCombatCharacter interface stubs** to CombatComponent (AI/targeting broken without this)
4. **Decompose CombatComponent.cpp** (3,946 lines, 10+ functions over 50 lines)
5. **Rewrite API Reference** (entire CombatComponent section documents pre-refactor API)

---

## Part 1: Code Audit Findings

### P0 -- CRITICAL (Crash / Data Corruption)

| ID | Finding | File | Line | Gap Tracker |
|----|---------|------|------|-------------|
| P0-1 | Null deref: `GetMesh()` unchecked in `CancelPairedAnimation` | CombatComponent.cpp | 3728 | Gap 13.2 (Pending) |
| P0-2 | Null deref: `GetMesh()` unchecked in `OnMontageEnded` | CombatComponent.cpp | 2366 | Gap 13.2 (Pending) |
| P0-3 | Null deref: `GetWorld()` unchecked in `GetHoldDuration` | CombatComponent.cpp | 2835 | **NEW** |
| P0-4 | Null deref: `GetMesh()` unchecked in `BeginPlay` and `WeaponComponent` | CombatComponent.cpp, WeaponComponent.cpp | ~56, ~358 | Gap 13.2 (Pending) |
| P0-5 | Interface stubs return wrong data (always Idle/false/nullptr) | BaseCombatCharacter.cpp | 395-417 | **DEFERRED**: Parry window approach TBD (AnimNotifyState vs implicit phase vs Windup==ParryWindow). Wire GetCombatState/CanPerformAttack when AI work begins. |
| P0-6 | `const_cast<UCombatComponent*>(this)` in const function (potential UB) | CombatComponent.cpp | 3166-3172 | **NEW** |

### P1 -- HIGH (Logic Errors / State Issues)

| ID | Finding | File | Impact | Gap Tracker |
|----|---------|------|--------|-------------|
| P1-1 | Static variable in `DrawDebugInfo` shared across instances | CombatComponent.cpp:2957 | Debug contamination | **NEW** |
| P1-2 | External damage not blocked during paired animations | BaseCombatCharacter.cpp:296-310 | **CLARIFIED**: Block non-partner damage only. Attacker damage must pass through for sync point health tracking. | Gap 1.2/7.3 (Partial/Pending) |
| P1-3 | HitReactionComponent uses tick for stun instead of timer | HitReactionComponent.cpp | Performance | Gap 8.1 (Pending) |
| P1-4 | ~~ActionQueue reverse iteration = LIFO not FIFO~~ | CombatComponent.cpp:2373-2428 | **FALSE POSITIVE**: LIFO is intentional "last-input-wins" design for action games. Documentation incorrectly said "FIFO". | **RESOLVED** |
| P1-5 | `SetPhase(None)` skipped when montage ends during Active | CombatComponent.cpp:2467 | **CONFIRMED**: Normal flow self-corrects but abnormal interrupts (death, stun, paired anim) leave phase stuck. Add defensive reset. | Gap 13.5 (Pending) |
| P1-6 | Hitstop restore clobbers pre-existing time dilation | AnimNotifyState_PairedAnimationSync.cpp | **CONFIRMED**: Save/restore approach needed. Slow-mo counter windows overlap with hitstop. | Gap 18.13 (Pending) |
| P1-7 | Delegates declared in component headers (Rule 6 violation) | WeaponComponent.h:244, HitReactionComponent.h | **RULE UPDATED**: Two-tier approach — cross-component → CombatTypes.h, component-internal → stays in header. | **RESOLVED** |

### P2 -- MEDIUM (Edge Cases / Suboptimal)

| ID | Finding | File | Gap Tracker |
|----|---------|------|-------------|
| P2-1 | HitDirection REVERSED in `CompletePairedAnimation` vs `OnWeaponHitTarget` | CombatComponent.cpp:3813 | **INVESTIGATE**: Appears correct in-game. May be attacker-relative vs victim-relative naming. Owner recalls specific reason. |
| P2-2 | `PrimaryActorTick.bCanEverTick = true` with no Tick override | BaseCombatCharacter.cpp:16 | **LOW PRIORITY**: Add empty override as safeguard or disable after verifying Blueprint subclass dependency. |
| P2-3 | `FindPropertyByName` reflection (fragile) for CombatSettings access | WeaponComponent.cpp:~587 | **INVESTIGATE**: May be intentional for editor dropdown support. Also: paired finisher asset needs custom editor UI for montage section dropdowns (parity gap). |
| P2-4 | `const_cast<AActor*>` in TargetingComponent filter methods | TargetingComponent.cpp | **NEW** |
| P2-5 | ComboWindow timing values not cleared on attack end | CombatComponent.cpp | **INVESTIGATE**: Check if stale values are ever read after combo window flag closes. |
| P2-6 | Missing null check on `ActivePairedAnimData` in `TriggerSyncPointEffects` | CombatComponent.cpp:3620 | **NEW** |
| P2-7 | No validation that montage sections exist before `Montage_JumpToSection` | PairedAnimationData.h | Gap 3.3 (Pending) |

### P3 -- LOW (Cleanup)

| ID | Finding | Notes |
|----|---------|-------|
| P3-1 | 37 TODO/FIXME comments | 15 blocking, 7 design, 15 nice-to-have |
| P3-2 | 12 deprecated code items still compiled | 2 classes, 4 properties, 1 method (80 lines), 5 const_cast workarounds |
| P3-3 | `LastDirectionalInput` maintained for backward compatibility | Dead code |
| P3-4 | Internal state vars exposed as `BlueprintReadOnly` | Violates CLAUDE.md guideline |

---

## Part 2: Gap Verification Results

### Summary

Of **37 "Done" gaps** audited against source code:

| Result | Count | Percentage |
|--------|-------|------------|
| CONFIRMED | 28 | 76% |
| CONFIRMED WITH CAVEAT | 5 | 13% |
| PARTIAL | 3 | 8% |
| TRACKER INCONSISTENCY | 1 | 3% |

### Gaps Requiring Status Correction

| Gap ID | Current Status | Verified Status | Reason |
|--------|---------------|-----------------|--------|
| 11.1 | Section header "ALL DONE" but item "Pending" | Working As Intended | Damage-at-completion is valid design; header is misleading |
| 12.3 | Done | **PARTIAL** | No explicit root motion mode management; motion warping handles most cases |
| 2.4 | Done | Confirmed (vacuous) | Evade/Block handlers are empty stubs -- input IS blocked but the blocked actions do nothing |

### Strongest Confirmations (Exemplary Implementation)

- **Gap 1.5** (Stacked Finisher): Proper mutex via `bIsFinisherTarget` in `EnterPairedAnimationState()`
- **Gap 7.4** (Montage Failure): Complete rollback in `TryExecuteFinisher` with 6-step cleanup
- **Gap 17.5** (Time Dilation Stacking): Prevents faster slow-mo from overriding slower slow-mo
- **Gap 20.3** (Input Blocking): Triple safety net (EndPairedAnimation, OnCharacterDeath, EndPlay)
- **Gap 20.4** (Guard Flag): `bCompletingPairedAnimation` cleared in completion, cancellation, AND EndPlay
- **Gap 21.1** (Double Death): Lifecycle API with atomic flag management

### Phase 5c Math Libraries Verification

| Library | Claimed Functions | Actual Functions | Claimed Lines | Actual Lines |
|---------|-------------------|-----------------|---------------|-------------|
| SkeletalAnalysisLibrary | 18 | **21** | 814 | **814** |
| GeometryMathLibrary | 20 | **29** | 499 | **499** |
| SpatialQueryLibrary | 15 | **20** | 706 | **706** |
| PhysicsIntegrationLibrary | 15 | **16** | 610 | **610** |
| PairedAnimationUtilityLibrary | 15 | **18** | 499 | **499** |
| **TOTAL** | **83** | **104** | **3,128** | **3,128** |

Lines match exactly. Function counts EXCEED claims (104 actual vs 83 claimed).

---

## Part 3: Best Practices Research Findings

### Industry-Standard Hitstop Values (60fps)

| Attack Type | Frames | Duration | KatanaCombat Status |
|-------------|--------|----------|---------------------|
| Light attack | 3-5 | 50-83ms | **NONE** (combat-polish-plan proposes 40ms) |
| Heavy attack | 7-9 | 117-150ms | **NONE** (combat-polish-plan proposes 80ms) |
| Finisher sync | 10-15 | 167-250ms | Scaffolded (off by default) |
| Counter/Parry | 8-12 | 133-200ms | Not implemented |

**Recommendation**: Update combat-polish-plan.md light attack hitstop from 0.04s (40ms, ~2.4 frames) to **0.06-0.08s (60-80ms, 4-5 frames)** to match industry minimum. Heavy attack from 0.08s to **0.12-0.15s (7-9 frames)**.

### Critical Implementation Detail: CustomTimeDilation Value

**Do NOT set `CustomTimeDilation` to exactly 0.0f** -- this can cause division-by-zero in engine calculations. Use **0.0001f** instead. The current `AnimNotifyState_PairedAnimationSync` sets to 0.0f which is a latent crash risk.

### Timer Method Matrix

| Method | Affected by Dilation | Safe for Hitstop |
|--------|---------------------|------------------|
| `GetWorld()->GetTimeSeconds()` | YES | NO |
| `FPlatformTime::Seconds()` | NO | **YES** |
| `FTimerManager::SetTimer()` | YES | NO |
| `FTSTicker` | NO | **YES** |

KatanaCombat correctly uses `FPlatformTime::Seconds()` + `FTSTicker` for the sync notify hitstop. This pattern should be replicated for normal attack hitstop.

### Camera Distance Best Practices

| State | KatanaCombat Plan | Industry Standard | Assessment |
|-------|-------------------|-------------------|------------|
| Default | 100cm | 300-500cm | **Placeholder** -- will tune during polish |
| Combat | 200cm | 400-600cm | **Placeholder** -- will tune during polish |
| Finisher | 300cm | Variable (cinematic) | Reasonable |
| Death | 400cm | N/A | Fine |

**User-confirmed**: These are placeholder values, not a deliberate stylistic choice. Will be tuned during polish phase. Industry standards provide reference range for tuning.

### Impact Effects Priority Order (Industry Standard)

When weapon hits connect, fire effects in this order:
1. **Hitstop** (immediate, frame of hit)
2. **VFX** (during hitstop -- particles continue while characters freeze)
3. **Audio** (immediate, layered)
4. **Camera Shake** (after hitstop ends, not during)
5. **Screen Effects** (heavy hits only)
6. **Controller Rumble** (matches audio timing)

Key insight: **VFX should play DURING hitstop** while characters are frozen. Camera shake should play AFTER hitstop. This creates the best feel.

### Input During Hitstop (Critical Interaction)

**Best Practice**: Continue reading and buffering input during hitstop. In fighting games, hitstop IS the cancel window. Buffer timestamps must use real-time (`FPlatformTime::Seconds()`), not game time.

### Posture System Research (Sekiro Model)

Key insight from research: **Lower HP should mean slower posture recovery**. This creates strategic depth where players choose between health damage (slow but safe) or posture damage (fast but requires aggression).

| Parameter | Sekiro-like | GoT-like |
|-----------|-------------|----------|
| Light posture damage | 10-15% | 5-10% |
| Heavy posture damage | 20-30% | 15-25% |
| Perfect parry damage | 25-40% | N/A |
| Recovery (full HP) | ~5%/sec | Fast |
| Recovery (low HP) | ~1%/sec | Slow |

---

## Part 4: Documentation Alignment Findings

### CRITICAL Misalignments (3)

#### D-1: API Reference Massively Outdated
**File**: `docs/API_REFERENCE.md`

The entire CombatComponent section documents a pre-refactor API. **20+ documented functions do not exist:**
- `GetCombatState()`, `SetCombatState()`, `ExecuteAttack()`, `CanAttack()`, `StopCurrentAttack()`, `GetComboCount()`, `CanCombo()`, `ResetCombo()`, `OpenComboWindow()`, `CloseComboWindow()`, `GetCurrentPosture()`, `ApplyPostureDamage()`, `IsGuardBroken()`, `TriggerGuardBreak()`, `IsBlocking()`, `CanBlock()`, `StartBlocking()`, `StopBlocking()`, `TryParry()`, `SetMovementInput()`, individual attack input methods

Actual current API: `OnInputEvent()`, `QueueAction()`, `PlayAttackMontage()`, `TryExecuteFinisher()`, etc.

**6 documented delegates don't exist** as CombatComponent members.
**Entire paired animation system undocumented.**

#### D-2: CLAUDE.md Finisher Flow References Non-Existent Function
**File**: `CLAUDE.md` (Active Development section)

Flow diagram references `SetDeathHandledByPairedAnimation()` -- this function does not exist. The actual mechanism is `EnterPairedAnimationState()` which sets the flag internally.

#### D-3: Paired Animation Spec Missing Lifecycle API
**File**: `docs/specs/PAIRED_ANIMATION_SPEC.md`

Zero mention of `EnterPairedAnimationState()`/`ExitPairedAnimationState()` -- the most significant architectural addition to the paired animation system.

### STALE References (12)

| ID | Doc File | Issue |
|----|----------|-------|
| D-4 | Spec | `VictimRelativeOffset` in spec vs `VictimRelativePosition` in code |
| D-5 | Spec | `bVictimFacesAttacker` (bool) vs `VictimFacingMode` (int32 tri-state) |
| D-6 | Spec | `bApplyHitPause` attributed to PairedAnimationData (actually on PairedAnimationSync notify) |
| D-7 | Spec | Phase 5c marked "Pending" but is COMPLETE |
| D-8 | Spec | Editor tool file names don't match actual files |
| D-9 | Spec | `ApplyHitstop` referenced but doesn't exist (actual: `FreezeActors`/`RestoreActors`) |
| D-10 | CLAUDE.md | `TriggerCameraShake()` listed but doesn't exist (actual: `PlayCameraShakeOnActor()`) |
| D-11 | CLAUDE.md | Gap 3.3 listed as "Planned" but montage sections ARE implemented |
| D-12 | CLAUDE.md | Hitstop described as "not called in finisher flow" but IS wired in sync notify (defaults off) |
| D-13 | Combat-polish-plan | `bApplyHitPause` incorrectly attributed to PairedAnimationData |
| D-14 | Combat-polish-plan | `OnCombatStateChanged` delegate not a UPROPERTY on CombatComponent |
| D-15 | API Reference | TargetingComponent settings documented as UPROPERTY (moved to data asset) |

### MISSING Documentation (2)

| ID | Doc File | What's Missing |
|----|----------|----------------|
| D-16 | API Reference | Entire paired animation system (30+ functions across 4 components) |
| D-17 | CLAUDE.md | File structure lists ~25 of 53 actual headers |

---

## Part 5: Architecture Health Metrics

### File Size Distribution

| Severity | Files | Largest |
|----------|-------|---------|
| CRITICAL (>3000 lines) | 2 | CombatComponent.cpp (3,946), PairedAnimationPreview.cpp (5,746) |
| HIGH (>1000 lines) | 6 | TargetingComponent.cpp (1,525), MontageUtilityLibrary.cpp (1,419) |
| MODERATE (>500 lines) | 4 | CombatComponent.h (872), CombatTypes.h (804) |

### Complexity Hotspots (Functions > 50 lines)

| Function | Lines | Risk |
|----------|-------|------|
| `TryExecuteFinisher()` | ~335 | CRITICAL -- multiple responsibilities |
| `SetupAttackWarp()` | ~186 | 4-case direction resolution |
| `CompletePairedAnimation()` | ~181 | Damage calc + partner cleanup |
| `QueueAction()` | ~181 | Combo-aware queue management |
| `OnMontageEnded()` | ~165 | Queue processing + paired detection |
| `DeactivateHold()` | ~163 | Hold system with directional follow-ups |
| `OnEaseTimerTick()` | ~160 | Bidirectional easing |
| `GetAttackForInput()` | ~149 | Context-aware resolution |
| `OnInputEvent()` | ~138 | 4 levels of nesting |
| `DrawDebugInfo()` | ~136 | Pure rendering |

### Coupling Analysis

- **CombatComponent.cpp**: 23 includes, 11 `FindComponentByClass` calls (highest coupling)
- **CombatTypes.h**: Included by 22 files (highest fan-in -- changes ripple everywhere)
- **CombatComponent.h includes BaseCombatCharacter.h**: Inverted dependency (component depends on owner)

### Test Coverage

| Component | Test Lines | Code Lines | Ratio | Assessment |
|-----------|-----------|-----------|-------|------------|
| Paired Animations | 1,155 | ~1,200 | 96% | EXCELLENT |
| Weapon | 696 | 595 | 117% | GOOD |
| HitReaction | 648 | 1,193 | 54% | GOOD |
| Death System | 462 | ~200 | 231% | GOOD |
| Debug Viz | 627 | 1,057 | 59% | GOOD |
| **CombatComponent** | **178** | **3,946** | **4.5%** | **CRITICAL GAP** |
| Math Libraries | 0 | 2,629 | 0% | HIGH GAP |
| Editor Module | 0 | 11,500 | 0% | HIGH GAP |

### Deprecated Code Inventory

| Item | Location | Impact |
|------|----------|--------|
| `AnimNotifyState_AttackPhase` class | Animation/ | Entire class deprecated, still compiled |
| `AnimNotify_ToggleHitDetection` class | Animation/ | Entire class deprecated, still compiled |
| `ProcessQueue(float)` method | CombatComponent.cpp:858 | 80 lines of dead code |
| `LastDirectionalInput` property | CombatComponent.h:614 | Written in 2 places, deprecated |
| `bDirectionalInputConsumed` property | CombatComponent.h:626 | Written via const_cast, deprecated |
| `LightHitReactions` property | HitReactionComponent.h:89 | DeprecatedProperty meta |
| `HeavyHitReactions` property | HitReactionComponent.h:94 | DeprecatedProperty meta |
| `GuardBrokenAnimation` property | HitReactionComponent.h:99 | DeprecatedProperty meta |
| `FinisherVictimAnimations` property | HitReactionComponent.h:104 | DeprecatedProperty meta |
| `FHitReactionAnimSet` struct | CombatTypes.h:381 | Used by deprecated properties |

---

## Part 6: New Gaps Discovered

These gaps were found by the code audit and are NOT currently tracked in `gap-tracker.md`:

| New ID | Description | Priority | Source |
|--------|-------------|----------|--------|
| N-1 | `GetWorld()` null in `GetHoldDuration()` const query | P0 | Code Audit P0-3 |
| N-2 | ICombatInterface stubs return wrong data (always Idle) | ~~P0~~ **DEFERRED** | Code Audit P0-5 — Wire when AI work begins; parry window approach TBD |
| N-3 | `const_cast` UB in `GetAttackForInput()` | P1 | Code Audit P0-6 |
| N-4 | Static variable cross-instance contamination in debug | P2 | Code Audit P1-1 |
| N-5 | ~~ActionQueue LIFO iteration (should be FIFO)~~ **FALSE POSITIVE** — LIFO is intentional | ~~P1~~ | Code Audit P1-4 (RESOLVED) |
| N-6 | HitDirection reversed in finisher vs normal hit | P2 | Code Audit P2-1 |
| N-7 | Unnecessary tick enabled on BaseCombatCharacter | P3 | Code Audit P2-2 |
| N-8 | Fragile reflection-based CombatSettings access | P2 | Code Audit P2-3 |
| N-9 | const_cast in TargetingComponent filters | P3 | Code Audit P2-4 |
| N-10 | ComboWindow timing not cleared on attack end | P3 | Code Audit P2-5 |
| N-11 | `CustomTimeDilation` set to 0.0f (should be 0.0001f) | P1 | Best Practices Research |
| N-12 | 17 uncached `FindComponentByClass` calls | P2 | Architecture Health |
| N-13 | CombatComponent.h includes BaseCombatCharacter.h (inverted dependency) | P3 | Architecture Health |
| N-14 | Internal state vars exposed as BlueprintReadOnly | P3 | Architecture Health |

---

## Part 7: Consolidated Priority Matrix

### P0 -- Fix Immediately

| Action | Source | Effort | Impact |
|--------|--------|--------|--------|
| Add null checks on all `GetMesh()` chains (5 locations) | Code Audit | Low | Prevents crashes |
| Wire `BaseCombatCharacter` interface stubs to CombatComponent | Code Audit | Low | **DEFERRED** until AI work begins; parry window approach TBD |
| Change `CustomTimeDilation = 0.0f` to `0.0001f` in sync notify | Research | Trivial | Prevents division-by-zero |

### P1 -- Next Sprint

| Action | Source | Effort | Impact |
|--------|--------|--------|--------|
| Implement per-hit hitstop (60-150ms, CustomTimeDilation) | Research | Low | **MASSIVE game-feel improvement** |
| ~~Fix ActionQueue LIFO -> FIFO iteration~~ | ~~Code Audit~~ | ~~Low~~ | **REMOVED** — LIFO is intentional "last-input-wins" design |
| Fix phase stuck on Active after montage interrupt | Code Audit | Low | Prevents permanent Active state |
| Save/restore `CustomTimeDilation` in hitstop (don't clobber) | Code Audit | Low | **CONFIRMED**: Slow-mo counter windows overlap with hitstop |
| Make `GetAttackForInput` non-const (remove const_cast) | Code Audit | Low | Eliminates potential UB |
| Wire hit audio (PlaySoundAtLocation at impact point) | Research | Low-Med | Major game-feel improvement |
| Wire hit VFX (Niagara burst at impact point) | Research | Medium | Major game-feel improvement |
| ~~Move ALL delegates to CombatTypes.h~~ → Adopt two-tier rule | Code Audit | Low | **RULE UPDATED**: Cross-component → CombatTypes.h, component-internal → stays in header |

### P2 -- Quality

| Action | Source | Effort | Impact |
|--------|--------|--------|--------|
| Decompose CombatComponent.cpp (3,946 lines) | Health Audit | High | Maintainability, testability |
| Add tests for CombatComponent core logic | Health Audit | Medium | Regression prevention |
| Fix reversed HitDirection in finisher damage | Code Audit | Low | Correct death animations |
| Cache `FindComponentByClass` lookups | Health Audit | Low | Performance in hot paths |
| Implement camera combat modifiers | Research | Medium | Camera quality |
| Remove 12 deprecated code items | Health Audit | Low | ~300 lines dead code removal |
| Add tests for math utility libraries (2,629 lines, 0 tests) | Health Audit | Medium | Pure math = ideal for testing |

### P3 -- Backlog

| Action | Source | Effort | Impact |
|--------|--------|--------|--------|
| Fix static variable in DrawDebugInfo | Code Audit | Trivial | Debug correctness |
| Remove header coupling (CombatComponent.h -> BaseCombatCharacter.h) | Health Audit | Medium | Compile times |
| Reduce internal Blueprint exposure | Health Audit | Low | API clarity |
| Standardize logging categories | Health Audit | Trivial | Consistency |
| Disable tick on BaseCombatCharacter | Code Audit | Trivial | Minor performance |

### Documentation Priority

| Action | Severity | Effort |
|--------|----------|--------|
| Rewrite API Reference CombatComponent section | CRITICAL | High |
| Fix CLAUDE.md finisher flow diagram | CRITICAL | Low |
| Add lifecycle API to paired animation spec | CRITICAL | Medium |
| Update spec field names to match code | STALE | Medium |
| Update CLAUDE.md "Planned" section (Gap 3.3 is done) | STALE | Low |
| Add paired animation system to API Reference | MISSING | High |

---

## Part 8: Best Practices Comparison

### Where KatanaCombat Aligns with Industry

| Practice | Status | Notes |
|----------|--------|-------|
| 4-component architecture | Aligned | Component-based is preferred over GAS for focused melee combat |
| Last-input-wins (LIFO) buffering | Aligned | Input always captured, most recent input takes priority (action game standard), window controls execution timing |
| Phase/Window distinction | Aligned | Phases exclusive, windows overlap -- industry standard |
| Checkpoint AnimNotify for phases (implicit inference), AnimNotifyState for windows | Aligned | Phases inferred from `AnimNotify_AttackPhaseTransition` checkpoints; windows (Combo, Hold, Parry) use AnimNotifyStates |
| Two-stage death (Dying -> Dead) | Aligned | Industry standard |
| `FPlatformTime::Seconds()` for hitstop timers | Aligned | Correct real-time tracking |
| Paired animation with motion warping | Aligned | Standard AAA approach |
| `bDeathHandledByPairedAnimation` flag | Aligned | Standard for finisher death handling |
| Event-driven over tick-based | Mostly aligned | HitReaction stun still uses tick |

### Where KatanaCombat Deviates from Industry

| Gap | Industry Standard | KatanaCombat Status |
|-----|-------------------|---------------------|
| Per-hit hitstop | 3-5 frames light, 7-9 heavy | **Zero** on normal attacks |
| Hit audio | Impact sound at hit point, 5+ variations | **Zero** audio on normal hits |
| Hit VFX | Particle burst at contact point | **Zero** VFX on normal hits |
| Camera shake on hit | Scaled to attack weight | **Zero** on normal attacks |
| Input during hitstop | Buffer continues on real-time | Unknown (needs verification) |
| Camera combat modifiers | Distance/DOF changes per combat state | **None** |
| Posture visual feedback | Bar with color-coded state | **None** |
| Music ducking | -6 to -9 dB during finishers | **None** |
| Controller haptics | Per-attack rumble patterns | **None** |

---

## Appendix A: Research Sources

### Hitstop
- SmashWiki: Hitlag
- Sakurai's Famitsu Column on Hitstop (Vol. 490)
- Shane Sicienski: Hitstop in Capcom Beat Em Ups
- Ahmad's Portfolio: A More Realistic HitStop
- Epic Dev Community: Simple Hitstop Implementation
- Cobra Code: Hitstop in Unreal Engine (Medium)

### Camera
- Daedalic: Six Ingredients for a Dynamic Third Person Camera (Unreal Fest Europe 2018)
- Game AI Pro: 47 Tips for Third Person Camera
- GDC Vault: Master of the Katana (Sucker Punch)

### Impact Effects
- Epic Tech Blog: Optimizing Blood and VFX in UE5 (Tuatara Games)
- CGHOW: Hit Impact FX Niagara Tutorial
- Wayline: Responsible Controller Vibration

### Paired Animations
- Devtricks: Contextual Animation Plugin Guide (UE 5.3+)
- Epic Docs: Motion Warping
- The Rookies: Freeflow Arena (2025)

### Input Buffering
- Wayline: The Art of Input Buffering
- Wayline: Mastering Input Buffering
- SmashWiki: Buffer

### Posture Systems
- GDC Vault: Honoring the Blade (Sucker Punch, Ghost of Tsushima)
- What's in a Game: Sekiro's Genius Posture Mechanic

### Death Systems
- Epic Dev Community: Ragdolling and How to Recover
- Epic Dev Community: Advanced Seamless Ragdoll Enter/Exit

### UE5 Architecture
- Devtricks: The Truth of GAS
- GAS Documentation (GitHub)
- Epic Dev Community: Combat System Tutorial

---

## Appendix B: Agent Execution Metrics

| Agent | Focus | Duration | Tokens | Tool Uses | Files Read |
|-------|-------|----------|--------|-----------|------------|
| Code Auditor | Full codebase review | ~7 min | 132,040 | 51 | 30+ |
| Gap Verification | Verify "Done" gaps | ~6 min | 150,033 | 53 | 25+ |
| Best Practices Research | External research | ~11 min | 112,909 | 33 | 10+ web sources |
| Documentation Alignment | Docs vs code | ~7 min | 132,915 | 57 | 40+ |
| Architecture Health | Code metrics | ~6 min | 157,216 | 49 | 40+ |
| **TOTAL** | | **~37 min** | **685,113** | **243** | |
