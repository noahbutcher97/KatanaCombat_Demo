# Combat Stabilization PIE Checkpoints

Use this sheet to judge only the behavior delivered by the named checkpoint. Before PIE, run `Combat.Debug.All 1`, `Combat.Defense.Debug 1`, `Combat.ActionReaction.Debug 1`, `Combat.Defense.ClearTelemetry`, and `Combat.ActionReaction.ClearTelemetry`. Before stopping PIE, run `Combat.Defense.DumpTelemetry Saved/Logs/DefenseTelemetry/<run-name>.csv` and `Combat.ActionReaction.DumpTelemetry Saved/Logs/ActionReactionTelemetry/<run-name>.csv`; component-owned rings disappear during PIE teardown. Retain `Saved/Logs/KatanaCombat.log`, and do not save either test map unless the asset change is intentional. Record branch HEAD, map, observed participant count, duration, result, exact input sequence, and a short video or timestamp for every run.

## Diagnostic Capture

Reproduce one anomaly per named run. Start from idle, clear both rings, perform the shortest known input sequence, wait long enough to observe recovery, dump both CSVs, and only then stop PIE. The action/reaction stream currently covers input capture/finalization, queue decisions, action start/finish, phase/context, hold and movement-lock transitions, montage callbacks, terminal reset, and paired-stage marker/start outcomes. It does not yet prove orbit requests, AI-token ownership, reaction arbitration, alignment error, or animation-lane selection; those emitters remain with their owning micro-plans.

## Checkpoint 1: Attack Lifecycle

**Run after:** Micro-Plan 01 is automation-green and marked ready in the execution handoff.

**Status:** Partial manual evidence. The latest extended `Lvl_DefenseMatrix` run completed without a crash and the user visually accepted rotation, but no `Lvl_ThirdPerson1` run or telemetry CSV was retained. Terminal ownership, repeated post-interruption recovery, and parry classification remain open acceptance items.

**Maps:** `Lvl_DefenseMatrix` with three observed enemies for five uninterrupted minutes, then `Lvl_ThirdPerson1` with four observed enemies for repeated attack/parry cycles. Record the live count rather than assuming fixture population.

**Actions:**

1. Let enemies complete several attack/recovery cycles while standing still.
2. Move inside and outside attack range, then remain inside range for repeated cycles.
3. Block and attack during enemy windup. Attempt perfect parry in both maps; use telemetry to distinguish input, cone, timing, policy, and presentation outcomes. The latest DefenseMatrix capture confirms attacker parry windows can open.
4. Kill one enemy during or adjacent to montage transition while other enemies remain active.
5. Continue fighting after every interruption; do not restart PIE to recover state.

**Should be fixed:**

- No access violation, frozen attack pose, or permanent enemy attack stall.
- Only the configured token budget attacks concurrently; released tokens continue to advance queued enemies.
- Enemies can recover and attack again without requiring an artificial range exit/re-entry.
- Interrupted, consumed, completed, and lethal attacks terminate once; a dead enemy cannot re-enter combat or repeat a paired finish.
- A lethal montage-stop callback cannot restart an attack montage or access an action entry after death clears the queue.

**Not fixed yet:** orbit jitter, targetless and live-input steering proof, asymmetric input lockout, hold replacement, cancel-window policy, reaction/trade policy, guard/parry animation readability, and any parry timing/tuning defect not yet classified by telemetry.

## Checkpoint 1A: Rotation And Terminal-State Blocker

**Run after:** the focused alignment, combo-race, targeting, defense-alignment, and debug-configuration suites and the full `680/680` no-UBA baseline are green.

**Maps:** `Lvl_DefenseMatrix`, then `Lvl_ThirdPerson1`. Do not save either map or `BP_Player`.

**Latest evidence:** Rotation is visually accepted in an extended `Lvl_DefenseMatrix` session on the current branch. The retained pre-telemetry log contains seven DefenseMatrix sessions, no crash, and no ThirdPerson session. Treat each unchecked line below as open until a named scenario and actor-qualified telemetry prove it.

**Actions:**

1. Enable `Combat.Debug.All 1`; remain idle for ten seconds in each map.
2. Attack selected enemies from roughly 45, 90, 135, and 180-degree offsets.
3. During a combo, move laterally around the attacker and confirm each attack continues updating toward the moving target.
4. Move away from all targets, hold a movement direction, attack, then release or change movement before the queued attack executes.
5. Let attacks and guard animations end normally, then immediately test movement, light, heavy, evade, and block again.

**Should be fixed:**

- The full combat HUD remains visible while idle in both maps; active-action shapes may still appear only while their owner exists.
- Player locomotion turns responsively at the independent 540-degree default.
- Attack alignment is no longer capped by the 180-degree defense rate or 70-degree defense budget.
- Targeted attack rotation continues to follow a moving target through each combo attack; targetless queued attacks retain their original world-space edge intent.
- Exact-opposite attacks choose a stable positive turn and do not stall.
- A normally ended montage returns phase to `None` and clears its queue. The queue overlay labels future windows as `opens in`, active windows as `remaining`, and stale-expired state explicitly instead of presenting a future checkpoint as an 11-second active window.

**Not fixed yet:** the legacy `bComboWindowActive` flag still represents a discovered checkpoint rather than only its live interval; configurable live steering influence during a warp, strict reachability/deadline rejection, final actor-yaw telemetry at contact, orbit jitter, cancel windows, reaction/trade behavior, and guard/parry presentation also remain open. Current modified Gate A paired assets also fail the no-save manifest audit and the automatic counter-to-finisher proof; do not classify that transition as a source regression until the asset graph is reconciled.

## Checkpoint 1B: One-Anomaly Telemetry Capture

**Run after:** commits `3fcc5efc` and `e1f30236`. This checkpoint diagnoses behavior; it does not claim the input/hold policy is fixed.

**Run 1 - movement-only lockout:** In `Lvl_DefenseMatrix`, start idle, clear both rings, hold Light until the freeze begins, release it, then immediately alternate movement and one Light press for five seconds. Stop inputs, wait five seconds, and dump to `dm-movement-lock-defense.csv` and `dm-movement-lock-actions.csv` before ending PIE.

**Run 2 - committed-hold replacement:** Restart PIE, clear both rings, hold Light until visibly frozen, then press Light exactly once while continuing to hold the original input. Do not add other inputs. Dump to `dm-hold-replacement-defense.csv` and `dm-hold-replacement-actions.csv` before ending PIE.

**Run 3 - map comparison:** In `Lvl_ThirdPerson1`, repeat only the shorter sequence that reproduced a bug above and dump with the `tp1-` prefix. If neither sequence reproduces there, record that result without broad exploratory combat.

**Evidence expected:** every physical edge has one `InputCaptured` and one reason-coded `InputFinalized`; a hold activation and release share one hold generation while retaining their press/release serials; each movement disable has a later restore; every accepted queue entry has one terminal execution or cancellation; montage callbacks identify accepted versus stale generations. Record the player actor path and the first sequence number where visible behavior diverges.

**Still open after capture:** the policy fix itself, one-slot arbitration, committed-hold protection, movement/guard cancel windows, orbit, reaction selection, parry readability, and Gate A asset reconciliation.

## Checkpoint 1C: Finisher Ownership And Terminal Outcome

**Run after:** `KatanaCombat.CombatInput` and `KatanaCombat.PairedAnimation` are green and every full-root failure is classified. Stop for any unclassified source or lifecycle failure. The current baseline is 732 passing tests plus seven known asset-gated failures: `KatanaCombat.Defense.GateA.PIEProof` and six DefenseAuthoring/DefenseMatrixAuthoring migration-contract tests blocked by the edited Gate A asset graph.

**Latest evidence:** `dm-finisher-terminal-actions.csv` was captured in `Lvl_ThirdPerson1` with four enemies. It confirms executor movement becomes allowed immediately after paired completion. The matching log exposed a separate failure: damage waited for attacker montage completion, allowing the paired victim to receive a token and interrupt its victim montage before death.

**Maps:** Run `Lvl_DefenseMatrix`, then `Lvl_ThirdPerson1`. Clear both telemetry rings immediately before one player-executed lethal finisher. During the paired sequence, hold movement and press Light once; after completion, release and press movement plus Light again. Dump each map to a correctly prefixed action telemetry file before ending PIE.

**Should be fixed:**

- Movement and attack input are rejected only while either paired role owns the sequence; ordinary and directional attacks cannot interrupt it.
- The executor transitions from `MovementInputSuppressedByPaired` to `MovementInputAllowed` at completion without requiring a directional hold, range exit, or PIE restart.
- At the primary `bApplyDamage` sync point, lethal damage immediately enters `Dying` once. The victim cannot move, queue/acquire a token, or start an attack during the paired sequence; ragdoll or freeze presentation may finalize when the victim montage or paired cleanup ends.
- `KatanaCombat.log` contains one `[PAIRED DAMAGE]` commit before `[PAIRED COMPLETE]`; a completion-fallback warning is a content-authoring failure for these reviewed finisher montages.
- Expected victim death does not cancel the owner sequence, discard the pending death outcome, restore a terminal movement baseline, snap the victim back into active play, or leave a paired notify lease behind.

**Record as a failure:** any post-finisher movement lock; victim movement, token grant, or attack after paired takeover; health remaining above zero after the impact notify; a completion-fallback warning; repeated death/finisher triggering; or `MovementInputSuppressedByPaired` without a later allowed decision.

## Checkpoint 1D: AI Target Lifecycle During Paired Sequences

**Automated gate:** no-UBA editor build; `KatanaCombat.EnemyAI.TargetLifecycle` `11/11`; `KatanaCombat.Defense.Chain.NoMontageBystanderTargetLifecycle` `1/1`; `KatanaCombat.EnemyAI` `54/54`; `KatanaCombat.PairedAnimation` `51/51`; and `KatanaCombat.DeathSystem` `14/14`. The full run must be `732/739`, with only the seven classified asset-gated failures above.

**Maps:** `Lvl_ThirdPerson1` with four observed enemies, then `Lvl_DefenseMatrix` with three. Keep at least two bystanders in combat range before starting a paired sequence.

**Actions:**

1. Wait until one enemy holds an attack token and another is visibly waiting or circling, then execute a player finisher without leaving combat range.
2. During the finisher, attempt movement and one attack input while watching every bystander. Continue for five seconds after paired completion.
3. Repeat through an enemy-executed paired sequence against the player when the authored combat state permits it.
4. Repeat one nonlethal counter or cancelled paired sequence and verify surviving actors resume against the retained target without a range exit/re-entry.
5. Dump both telemetry rings before stopping PIE and retain the matching `KatanaCombat.log` token and `[EnemyAI]` lifecycle lines.

**Should be fixed:**

- A paired executor or victim cannot be newly selected for a normal AI attack.
- Existing attackers immediately stop movement and release active or queued token ownership when their target enters either paired role; no later StateTree tick is required.
- Only the exact paired-sequence owner may retain its authored victim through expected terminal damage. Other tracked partners receive no exemption.
- A surviving, temporarily paired target becomes actionable again after cleanup. A `Dying` or `Dead` target is cleared and cannot be approached, circled, granted a token, or attacked.
- Pair completion cannot overwrite an already consumed or completed StateTree attack result, and token release or attack-end callbacks occur once.

**Record as a failure:** any bystander montage start, approach, token grant, or queue retention during paired ownership; any attack against a corpse; failure to resume against a surviving target; range-exit dependence; repeated token release; or the paired owner losing its victim before the authored sequence completes.

## Checkpoint 2: Alignment And Orbit

**Run after:** Micro-Plans 02 and 03 have separate green commits.

**Maps:** `Lvl_DefenseMatrix` and `Lvl_ThirdPerson1`.

**Actions:** attack with and without a target at 45, 90, 135, and 180 degree input offsets; steer during an active warped turn; then observe four enemies circling for five minutes around obstacles and while the player moves.

**Should be fixed:**

- Accepted attacks reach their authored facing deadline without direct rotation snaps.
- Targetless attacks use captured input direction; active player input produces bounded, smooth, per-attack steering.
- Enemies orbit within the configured annulus, face the player, and avoid back-and-forth path-request jitter.
- Locomotion rotation is responsive outside attack-owned alignment and ownership settings restore after each task.

**Not fixed yet:** hold commitment, reason-coded input lockouts, movement/guard cancellation, reaction severity and trades, additive flinches, and final guard/parry presentation.

## Checkpoint 3: Input And Action Arbitration

**Run after:** Micro-Plans 04A and 04B are green, including reviewed cancel-window assets.

**Maps:** both test levels with `Combat.Debug.All 1`.

**Actions:** exercise tap attacks, light hold and every directional follow-up, queued attacks, movement and guard during closed/open attack windows, and movement/guard during eligible reactions.

**Should be fixed:**

- Movement and attack input remain available unless a reason-coded policy decision rejects or queues them.
- A committed hold/follow-up cannot be replaced by an ordinary buffered attack.
- The normal attack buffer is one-slot, deterministic, and starts at most one action per boundary.
- Movement and guard cancel only in authored windows, preserve exact action ownership, and blend without a pose or root-motion snap.

**Not fixed yet:** final damage-magnitude reaction selection, active-phase trade behavior, additive body-part flinches, heavy full-body interruption tuning, and final layered guard/parry assets.

## Checkpoint 4: Reactions And Animation

**Run after:** Micro-Plans 05 and 06 are green and all migrated assets pass validation.

**Actions:** create light active-phase trades, windup/recovery interrupts, heavy interrupts, normal blocks, perfect parries, moving guard, lethal hits, and explicit finisher-vulnerable staggers.

**Should be fixed:**

- Health commits once before one deterministic reaction decision.
- Eligible trades preserve attacks with directional additive flinches; stronger hits use owned full-body interruption and release AI tokens exactly once.
- Moving guard does not slide, normal block and perfect parry are visually distinct, and reaction selection covers height/direction without weapon-socket drift.
- Ordinary stun does not grant finisher eligibility; lethal and paired ownership remain terminal.

**Not fixed yet:** only defects explicitly recorded in the execution handoff. Do not carry an unexplained gameplay defect into final acceptance.

## Final Acceptance

After Micro-Plan 07, repeat the core scenarios in both levels, restart the Editor, and repeat them again. Merge remains blocked by any crash, frozen pose, unexplained input lockout, unauthorized hold replacement, token imbalance, path churn, unaligned accepted attack, steering leak, guard sliding, indistinct parry, or unintended asset save.

Record each run as: `HEAD | map | duration | pass/fail | log path | video/timestamps | observed deviations`.
