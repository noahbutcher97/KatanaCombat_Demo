# Paired Animation System - Gap Tracker

> **Extracted From**: gap-mitigation-plan.md (2026-02-03)
> **Purpose**: Reference document tracking all identified gaps and their status
> **Last Updated**: 2026-02-16 (Hit detection overhaul: blade segmentation, adaptive substeps, surface FX, audit fixes)
> **Audit Reference**: `docs/audits/COMPREHENSIVE_AUDIT_2026-02-03.md`

---

## Gap Coverage Summary

| Category | Total | Done | Pending | Deferred/Other |
|----------|-------|------|---------|----------------|
| 1. AI Coordination | 5 | 2 | 3 | 0 |
| 2. Input Handling | 5 | 2 | 3 | 0 |
| 3. Animation/Timing | 5 | 1 | 4 | 0 |
| 4. Audio Sync | 4 | 0 | 4 | 0 |
| 5. UI/HUD | 5 | 0 | 5 | 0 |
| 6. Environment | 4 | 0 | 2 | 2 |
| 7. State Transitions | 5 | 3 | 2 | 0 |
| 8. Performance | 3 | 0 | 2 | 1 |
| 9. Recovery/Cleanup | 4 | 0 | 4 | 0 |
| 10. Extensibility | 5 | 0 | 2 | 3 |
| 11. Delegate Wiring | 4 | 4 | 0 | 0 |
| 12. Animation Instance | 3 | 1 | 1 | 1 |
| 13. Bug Prevention | 5 | 0 | 5 | 0 |
| 14. Polish | 4 | 0 | 3 | 1 |
| 15. VFX Scaffolding | 6 | 1 | 1 | 4 |
| 16. Implementation | 5 | 2 | 2 | 1 |
| 17. Edge Cases | 5 | 1 | 4 | 0 |
| 18. Phase 5b-4 Analysis | 20 | 5 | 5 | 1 + 7 consolidated→19 |
| 19. Gap Audit | 14 | 5 | 9 | 0 |
| 20. Testing Session | 9 | 5 | 4 | 0 |
| 21. Death Animation | 1 | 1 | 0 | 0 |
| PT. Preview Tool | 9 | 9 | 0 | 0 |
| Phase 5c Math | 5 | 5 | 0 | 0 |
| 22. Audit Findings | 14 | 5 | 7 | 2 (deferred/closed) |
| 23. Camera & Collision | 4 | 2 | 2 | 0 |
| 24. Hit Detection | 7 | 7 | 0 | 0 |
| 25. Input Resolution | 8 | 5 | 3 | 0 |
| 26. Core Combat Flow | 9 | 1 | 3 | 5 (partial) |
| **TOTAL** | **177** | **77** | **~80** | **~20** |

**Notes**:
- Gap 1.2 (invulnerability during paired) partially addressed by `bReactionsSuppressed` in lifecycle API
- Gap 21.1 (double death) superseded by `EnterPairedAnimationState()`/`ExitPairedAnimationState()` lifecycle API
- Gap 19.4 (SoftAimRange) is INTENTIONAL - working as designed
- 14 gaps deferred to Phase 6+ (IK, multi-victim, environmental finishers, VFX implementation)
- Phase 3 (UPairedAnimationComponent extraction) completed 2026-02-09. Methods referenced in gaps now live in PairedAnimationComponent, not CombatComponent.
- Gap 3.3 (Montage sections) verified as DONE by audit -- `Montage_JumpToSection` called in TryExecuteFinisher
- Gap 11.1 section header "ALL DONE" is misleading -- 11.1 is Working As Intended (damage at completion)
- Gap 12.3 (Root Motion Blending) downgraded to PARTIAL by audit -- motion warping handles most cases but no explicit mode management

---

## Full Gap Matrix

### Legend
- Done | Pending | Deferred

### 1. AI/ENEMY COORDINATION

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 1.1 | No Attack Token System | P1 | Pending |
| 1.2 | No Interrupt Finisher Mechanic | P0 | Partial (bReactionsSuppressed blocks reactions, but external damage still applies) |
| 1.3 | AI Awareness of Paired State | P1 | Pending |
| 1.4 | No Execution Prevention Window | P2 | Pending |
| 1.5 | Stacked Finisher Exploitation | P0 | Done (victim mutex via bIsFinisherTarget) |

### 2. INPUT HANDLING

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 2.1 | Player Input Not Blocked | P0 | Done (bBlockCombatInput in PairedAnimationComponent) |
| 2.2 | Camera Input Handling Undefined | P2 | Pending |
| 2.3 | No Finisher Button Prompt Timing | P2 | Pending |
| 2.4 | Evade/Block Not Disabled | P2 | Done (covered by bBlockCombatInput) |
| 2.5 | Menu Input During Cinematics | P3 | Pending |

### 3. ANIMATION/TIMING

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 3.1 | Montage Length Mismatch | P1 | Pending |
| 3.2 | Playback Rate Desync | P2 | Pending |
| 3.3 | Section Playback Not Enforced | P1 | Done (Montage_JumpToSection in PairedAnimationComponent::TryExecuteFinisher, verified by audit) |
| 3.4 | Animation Loop/Repeat Behavior | P2 | Pending |
| 3.5 | Interrupt Handling Incomplete | P1 | Pending |

### 4. AUDIO SYNCHRONIZATION

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 4.1 | Sound Effect Sync Points | P2 | Pending (see combat-polish-plan.md) |
| 4.2 | Voice Line Timing | P3 | Pending (see combat-polish-plan.md) |
| 4.3 | Music Ducking | P3 | Pending |
| 4.4 | Spatial Audio | P3 | Pending |

### 5. UI/HUD

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 5.1 | Health Bar Visibility | P3 | Pending |
| 5.2 | Damage Numbers Timing | P3 | Pending |
| 5.3 | Finisher Prompt Lifecycle | P2 | Pending |
| 5.4 | Screen Effects During Slow-Mo | P3 | Pending |
| 5.5 | Multi-Victim UI Conflict | P3 | Pending |

### 6. ENVIRONMENTAL INTERACTION

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 6.1 | Ledge Detection Not Integrated | P2 | Pending |
| 6.2 | Destructible Environment | P3 | Deferred |
| 6.3 | Damage Volumes | P3 | Pending |
| 6.4 | Moving Platforms | P3 | Deferred |

### 7. STATE TRANSITIONS (CRITICAL)

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 7.1 | Attacker Death Mid-Finisher | P0 | Done (OnPairedPartnerDeath + CancelPairedAnimation) |
| 7.2 | Victim Becomes Invulnerable | P1 | Pending |
| 7.3 | External Damage During Paired | P2 | Pending |
| 7.4 | Montage Fails to Play | P1 | Done (validated in TryExecuteFinisher) |
| 7.5 | Component Null Reference | P1 | Done (null checks added) |

### 8. PERFORMANCE

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 8.1 | Multiple Simultaneous Paired Anims | P3 | Pending |
| 8.2 | Ragdoll During Paired Animation | P2 | Pending |
| 8.3 | Physics Simulation Overhead | P3 | Deferred |

### 9. RECOVERY & CLEANUP

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 9.1 | State Machine Recovery | P1 | Pending |
| 9.2 | Incomplete Cleanup on Interrupt | P1 | Pending |
| 9.3 | Capsule Size Mismatch | P2 | Pending |
| 9.4 | Pose Recovery | P3 | Pending |

### 10. EXTENSIBILITY (FUTURE)

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 10.1 | Multi-Victim Finishers | P3 | Deferred |
| 10.2 | Environmental Finishers | P3 | Deferred |
| 10.3 | Weapon-Type Finishers | P2 | Pending |
| 10.4 | Context-Sensitive Finishers | P2 | Pending |
| 10.5 | Replay/Animation Blending | P3 | Deferred |

### 11. DELEGATE WIRING

| ID | Description | Status |
|----|-------------|--------|
| 11.1 | Sync Point Damage | Working As Intended (damage applied at completion, not sync point -- design choice) |
| 11.2 | Slow-Motion Trigger | Done |
| 11.3 | Camera Shake | Done |
| 11.4 | Hit Pause | Done |

### 12. ANIMATION INSTANCE

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 12.1 | Montage Position Sync | P2 | Pending |
| 12.2 | Bone Lock for Contact Points | P3 | Deferred |
| 12.3 | Root Motion Blending | P2 | Partial (motion warping handles practical cases, no explicit mode management) |

### 13. BUG/CRASH PREVENTION

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 13.1 | Division by Zero in Sync | P1 | Pending |
| 13.2 | Null Reference in Warp | P1 | Pending |
| 13.3 | Double Ragdoll Activation | P2 | Pending |
| 13.4 | Infinite Loop in History | P2 | Pending |
| 13.5 | Unhandled Montage Interrupted | P1 | Pending |

### 14. POLISH

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 14.1 | Ragdoll Settling | P3 | Deferred |
| 14.2 | Camera Follow During Finisher | P2 | Pending (see combat-polish-plan.md) |
| 14.3 | Screen Shake Stacking | P2 | Pending |
| 14.4 | Death Animation Loop Prevention | P2 | Pending |

### 15. VFX SCAFFOLDING

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 15.1 | Impact VFX at Sync Point | P3 | Done (3038b21, f27a068) |
| 15.2 | Blood/Damage Decals | P3 | Deferred |
| 15.3 | Slow-Motion Post-Process | P3 | Pending |
| 15.4 | Weapon Trail Enhancement | P3 | Deferred |
| 15.5 | Screen Blood Splatter | P3 | Deferred |
| 15.6 | Environment Destruction | P3 | Deferred |

### 16. IMPLEMENTATION

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 16.1 | Warp Config Struct Inconsistency | P2 | Pending |
| 16.2 | No Finisher Distance Validation | P2 | Done |
| 16.3 | Partner Array Not Persisted | P3 | Deferred |
| 16.4 | Cinematic Effects Not Auto-Wired | P2 | Done |
| 16.5 | No Finisher Cancel Animation | P2 | Pending |

### 17. EDGE CASES

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 17.1 | Finisher During Hitstop | P2 | Pending |
| 17.2 | Double Finisher Input | P2 | Pending |
| 17.3 | Victim Movement After Finisher | P2 | Pending |
| 17.4 | Attacker Blocked During Warp | P2 | Pending |
| 17.5 | Time Dilation Stacking | P1 | Done |

### 18. PHASE 5b-4 ANALYSIS (20 gaps, 7 consolidated to Cat 19)

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 18.1 | Failed execution cleanup | P0 | Done |
| 18.2 | Victim warp offset | P0 | Done |
| 18.3 | Attacker warp offset | P0 | Done |
| 18.4 | Null MotionWarping silent fail | P1 | Pending |
| 18.5 | Partner validity mid-anim | P1 | Pending |
| 18.6 | Distance uses SoftAimRange | P1 | Working As Intended |
| 18.7 | Target flag not cleared on interrupt | P0 | Done |
| 18.8 | Partner null at sync point | P1 | Pending |
| 18.9 | Victim warp after death | P1 | Pending |
| 18.10 | Cancel doesn't clear victim warp | P0 | Done |
| 18.11 | Stale WeakObjectPtr | P3 | **See 19.9** |
| 18.12 | Capsule physics + movement | P2 | **See 19.7** |
| 18.13 | Hitstop vs slow-mo coordination | P2 | Pending |
| 18.14 | Health threshold hardcoded | P2 | **See 19.5** |
| 18.15 | Multiple TODOs | P3 | Deferred |
| 18.16 | Victim montage start offset | P2 | **See 19.11** |
| 18.17 | Victim lacks TargetingComponent | P2 | **See 19.12** |
| 18.18 | Asymmetric collision settings | P2 | **See 19.13** |
| 18.19 | Duplicate partner registration | P2 | **See 19.10** |

### 19. GAP AUDIT (14 gaps)

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 19.1 | Null MotionWarping warning | P1 | Done |
| 19.2 | Sync point null check | P1 | Done |
| 19.3 | Warp tracking after death | P1 | Done |
| 19.4 | SoftAimRange for distance | P3 | Working As Intended |
| 19.5 | Health threshold hardcoded | P2 | Pending |
| 19.6 | Warp blocked by obstacle | P2 | Pending |
| 19.7 | Capsule physics + movement | P2 | Pending |
| 19.8 | Hitstop vs slow-mo dilation | P2 | Pending |
| 19.9 | Stale WeakObjectPtr | P3 | Pending |
| 19.10 | Bidirectional partner registration | P2 | Pending |
| 19.11 | Victim montage offset | P2 | Pending |
| 19.12 | Victim lacks TargetingComponent | P2 | Pending |
| 19.13 | Asymmetric collision | P2 | Pending |
| 19.14 | Cleanup on montage interrupt | P1 | Done |

### 20. TESTING SESSION (9 gaps)

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 20.1 | OnMontageEnded detection | P0 | Done |
| 20.2 | Damage application | P0 | Done |
| 20.3 | Input blocking cleanup | P0 | Done |
| 20.4 | Guard flag double-complete | P1 | Done |
| 20.5 | Sync point distance drift | P1 | Pending |
| 20.6 | Victim tracking reference | P1 | Done |
| 20.7 | Cleanup not atomic | P2 | Pending |
| 20.8 | No recovery phase after finisher | P2 | Pending |
| 20.9 | Weapon state during finisher | P2 | Pending |

### 21. DEATH ANIMATION (1 gap)

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 21.1 | Double death animation | P0 | Done (superseded by EnterPairedAnimationState lifecycle API) |

### 22. AUDIT FINDINGS (2026-02-03)

> Discovered by comprehensive audit. See `docs/audits/AUDIT_CLAUDE_2026-02-03.md` and `docs/audits/AUDIT_SYNTHESIS_2026-02-03.md` for details.

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 22.1 | GetWorld() null in GetHoldDuration() const query | P0 | Done (null check already exists at line 3163) |
| 22.2 | ICombatInterface stubs return wrong data (always Idle/false) | ~~P0~~ | **DEFERRED** — Wire when AI work begins. Parry window approach TBD (AnimNotifyState vs implicit phase vs Windup==ParryWindow). |
| 22.3 | const_cast UB in GetAttackForInput() -- make non-const | P1 | **Reclassified** — Not UB, just unnecessary const_cast leftovers (function already non-const). Fixed in a57eedd. |
| 22.4 | Static variable cross-instance contamination in DrawDebugInfo | P2 | Pending |
| 22.5 | ~~ActionQueue reverse iteration = LIFO not FIFO~~ | ~~P1~~ | **FALSE POSITIVE** — LIFO is intentional "last-input-wins" design for action games |
| 22.6 | HitDirection reversed in finisher vs normal hit | P2 | Done (2480e68 — fixed counter damage direction to Owner→Enemy) |
| 22.7 | Unnecessary tick enabled on BaseCombatCharacter | P3 | Pending — Add empty Tick() override as safeguard or disable after verifying Blueprint subclass dependency |
| 22.8 | Fragile reflection-based CombatSettings access in WeaponComponent | P2 | **INVESTIGATE** — May be intentional for editor dropdown support. Also: paired finisher asset needs montage section dropdown UI. |
| 22.9 | const_cast in TargetingComponent filter methods | P3 | Pending |
| 22.10 | CustomTimeDilation set to 0.0f (should be 0.0001f) | P1 | Done (879d1c2 — hitstop system uses 0.0001f) |
| 22.11 | 17 uncached FindComponentByClass calls in hot paths | P2 | Pending |
| 22.12 | CombatComponent.h includes BaseCombatCharacter.h (inverted dependency) | P3 | Pending |
| 22.13 | Internal state vars exposed as BlueprintReadOnly | P3 | Pending |
| 22.14 | Delegates in component headers instead of CombatTypes.h (Rule 6) | ~~P2~~ | **RULE UPDATED** — Two-tier approach adopted: cross-component → CombatTypes.h, component-internal → stays in header. Only move delegates that are truly cross-component. |

---

---

## NEW GAPS (2026-02-06)

### 23. CAMERA & COLLISION (CAM-1)

> **Problem**: Camera pushed inside player when moving between enemies. Spring arm collision treats enemy pawns as obstacles. **Worst during root motion** due to rapid position changes.

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 23.1 | Camera pushed inside player when between enemies | P1 | Done (5dc5dc3 — "Enemy" collision profile ignores ECC_Camera) |
| 23.2 | Spring arm has no ignored actors list for enemies | P1 | Done (5dc5dc3 — solved via collision profiles, not ignored actors) |
| 23.3 | Camera collision during finisher cinematics | P2 | Pending |
| 23.4 | No smooth blend for collision avoidance | P3 | Pending |

**Root Cause**: Spring arm collision treated enemy pawns as obstacles, auto-retracting camera.

**Fix Applied** (5dc5dc3):
- Added "Enemy" collision profile in `DefaultEngine.ini` that ignores ECC_Camera
- `EnemyCharacter` constructor sets `GetCapsuleComponent()->SetCollisionProfileName(TEXT("Enemy"))`
- Clean config-based approach — no runtime collision management needed

**Remaining**: 23.3 (finisher cinematics) and 23.4 (smooth blend) are separate polish items.

---

### 24. HIT DETECTION ROBUSTNESS (HIT-1) — ALL DONE

> **Problem**: Hits being dropped. Swept trace system inadequate for fast targets and lacked rich hit analytics.
> **Resolved**: 2026-02-16 (2744f5c, 09d3d8b)

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 24.1 | Hits dropped on fast-moving targets | P1 | Done (2744f5c — blade segmentation + velocity-adaptive substeps) |
| 24.2 | Silent socket fallback causes misaligned traces | P1 | Done (2744f5c — GetSocketLocation logs Error on missing sockets) |
| 24.3 | FHitReactionInfo missing AnimationTime field | P1 | Done (prior commit — AnimationTime populated from Montage_GetPosition) |
| 24.4 | bWasCounter always false (never populated) | P1 | Done (prior commit — populated from CombatComponent::IsInCounterWindow) |
| 24.5 | No WeaponVelocity in hit info for knockback direction | P2 | Done (2744f5c — CachedWeaponTipVelocity from per-frame socket tracking) |
| 24.6 | Surface type detection (ECombatSurfaceType) not wired | P2 | Done (2744f5c — bReturnPhysicalMaterial + MapPhysicalMaterialToSurfaceType) |
| 24.7 | No trajectory prediction for high-speed weapons | P3 | Done (2744f5c — PredictHitLikelihood in WeaponTraceLibrary for AI use) |

**What was built** (2744f5c, 09d3d8b):
- **WeaponTraceLibrary** (new): Pure static functions for blade segmentation, adaptive substeps, surface mapping, hit confidence, trajectory prediction
- **Blade segmentation**: Multi-point trace (base→mid→tip) captures full swing arc
- **Velocity-adaptive substeps**: 1-5 substeps scaled by weapon tip speed (configurable on WeaponData)
- **Surface FX**: bReturnPhysicalMaterial enabled, ECombatSurfaceType populated via PhysicalMaterial mapping
- **Hit confidence**: Quality metric (velocity + blade position) for hitstop/VFX scaling
- **Weapon-velocity HitDirection**: Blade travel direction (negated for convention) replaces position-based fallback
- **MaxHitCount**: Per-attack hit cap on AttackData (0=unlimited)
- **Diagnostics**: Zero-hit warnings on window close, verbose per-frame CVar logging
- **Audit fixes**: Socket names use GetEffective*, stale velocity cleared, GetCurrentPhase delegated

---

### 25. INPUT/COMBO RESOLUTION (INPUT-1)

> **Problem**: Rapid input spam causes combo flip-flop: 1→2→partial 3→back to 1 instead of smooth 1→2→3→4→5→6 chain.

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 25.1 | CurrentAttackData set AFTER PlayAttackMontage returns (race condition) | P0 | Done (9534131 — set state BEFORE PlayAttackMontage, revert on failure) |
| 25.2 | StopAllMontages(0.0f) triggers immediate OnMontageEnded callback | P0 | Done (9534131 — PendingComboTransitions counter rejects stale callbacks) |
| 25.3 | SetPhase(None) clears CurrentAttackData while ComboWindow still active | P0 | Done (9534131 — FAttackStateMachine.ShouldProcessMontageEnd Rule 0) |
| 25.4 | BUG-3 FIX (line 3387) forces bShouldCombo=false when CurrentAttackData=nullptr | P0 | Done (9534131 — CurrentAttackData no longer null during combo) |
| 25.5 | Attacks can blend out of Active phase (should only blend in Recovery) | P1 | Pending |
| 25.6 | No guard ensuring each attack reaches Recovery before next blend | P1 | Pending |
| 25.7 | Same input sequence can produce different animation results | P2 | Done (9534131 — combo progression now deterministic) |
| 25.8 | 100+ rapid input stress test not validated | P2 | Pending |

**Root Cause & Fix** (9534131): Race condition — `CurrentAttackData` set after `PlayAttackMontage()` returned, allowing stale `OnMontageEnded` callbacks to clear state mid-combo. Fix: set state BEFORE montage play with revert-on-failure pattern, plus `PendingComboTransitions` counter to reject stale callbacks. See `ComboRaceConditionTests.cpp` for test coverage.

---

### 26. CORE COMBAT FLOW (New — 2026-02-06)

> **Context**: The parry → counter → finisher chain is the primary remaining combat loop. Counter foundation committed (f6b4318) but full flow is unimplemented. Guard break mechanics also missing despite posture system being stable.

| ID | Description | Priority | Status |
|----|-------------|----------|--------|
| 26.1 | Parry system implementation (defender checks attacker's ParryWindow) | P1 | Source implemented - public Block-input entry, attacker-side parry selection, active Chain target/context, and readiness reporting are covered by automation. Remaining blocker: asset-backed montage/map proof. |
| 26.2 | Counter system flow (counter window → counter attack execution) | P1 | Source implemented - attack-input Chain advance, selected AttackData handoff, paired counter continuation, nonlethal counter semantics, and readiness report fields are covered by automation. Remaining blocker: asset-backed counter montage proof. |
| 26.3 | Parry -> Counter -> Finisher chain (full cinematic combat loop) | P1 | Source implemented at fixture level - public Block entry, attack advance, active context, paired counter completion handoff, and finisher transition are covered. Remaining blockers: concrete montage/map proof and remaining asset-save decisions beyond the reviewed heavy montage save. |
| 26.4 | Guard break mechanics (posture depletion → guard broken state) | P1 | Replaced — Posture deprecated (9534131). Contextual stagger via ApplyStagger() replaces guard break. |
| 26.5 | Counter-specific fields on PairedAnimationData | P2 | Source implemented - selected `AttackData::CounterData` priority, explicit notify fallback gate, and nonlethal default are covered. Remaining blocker: real asset authoring/proof. |
| 26.6 | Parry-specific fields on PairedAnimationData | P2 | Partially source-proven - parry windows are detected from attacker montages and surfaced in readiness reports. Remaining blocker: real asset authoring/proof. |
| 26.7 | FOnAttackHit delegate downstream consumers | P2 | Pending — Upgraded to (AActor*, FHitReactionInfo&) in 879d1c2, no consumers wired |
| 26.8 | Procedural blend edge cases (blend during death, paired animation, guard break) | P2 | Pending — 64 tests pass but edge cases untested |
| 26.9 | Attack state machine recovery (stale section data after interrupts beyond grace period) | P2 | Done — PendingComboTransitions counter in 9534131 provides systemic recovery |

**Dependencies**:
- 26.1 (Parry) → enables 26.2 (Counter) → enables 26.3 (Full Chain)
- 26.4 (Guard Break) is independent but synergizes with 26.1
- 26.5, 26.6 depend on 26.1/26.2 design decisions

**Branch status summary**: source flow is implemented at fixture level. `AttackDataTimingMigration` fixed the short-section timing blockers for `LightAttack_6` and `LightAttack_9`. `AttackDataNotifyMigration` saved the reviewed heavy/light accepted targets plus the remaining 18 branch-critical notify candidates. Final pre-merge audits reported `ContentReadinessAudit` at 41 targets, 41 unchanged, 0 failed, and global `AttackDataNotifyMigration` at 20 targets, 20 unchanged, 0 failed. Future commit work remains: explicit content-authored counter-chain proof because no audited AttackData currently has `CounterData` set, plus any separately scoped cleanup of broader dirty `Content/` WIP.

---

## Active Priority Buckets (Refreshed 2026-02-09)

> All P0 items resolved. See individual gap tables above for Done status and commit references.

### P1 HIGH

**HIT-1 (Hit Detection)**: ALL DONE (2744f5c, 09d3d8b) — See Category 24

**Core Combat Flow** (NEXT FOCUS):
- 26.1: Parry system implementation
- 26.2: Counter system flow
- 26.3: Parry → Counter → Finisher chain

**Paired Animation Robustness**:
- 1.2: Interrupt finisher mechanic (partial)
- 3.5: Interrupt handling
- 9.1/9.2: State machine recovery + incomplete cleanup
- 13.1-13.2, 13.5: Crash prevention (null refs, division by zero)
