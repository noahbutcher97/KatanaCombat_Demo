# Finisher System & Motion Warping Audit — 2026-07-21

Current-state update (2026-09-10): this is a historical audit. The current attacker/victim source sequences now have root motion enabled, and the preview/evaluation tool has since changed. The [current contact diagnosis](FINISHER_CONTACT_DIAGNOSIS_2026-09-10.md) verifies movement-disabling montage notifies, an opening damage/alignment snap and a start-boundary sync-reporting defect. Use that report for the next correction; do not apply the disabled-root-motion recommendation below without rechecking current assets.

**Trigger**: paired sequence members do not align ahead of sequence start; motion warping control is unclear.
**Method**: systematic-debugging Phase 1-3 — full source trace of both finisher paths, the warp control surface, and the preview tool, cross-checked against serialized montage/sequence/data-asset content. No fixes applied; recommendations at end.

---

## 1. Root-Cause Verdict (the alignment problem)

The symptom has a **two-layer root cause**, both confirmed with evidence:

### Layer 1 — PRIMARY: the finisher animations have root motion disabled

Motion warping (`RootMotionModifier_SkewWarp`) does not *create* movement — it **redirects root motion the montage extracts**. Zero root motion in → zero movement out, no matter how the warp targets are configured.

Serialized-content evidence:
- `GhostSamurai_Ambush01` (attacker finisher source): `bEnableRootMotion` **not serialized → false** (UAnimSequence default)
- `GhostSamurai_Ambushed01` (victim finisher source): same — **false**

Meanwhile every layer *above* the root motion is correctly configured on the Gate A montages (`AM_Finisher_Attacker`/`AM_Finisher_Victim`) and the legacy `Finishers_A_Attacker`:
- `AnimNotifyState_MotionWarping` present ✓, with `RootMotionModifier_SkewWarp` ✓
- Modifier `WarpTargetName = "PairedTarget"` ✓ — matches `FPairedWarpConfig.WarpTargetName` default
- `UMotionWarpingComponent` created on every `ABaseCombatCharacter` (BaseCombatCharacter.cpp:209) ✓
- Runtime registers and continuously updates the `"PairedTarget"` target (TargetingComponent.cpp:1724/1802 victim, 1925/2012 attacker) ✓

So the chain is: config ✓ → target registered ✓ → notify window ✓ → name match ✓ → **root motion ✗** → nobody moves.

**Caveat to verify in-editor** (5 minutes): open each sequence and check whether the **root bone actually animates**. Two outcomes:
- Root bone has motion → ticking `Enable Root Motion` on both sequences makes SkewWarp work immediately.
- Animation is authored in-place (root bone static) → enabling the flag changes nothing; warping *cannot* produce the approach, and Layer 2 below becomes the required fix.

### Layer 2 — ARCHITECTURAL: no pre-start alignment exists at all

Even with root motion working, the legacy finisher path has **no mechanism that aligns actors before the montages start**:
- Grep-verified: zero `SetActorLocation`/`SetActorRotation`/`TeleportTo` calls in PairedAnimationComponent.cpp/HitReactionComponent.cpp before montage play. `EnterPairedAnimationState` sets state flags only (HitReactionComponent.cpp:1549-1591).
- The execution order is: gates → `Montage_Play` (attacker :3773) → `SetupAttackerPairedWarp` (:3798) → victim `Montage_Play` (:3825) → `SetupVictimWarp` (:3852). Warping then converges the actors **during** the notify window.
- The only positional correction anywhere is a **mid-montage nudge at the sync point** (`AnimNotifyState_PairedAnimationSync.cpp:115`, gated by `bNudgeOnMinorMisalignment` within `[NudgeThreshold, MaxContactDistance]`).
- There is **no facing gate** (finisher triggers regardless of attacker orientation; `GetAbsoluteYawToTarget` is used only by the defense-chain path) and **no minimum distance** (SoftAimRange is a max-only check, PairedAnimationComponent.cpp:2677-2688; an overlapping attacker passes).

So "members don't align ahead of the sequence start" is literally true by design of the legacy path: first frames always play from wherever the actors stood.

**Contrast — the defense-chain finisher stage already solves this** (PairedAnimationComponent.cpp:2846-3465): it *preflights* pair distance against `[MinTriggerDistance, MaxTriggerDistance]` (:2941), checks a translation budget against `MaxWarpDistance` (:2956-2995), checks a rotation budget (:2997-3042), requires named rotation warps on both montages (:2932), and acquires alignment-arbiter ownership **before** `Montage_PlayWithBlendIn` (:3307-3318 vs :3387). This is why Gate A proof content behaves while manual finishers don't.

---

## 2. How a Finisher Actually Executes (control map)

### Legacy path (player-facing finishers)
`CombatComponent::ExecuteAction` (:3987) tries `TryExecuteFinisher(AttackData)` before the normal attack.

Gates, in order (PairedAnimationComponent.cpp:2610-2750):
`AttackData->FinisherData` null-check → target (hard-lock, else soft-aim forward) → hostility → **distance ≤ TargetingSettings.SoftAimRange** (fallback 500; max only) → `IsPathClear` (radius 30) → target has HitReactionComponent → `IsVulnerableToFinisher()` → hand off to `TryStartPairedAnimationWithTarget`.

Then (:3645-3911): numeric validation → busy/ownership checks → victim `EnterPairedAnimationState` (flags only) → partner registration → `BeginPairedAnimation` (input lease, optional slow-mo lease; **collision lease comes later from the montage's PairedAnimationCollision notify**, :846/:926) → attacker montage plays → attacker warp setup → victim montage plays (start pos `max(0,-VictimStartOffset)`) → victim warp setup → rollback on failure.

### What the data asset controls (the knobs that are real)
| Knob | Effect |
|---|---|
| `AttackData.FinisherData` | THE finisher gate (null = no finisher). `bCanTriggerFinisher` is editor-validation only. |
| `AttackerMontage/VictimMontage` + sections | What plays |
| `VictimStartOffset` | Victim starts later (negative only honored on this path) |
| `AttackerWarpConfig` / `VictimWarpConfig` (`FPairedWarpConfig`) | The entire alignment intent — see §3 |
| `bApplySlowMotion`/`SlowMotionScale`/`SlowMotionDuration` | World time-dilation lease |
| `ImpactSound/VictimReactionSound/AttackerVoiceLine/ImpactVFX/ImpactCameraShake` | Fired by the sync-point **notify** |
| `BaseDamage × DamageMultiplier`, `bIsLethal`, `VictimDeathOutcome`, `RagdollBlendTime` | Completion/damage |
| `MinTriggerDistance/MaxTriggerDistance/MaxWarpDistance` (top level) | **Defense-chain preflights only** — the legacy finisher path ignores them (uses SoftAimRange) |

### FPairedWarpConfig semantics (what the runtime actually does)
- `WarpTargetName` (default `"PairedTarget"`): must equal the montage's MotionWarping notify modifier name. Registration is **blind** — nothing verifies the montage has a matching window (no `ContainsAnyMatchingWarpNotifies` anywhere).
- `RelativeOffset`: **partner-local space**. Victim target = `AttackerLoc + AttackerRot.RotateVector(Offset)` (TargetingComponent.cpp:1707). Attacker target = `VictimLoc + VictimRot.RotateVector(Offset)`, clamped to `MaxWarpDistance` (:1899-1908). Default `(100,0,0)` = 1m in front of partner.
- `bWarpTranslation`: when false the target is published at the actor's own location (:1726/:1927) — rotation-only. Defaults: attacker rotation-only, victim translates (PairedAnimationData.cpp:107-117).
- `bWarpRotation`: victim faces attacker / attacker faces victim (:1720/:1921).
- `bAdjustToTerrain`: ground-samples target Z.
- Continuous tracking: both setups bind `MotionWarpingComponent->OnPreUpdate` and re-register the target **every warp update** from the partner's current transform (:1802/:2012) — cadence exists only while a root-motion montage is evaluating (another reason root motion off = fully inert).

---

## 3. The Three Warp Systems (and how not to mix them)

| System | Entry | Warp names | Notify type expected | Arbiter? |
|---|---|---|---|---|
| **Attack warp** (normal attacks) | `SetupAttackWarp` via `FAttackWarpConfig` | `"AttackTarget"` / `"RotationTarget"` | **custom `AnimNotifyState_CombatWarp`** | Yes — `AcquireAlignmentRequest`, priority ActiveAttackWarp |
| **Paired warp** (legacy finishers/counters) | `SetupAttackerPairedWarp`/`SetupVictimWarp` via `FPairedWarpConfig` | `"PairedTarget"` | **stock `AnimNotifyState_MotionWarping`** (SkewWarp) | No — direct registration |
| **Defense alignment** (blocks/parries/defense-chain) | `AcquireAlignmentRequest` specs | `"DefenseContactTarget"`, `"AttackerResponseTarget"`, `"PairedTarget"` (chain stages) | stock MotionWarping (validated) / CharacterMovement executor for guard | Yes — priority arbiter |

**The trap**: `CombatWarp` notifies **fail closed** unless the alignment arbiter owns the target (`AnimNotifyState_CombatWarp.cpp:83-98` — modifier `MarkedForRemoval`, Warning logged). The legacy paired path never creates an arbiter request, so:
- Paired/finisher montages must use the **stock** MotionWarping notify (they do, verified).
- Putting a CombatWarp notify on a finisher montage silently disables its warp.
- Conversely the arbiter **rejects** names already registered outside it (TargetingComponent.cpp:797-803), so the two systems cannot share a name.

---

## 4. Debugging Warping (how to see what's happening)

1. `Combat.Debug.PairedAnim.Warp 1` — draws warp crosshairs + finisher range circle. **Caveat**: the HUD draws *approximations* (`PartnerActorLocation`, CombatDebugHUD.cpp:661-684), i.e. "tracking is active", not the true published target.
2. Targeting debug (attack path) draws the **actual** `FindWarpTarget()` transform (green sphere, TargetingComponent.cpp:698-706).
3. Log channels: `LogPairedAnim` warnings "no MotionWarpingComponent" (:1663/:1852); `LogCombatWarp` "No target … skipping warp" (Verbose) and "could not bind to one active alignment owner" (Warning).
4. Diagnostic ladder for "registered but not moving": green sphere/crosshair present? → montage playing? → **does the sequence have root motion?** → does the notify window overlap playback? → correct notify type + name?

---

## 5. Recommendations (not yet applied — pick and I'll execute)

### R1 — Immediate (content): enable root motion on the finisher sequences
Tick `Enable Root Motion` on `GhostSamurai_Ambush01`/`Ambushed01` (and any other execution sources). Verify the root bone animates; test a finisher. If the anims are in-place, R2 becomes mandatory rather than recommended.

### R2 — Architectural: give the legacy finisher path a pre-start alignment step
Options, in increasing fidelity:
- **(a) Entry snap** (cheap, Arkham-style): before `Montage_Play`, compute both entry transforms from the warp configs (victim: `AttackerLoc + AttackerRot⋅RelativeOffset`; attacker rotation to face) and `SetActorLocationAndRotation` when within a tolerance; reject or approach otherwise. The sync-nudge code (AnimNotifyState_PairedAnimationSync.cpp:103-115) is the in-codebase precedent.
- **(b) Pre-align phase** (best feel): a brief (~0.15-0.3s) alignment phase before montage start that drives both actors to entry poses via the **existing alignment arbiter** with `CharacterMovement` executor (the guard path already does exactly this, CombatComponent.cpp:2693) — capsule-driven, root-motion independent.
- **(c) Adopt the defense-chain preflight** for legacy finishers: reuse `PreflightDefenseChainStage`'s distance-window/translation-budget/rotation-budget gates so unstartable finishers are rejected instead of playing misaligned.
Recommended: (a) now, (b) as the polish pass; (c)'s gates fold into either.

### R3 — Validation (stop this class of bug recurring)
Extend `UPairedAnimationData::IsDataValid`:
- Error if `AttackerMontage`/`VictimMontage` lacks a stock `AnimNotifyState_MotionWarping` whose modifier `WarpTargetName` matches the corresponding `FPairedWarpConfig.WarpTargetName` (the `HasNamedRotationWarp` pattern already exists in DefenseAssetValidationService.cpp:345-364 — lift it).
- Warning if `bWarpTranslation=true` but the montage's source sequences have `bEnableRootMotion=false` (**this exact bug**, machine-checkable).
- Warning if a paired montage carries a `CombatWarp` notify (fail-closed trap).
Optionally add a one-line runtime warning in `SetupVictimWarp`/`SetupAttackerPairedWarp` when the playing montage contains no matching warp window (registration is currently blind).

### R4 — Debug accuracy
Make the paired warp HUD draw the real published target (`MotionWarpingComp->FindWarpTarget(Config.WarpTargetName)`) instead of partner actor locations, and add a "root motion extracted this frame: N cm" readout — turns the diagnostic ladder in §4 into one screen.

---

## 6. Preview Tool — Current State & Workflow Proposal

### What it is today (~11.7k lines, Window → Paired Animation Preview)
A standalone **analysis sandbox** with genuinely strong math (holistic optimization, contact detection, spatial-relationship inference, bone trajectories) — and **zero coupling to `UPairedAnimationData`**:
- Inputs are hand-picked meshes + montages; no asset picker; nothing seeded from warp configs.
- Its transform model is its own: `LockedDistance` along world +X plus yaw offsets and a hardcoded −90° mesh yaw (PairedAnimationPreview.cpp:667-729) — **not** the runtime's partner-local `RelativeOffset` model.
- It does not simulate warp windows or read/apply any asset field (`SyncPointTime`, `VictimStartOffset`, blends: all unreferenced). Timeline is a single scrub slider — no per-montage tracks, no notify windows, no sync marker.
- **No write-back of any kind**: optimizer results (`RecommendedDistance`, both `RecommendedRotation`s — exactly the values a warp config needs) mutate only the in-memory model; export = clipboard CSV; JSON export is a TODO stub; Redo is documented-broken.

### Proposed workflow upgrade — "Finisher Authoring Mode" (phased)

**Phase P1 — Asset in (1-2 sessions of work):**
- `UPairedAnimationData` picker in the Assets panel; selecting it populates both montages + sections (extension point: mirror `OnAttackerMontageSelected`, PairedAnimationPreview.cpp:856).
- Seed positioning from the asset: map `VictimWarpConfig.RelativeOffset` (partner-local) → the tool's distance/yaw model on load, replacing the arbitrary +X default.
- Timeline upgrades: draw both montages' notify windows (MotionWarping, Sync, Collision) + a `SyncPointTime` marker; **badge missing/mismatched warp notifies and root-motion-off sequences** — the preview becomes the place where §5-R3's failures are visible before PIE.

**Phase P2 — Asset out (the payoff):**
- "Apply to Asset" button: `BeginTransaction` → write tool distance/rotations back as `VictimWarpConfig.RelativeOffset` (+ attacker config), `VictimTimeOffset` → `VictimStartOffset` → `MarkPackageDirty`. Natural hook: `ApplyOptimizationResult` (:2599) already receives the computed values; the missing piece is the scalar-distance→partner-local-FVector conversion helper.
- "Optimize → Apply" then becomes a two-click authoring loop: load finisher asset → auto-optimize → apply → save.

**Phase P3 — WYSIWYG fidelity (unification goal):**
- Replace/augment the tool's positioning with the **same math as the runtime** (`AttackerLoc + AttackerRot⋅RelativeOffset` etc.) — ideally by extracting that math from `TargetingComponent` into a static pure function both call (the same pattern CLAUDE.md already prescribes for `PerformWeaponTrace`).
- Optional: simulate warp convergence over the notify window using extracted root motion, so the preview shows the actual approach, not just the end pose.

Also worth folding into any preview work: fix broken `RedoOptimization`, wire or remove `BuildGraphsPanel`, surface the computed-but-hidden active-notify arrays.

---

## Evidence Index
Finisher timeline & gates: PairedAnimationComponent.cpp:2610-2750, 3645-3911; defense-chain: 2846-3465. Warp setup/tracking: TargetingComponent.cpp:1650-2040; arbiter: 550-1200. CombatWarp fail-closed: AnimNotifyState_CombatWarp.cpp:25-101. Sync nudge: AnimNotifyState_PairedAnimationSync.cpp:103-115. Component: BaseCombatCharacter.cpp:209. Content: AM_Finisher_Attacker/Victim exports (MotionWarping notify + SkewWarp "PairedTarget"), GhostSamurai_Ambush01/Ambushed01 (`bEnableRootMotion` unserialized=false). Preview tool: PairedAnimationPreview.cpp (5,746 lines), SPairedAnimTimelineView.cpp, PairedAnimationAnalysisSubsystem.cpp. Validation gap: PairedAnimationData.cpp:180-234 vs DefenseAssetValidationService.cpp:345-364, 2834-2854.
