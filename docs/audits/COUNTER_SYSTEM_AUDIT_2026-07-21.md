# Counter System Audit — 2026-07-21

Companion to `FINISHER_WARP_AUDIT_2026-07-21.md` (same method; read that first for the warp-mechanics background). `comp:` = `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`.

---

## 1. Mode Selection — AC3 is dormant

- `ECounterSystemMode { AC3, Chain }` (CombatTypes.h:212) is a **per-component EditAnywhere property**, default **Chain** (PairedAnimationComponent.h:599-600). Companion flags: `bAllowNotifyCounterDataFallback=false` (:619), `bAllowLethalCounterPairedData=false` (:623).
- **No content overrides it** — binary scan of BP_Player.uasset and all Content/ found no serialized `CounterMode`; Config/ has none. Chain is authoritative everywhere.
- Consequence: `TryCounter()` and `CanCounter()` early-return under Chain (comp:3994-3999, 4049-4052) — **the entire AC3 timeline is dormant code** in the shipping default. It runs only if a Blueprint flips the property.

## 2. AC3 Mode (dormant) — instant counter-kill

Block press (no locked parry threat) → `TryCounter()` (CombatComponent.cpp:1430-1435) → gates: state Idle/Blocking, not blocked/completing, `FindCounterableEnemy()` (sphere overlap SoftAimRange + enemy `IsInCounterWindow()` from the attacker-side `AnimNotifyState_CounterWindow`) → `TryCounter_AC3Mode()` (comp:4271):
- With notify `SpecificCounterData`: **legacy paired path** `TryStartPairedAnimationWithTarget` (comp:4285) — the blind-warp body (no translation/rotation preflight, `SetupAttackerPairedWarp` after Montage_Play).
- Without: **no montage at all** — `ApplySlowMotion(0.2)` → `ApplyStagger(2.0)` → direct lethal damage `CurrentHealth+1` (comp:4297-4322).

## 3. Chain Mode (the real system) — full timeline

**Load-bearing structural fact:** `BeginDefenseSequence` sets `ChainState=ParryActive` *before* starting the parry bridge (comp:1690-1694), and `TryStartPairedAnimationWithTarget` **redirects into `TryStartDefenseChainStage`** whenever a defense sequence owns the component (comp:3659-3674). So **all three Chain stages (Parry, Counter, Finisher) are preflighted and arbiter-aligned**; the legacy blind-warp body is reached only by AC3 counters and standalone `TryExecuteFinisher`.

1. **Block press → parry resolution**: `TryCommitPerfectParry` (CombatComponent.cpp:1427-1429) → `DefenseResolver::ResolveInputIntent` (DefenseResolver.cpp:188): parry window + `Attack.Defense.Parryable` tag + High-confidence predicted contact + reachability → `PerfectParry`, `bChainEligible` (:281-286).
2. **`BeginDefenseSequence`** (comp:1502): entry gates (:1513-1543), bridge presentation via `PreflightDefenseBridge` (:1550, 1734).
3. **ParryActive** — two sub-paths:
   - *Montage bridge* (`PairedBridgeData`, e.g. DA_ParryBridge_GateA): defense-chain stage start (below).
   - *No-montage bridge*: **zero animation** — a `NoMontageParryBridgeSeconds` (0.15s) timer, then straight to CounterWindow (comp:1718, 2078-2098). The parry is invisible.
   - Bridge budgets (comp:1734-2073): distance ∈ `[MinTriggerDistance, MaxTriggerDistance]`; translation ≤ `PerfectParryTranslationAllowancePerRole` (75) ∩ `MaxWarpDistance` ∩ payload `MaximumTranslation`; rotation ≤ `MaximumAutomaticTurn`/`DefenseTurnRate` vs time-to-contact; path-clear sweeps.
4. **→ CounterWindow**: driver montage's `AnimNotify_ChainStageTransition(OpenCounterWindow)` marker, identity-verified (driver role, montage instance, notify source+index, comp:531-595) → `EnterDefenseCounterWindow` (comp:2133), deadline `CounterWindowSeconds` (2.0s, comp:2219-2231).
5. **CounterWindow + Light/Heavy press → CounterActive**: `TryAdvanceChainCounter` (CombatComponent.cpp:1448-1454 → comp:4345) → `ExecuteChainCounterAttack` (comp:4365) → **`TryStartDefenseChainStage(CounterData, Counter, CounterActive)`** (comp:4399-4402):
   - `PreflightDefenseChainStage` (comp:2846): montages/sections valid, named rotation warps required on both roles, distance window, **translation budget = min(PairedData.MaxWarpDistance, WarpConfig.MaxWarpDistance)** (parry-only allowances do NOT apply to counters, comp:3216-3233), rotation budget, path-clear, exactly-one playable driver marker, distinct adjacent role montages.
   - Then collision leases (comp:3183-3188) → **alignment-arbiter acquisition BEFORE montage play** (`BuildAlignmentSpec` executor=MotionWarping, name from `FPairedWarpConfig.WarpTargetName`, comp:3237-3318) → both montages `Montage_PlayWithBlendIn` (comp:3387, 3434) → rollback on any failure with deadline restored (comp:3465-3583).
6. **CounterActive → finisher handoff**: auto-continue marker (`bAutoContinue`) applies damage then starts FinisherActive immediately (comp:2238-2281; failed start + `bFinisherRetryable` → FinisherReady, comp:2283-2316); otherwise montage end applies damage → **FinisherReady** with `FinisherReadySeconds` (2.0s) deadline (comp:2410-2463).
7. **FinisherReady + press → FinisherActive** (`ExecuteChainFinisher`, comp:4415-4428) → same arbiter path → completion cleans up (comp:2404-2409).

## 4. Counter Data Resolution

- **Chain** (comp:4378-4389): `ChainAttackData->CounterData` → `SpecificCounterData` *only if* `bAllowNotifyCounterDataFallback` → **no non-paired fallback**: returns false, window stays open, input marked `Expired` (CombatComponent.cpp:1458). Matches the documented contract.
- **AC3** (comp:4280-4327): notify `SpecificCounterData` paired → else the slow-mo/stagger/lethal-damage fallback (no montage). The "plain damage" fallback exists **only** in AC3.

## 5. Alignment & Positioning Facts

- **No pre-start snap exists for counters or finishers** — zero `SetActorLocation`/`TeleportTo` in the component (grep-verified). Chain relies on preflight *rejection* + arbiter-registered MotionWarping; AC3 relies on blind warp registration.
- **Out-of-position counter-victim: Chain rejects** (preflight fails → stage doesn't start → window stays open → eventually `ResponseTimeout`); **AC3 plays misaligned** (no budgets).
- Destination math is the same partner-local form as finishers: `TargetLocation + TargetRotation ⋅ RelativeOffset` (comp:2971, 3251-3254).
- **Content status (verified)**: `AM_Counter_Attacker`/`AM_Counter_Defender` carry stock MotionWarping notifies with `SkewWarp("PairedTarget")`, and their DynamicKatana source sequences have **`bEnableRootMotion = true`** — unlike the finisher sequences (false). This is why Gate A counters can visibly align while standalone finishers cannot: same plumbing, one asset flag different. Note the counter attacker montage is composited from repurposed hit/block sequences (`AS_Block_Hit_Break_Seq` at 0.13× + `AS_Hit_Large_F_Seq`) — placeholder-quality, but mechanically live.

## 6. Damage

- **Chain counters**: single clamp site `ApplyActivePairedDamageOnce` (comp:1217-1219): non-lethal counter damage ≤ `CurrentHealth − 1`. Lethal only when `bAllowLethalCounterPairedData && PairedAnimData->bIsLethal` (comp:3927-3930); parries always non-lethal.
- **Legacy clamp GAP (bug)**: `CompletePairedAnimation` (comp:5090-5131) applies unclamped `FinalDamage` for non-lethal counters (:5128-5130). Reached by **AC3 counters started with SpecificCounterData** — a "non-lethal" counter with `BaseDamage×DamageMultiplier ≥ CurrentHealth` kills anyway. Dormant under Chain default, live under AC3.

## 7. Failure / Retry / Silence

- Counter stage start fails → state restored to CounterWindow, deadline preserved — retry by pressing again (comp:3572-3583, 4408-4412).
- Window expiry → `CleanupDefenseSequence("ResponseTimeout")` (comp:1139-1176) tears down leases/warps.
- Parry with no CounterData → counter press silently fails (comp:4384-4389); window runs out. **No player-facing feedback for any of the silent-rejection cases.**

## 8. Ranked Failure/Misalignment Vectors

1. **AC3 blind-warp misalignment + unclamped kill** — highest severity, currently dormant; becomes live the moment `CounterMode` flips to AC3. (comp:3798 blind warp; comp:5128 clamp gap.)
2. **Missing MW notify / root motion on counter montages** — preflight cannot detect it; arbiter registers a target, montages play, nobody moves. The dominant Chain misalign vector. (Content currently OK for Gate A counters.)
3. **Silent preflight rejection** — out-of-position/blocked counters produce no animation and no feedback; window quietly times out.
4. **Invisible no-montage parry** — ParryActive without `PairedBridgeData` plays nothing; combined with (3) or missing CounterData, the entire exchange is invisible to the player.
5. **Marker authoring violations** — not-exactly-one playable driver marker, or identical adjacent role montages, refuse the stage (comp:2916-2924, 3079-3103).
6. **Stale marker identity** — re-authored/duplicated `ChainStageTransition` notifies are identity-rejected (comp:531-595); CounterWindow never opens; chain stalls to deadline.

## 9. Recommendations

- **C1 (bug)**: add the one-health clamp to `CompletePairedAnimation`'s non-lethal counter path (comp:5128-5130) — same rule as `ApplyActivePairedDamageOnce`. Small, test-backed.
- **C2 (decision ⚡)**: decide AC3's fate. It is dormant, gate-free, and carries vector #1. Options: delete (Chain is the accepted system; removes ~200 lines + the `TryCounter` API), or keep-and-harden (route AC3 SpecificCounterData counters through the same preflight). Deleting also resolves the clamp-gap route (C1 still worth fixing for the standalone finisher route).
- **C3 (UX)**: player feedback for silent rejections — at minimum a whiff/decline cue when counter preflight rejects or CounterData is missing, and telemetry counters already exist to hook (`Combat.ActionReaction` channel).
- **C4 (validation)**: extend the same `IsDataValid` upgrades from the finisher audit (R3) to counter-referenced `UPairedAnimationData`: MW notify presence + name match + root-motion check + exactly-one `ChainStageTransition` marker when `ChainTransitionPolicy` is set (the marker rule is enforced at runtime and by DefenseAssetValidationService for manifests, but not on the asset itself).
- **C5 (docs)**: CLAUDE.md "Counter Chain Mode" is accurate; add one line noting AC3 is dormant-by-default and that all Chain stages run the defense-chain preflight (the "Scaffolded — Counter AC3 Mode" table row overstates its liveness).

## Evidence Index
Mode/flags: PairedAnimationComponent.h:599-624. AC3: comp:3992-4327. Chain entry/redirect: comp:1502-1694, 3659-3674. Stage start/preflight: comp:2846-3583. Data resolution: comp:4365-4428. Damage: comp:1217, 3922-3930, 5090-5131. Inputs: CombatComponent.cpp:1421-1463. Markers: AnimNotify_ChainStageTransition.cpp:80-99, comp:425-602. Content: AM_Counter_* exports (MW notify + SkewWarp "PairedTarget"), AS_Block_Hit_Break_Seq / AS_Hit_Large_F_Seq (`bEnableRootMotion=true`).
