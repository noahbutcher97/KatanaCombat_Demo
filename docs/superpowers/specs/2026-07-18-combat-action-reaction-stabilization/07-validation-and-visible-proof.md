# Micro-Spec 07: Validation And Visible Proof

## Purpose

Define the evidence required to accept the stabilization work without confusing source inspection, automation, asset structure, and visible gameplay quality.

## Precedent

Epic's [Automation Test Framework documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine) distinguishes unit, feature, and content-stress tests and requires tests to leave disk state unchanged. Epic's [Data Validation documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-validation-in-unreal-engine) supports `IsDataValid`, editor validators, and commandlet execution.

## Evidence Tiers

| Tier | Can prove | Cannot prove alone |
|---|---|---|
| Source/static | ownership, prohibited calls, dependency direction | runtime timing or visual quality |
| Pure automation | matrices, thresholds, identity, fallback, deterministic policy | actual montage graph behavior |
| World/component automation | lifecycle, delegates, token cleanup, StateTree adapters | human-visible smoothness |
| Asset validation | references, slots, root motion, notify structure, allowlists | pose quality or motion continuity |
| Editor/PIE telemetry | runtime ordering, final yaw, path churn, slot activity, socket drift | subjective quality without capture/review |
| Video/playtest review | sliding, jitter, snap, readability, combat feel | exhaustive edge-case correctness |

No tier may be reported as a stronger tier.

The bounded telemetry contract may be implemented early after lifecycle and blocker-alignment stabilization when exploratory PIE lacks actor/generation identity. This exception is observational only: it may record, clear, and serialize existing decisions, but cannot alter routing, timing, arbitration, movement, animation, or damage behavior.

## Proof Fixture

Do not resave the user's dirty maps. Add a C++ `ActionReactionProofDirector` that can be spawned in PIE and configures existing actors transiently. The director owns no production gameplay decisions. It runs named scenarios, emits structured telemetry, restores runtime-only overrides, and destroys itself cleanly.

Required scenarios:

1. repeated AI attack start/consume/death reentrancy;
2. player and AI attack alignment across representative yaw and play-rate cases, including isolated targetless 45/90/135/180-degree input, terminal-zero/no-input facing preservation, invalid-target fallback, queued-intent retention, and targeted/targetless live steering with hold/reverse/release/late-input cases;
3. three-enemy `Lvl_DefenseMatrix` and four-enemy `Lvl_ThirdPerson1` approach/orbit/token behavior, recording the discovered participant count rather than assuming fixture population;
4. input availability and hold commitment across movement, Light/Heavy, release, canceled input, direction/no-direction, damage, and paired entry;
5. attack and full-body-reaction cancel windows for movement and guard;
6. guard locomotion, normal block, and distinct perfect parry;
7. additive trade while both attacks continue;
8. heavy full-body interrupt and exact token release;
9. stagger finisher eligibility versus ordinary action lock;
10. lethal hit during attack/reaction/paired-adjacent states;
11. nine height/lane reaction selection cells.

## Verification Ladder

For each micro-slice:

1. Run context refresh and classify dirty WIP.
2. Add and observe a focused failing test.
3. Build `KatanaCombatEditor Win64 Development`.
4. Run focused automation roots.
5. Run slice-specific validators or commandlet audit.
6. Review complete diff and run the slice adversarial checklist.
7. Commit only that slice.

Before branch acceptance:

1. Run `Tools/Codex/run-agent-baseline.ps1`.
2. Run the full `KatanaCombat` automation suite and summarize the latest log.
3. Run data validation and content migration audit in no-save mode.
4. Run all PIE proof scenarios in `Lvl_DefenseMatrix` and `Lvl_ThirdPerson1` without saving either map.
   Enable `Combat.Debug.All 1`, `Combat.Defense.Debug 1`, and `Combat.ActionReaction.Debug 1`; clear both telemetry buffers before each run and dump both before PIE stops.
5. Capture telemetry plus short video for visible cases.
6. Re-run after restarting the Editor to catch stale transient state.
7. Inspect `git diff --check`, full diff, LFS pointers, and `git status --short`.

## Required Telemetry

- scenario, map, discovered participant count, enabled debug channels, actor, attack/primary-action generation, montage instance/source, and queue-entry identity;
- physical input capture, policy decision, application/result, intent route, hold phase/generation, cancel decision, and reason;
- reaction class, pressure, resistance, impact policy, and interruption result;
- AI token acquire/release reason and count;
- attack alignment facing source, stable target/translation identity, captured base intent/provenance, live input serial/world direction, steering mode/raw/clamped offset, response/reachability limit reason, derived requested/published/applied yaw, antipodal sign, rate, budget, first-contact deadline, release reason, and final error at contact/end;
- orbit destination, projection, move result, replacement reason, speed mode, and radial error;
- active animation lanes, selected presentation key/fallback, and weapon socket drift;
- terminal/death precedence and stale-callback rejection.

## Documentation Reconciliation

Target documents must not describe unimplemented work as current behavior. After each accepted slice, update only the corresponding sections of `CLAUDE.md`, `docs/architecture/ARCHITECTURE_QUICK.md`, test README, and relevant specs. Final reconciliation classifies every target contract as `Proven`, `Partial`, `Not Implemented`, or `Out Of Scope`; in-scope `Partial` or `Not Implemented` blocks merge.

## Acceptance

- Full baseline and automation are green with current logs.
- All validators pass and no unintended package is saved.
- Every scenario has the evidence tier required by its claim.
- No captured input or movement-policy change lacks a terminal, reason-coded disposition, and no committed hold is replaced without a matching cancellation record.
- No high/medium adversarial finding remains open.
- User map and external-actor WIP remains byte-for-byte untouched by this branch.
