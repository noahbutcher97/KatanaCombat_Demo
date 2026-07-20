# Combat Stabilization PIE Checkpoints

Use this sheet to judge only the behavior delivered by the named checkpoint. Before PIE, run `Combat.Debug.All 1`, `Combat.Defense.Debug 1`, and `Combat.Defense.ClearTelemetry`. Once action-reaction telemetry lands, also run `Combat.ActionReaction.Debug 1` and `Combat.ActionReaction.ClearTelemetry`. Before stopping PIE, dump defense telemetry to `Saved/Logs/DefenseTelemetry/<run-name>.csv` and action-reaction telemetry to `Saved/Logs/ActionReactionTelemetry/<run-name>.csv`. Retain `Saved/Logs/KatanaCombat.log`, and do not save either test map unless the asset change is intentional. Record branch HEAD, map, observed participant count, duration, result, and a short video or timestamp for every run.

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

**Latest evidence:** Rotation is visually accepted in `Lvl_DefenseMatrix` at commit ancestry `d7cf5001` plus the uncommitted lifecycle/alignment slice. The retained log contains seven DefenseMatrix sessions, no crash, and no ThirdPerson session. Treat each unchecked line below as open until a named scenario and actor-qualified telemetry prove it.

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
- A normally ended montage returns phase to `None`, clears the combo indicator/queue, and never displays an 11-second false combo countdown or leaves guard-looking input lockout.

**Not fixed yet:** configurable live steering influence during a warp, strict reachability/deadline rejection, final actor-yaw telemetry at contact, orbit jitter, asymmetric input policy, hold replacement, cancel windows, reaction/trade behavior, and guard/parry presentation.

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
