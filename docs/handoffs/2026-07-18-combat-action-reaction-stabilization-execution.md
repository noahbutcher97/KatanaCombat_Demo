# Combat Action And Reaction Stabilization Execution

Updated: 2026-07-19

## Current State

- Branch: `codex/combat-action-reaction-stabilization`
- Branch point: `47ef5723 Merge pull request #122 from noahbutcher97/codex/defense-interaction-design`
- Planning baseline: pending docs-only commit
- Implementation status: Pre-implementation readiness gate
- Active micro-plan: None
- Authority: `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization-design.md`
- Master plan: `docs/superpowers/plans/2026-07-18-combat-action-reaction-stabilization.md`
- Reconciliation status: source/full-debug PIE trace and read-only live-steering precedent audit complete; alignment and input/hold plans updated

## New Playtest Evidence Requiring Reconciliation

- Movement can stop accepting input while attack input remains usable.
- Attack input can stop producing attacks while movement remains usable.
- A committed light-attack hold/freeze and its directional follow-up can be replaced by an ordinary attack at apparently arbitrary progress.
- With no attack target, a player attack does not visibly complete rotation toward movement input.
- During an active motion-warped turn, current player movement input must retain bounded, per-attack influence over rotation.

Source tracing confirmed multiple mechanisms behind the observations:

- light-hold easing calls `DisableMovement()` while attack routing remains active;
- Recovery-phase attack input executes immediately, and montage startup clears the hold;
- duplicate queued attack types are rejected while phase processing can select newest-first and start multiple entries;
- hold cleanup can force `MOVE_Walking` despite other movement-mode owners;
- Move and combat actions omit relevant Enhanced Input terminal bindings, allowing stale direction/held state.
- normal queue entries discard the captured direction and attack warping rereads mutable movement input at execution;
- regular attack rotation is capped to defense's 180-degree/second rate and 70-degree budget even though AttackData authors 720 degrees/second.
- targeted warp refresh hard-faces the selected actor; targetless warp has no terminal-aware live steering, and Move has no terminal clear.

Micro-Spec/Micro-Plan 04A now separates capture, policy, application, and visible result; defines one normal pending slot; gives hold/follow-up exact-generation ownership; and removes ordinary hold suppression from CharacterMovement-mode ownership. Micro-Spec/Micro-Plan 02 now separates immutable attack-edge facing from terminal-aware live steering, defines four bounded per-attack policies, retains exact alignment ownership, and requires attack-owned rate/window reachability plus actual actor-yaw proof. Runtime reproduction is complete; implementation proof remains outstanding.

The rotated capture at `Saved/Logs/KatanaCombat-backup-2026.07.19-14.49.54.log` adds bounded runtime evidence:

- the crash is directly attributed to `UEnemyCombatAIComponent::ExecuteAttack()` line 278 through `FStateTreeExecuteEnemyAttackTask::EnterState()`;
- Heavy and Light resolve during the same source attack and both montage checkpoint sets appear at one phase boundary, strongly corroborating the source multi-start risk;
- normal Light resolves while `DA_DirectionalAttack_F` is current, confirming that ordinary input is accepted during a directional follow-up;
- no input, queue, phase, montage, movement, or hold transition diagnostics were enabled, so individual asymmetric lockouts remain unproven.

Checkpoint records lack actor and generation identity. Treat the multi-start sequence as corroboration until a deterministic regression and owner-qualified PIE trace prove it.

The later full-debug capture preserved at `Saved/Logs/KatanaCombat-backup-2026.07.19-15.23.35.log` closes the principal reproduction gaps:

- lines 2643-2726 show a queued Light clearing a hold immediately after the hold activates at Recovery;
- 119 same-type inputs are rejected, including a queued entry retained from 8598 until terminal cleanup at 11583;
- eight movement-disable/enable pairs confirm movement-only suppression while combat input remains routable, without proving a leaked movement mode in this run;
- seven owner-qualified player rotation-only warp requests reach the notify, but character yaw advances only 12-24 degrees toward several 90-135-degree requests before replacement/interruption;
- existing tests stop at request/modifier setup and do not prove final actor yaw.

A read-only `DefenseProofMigration` audit completed with `-DDC-ForceMemoryCache` and wrote `Saved/Logs/Commandlets/KatanaAssetMigration/action-reaction-current-defense-audit.json`: 27 targets were unchanged, with zero changes, failures, or saves. The preserved montage/map/external-actor hashes remained identical. Unreal still emitted a residual DDC warning during shutdown, and this defense-manifest audit does not inspect attack warp windows or first-contact timing; Micro-Plan 02's dedicated attack-alignment audit remains required.

## Preserved User WIP

```text
 M Content/ProjectFiles/Animation/Montages/Katana/Light/AM_Light_Combo_1.uasset
 M Content/ProjectFiles/Levels/Test/Lvl_DefenseMatrix.umap
 M Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/6/69/JZK42Z6X6VW6A5NALDH47D.uasset
 M Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/6/JS/VGKO8NKW281LCY3A3K4ETM.uasset
 M Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/7/CB/19YS22F55XPIYEV7ASGWJO.uasset
 M Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/A/1B/Y3W89WVL6CTN8GGMOAELDW.uasset
?? Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/0/DE/
?? Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/E/RI/
```

These user montage/map/external-actor changes are outside the planning work. The montage became modified during the later Editor session; do not revert, resave, stage, or include any listed package in an asset migration allowlist until the user classifies it.

Planning-time SHA-256 baseline:

```text
274CC25613CA6A41A5F3D9AB4A24A6B635DD80CA395874CD0C00DEC35C28FC52  Content/ProjectFiles/Animation/Montages/Katana/Light/AM_Light_Combo_1.uasset
C1E51C2DE9C17E8D0066BD37A58DDF363BD768E21440C5E64431F6DDEDE604E7  Content/ProjectFiles/Levels/Test/Lvl_DefenseMatrix.umap
7788723149C59AB3D1AF9B29D3F1CB8EED1031D5094657C28DC8599F86C55973  Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/6/69/JZK42Z6X6VW6A5NALDH47D.uasset
78BDA0169BBD7E23A91FAE9D3696E406909DF1759BF376C54B0CAF6E33EF3CCB  Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/6/JS/VGKO8NKW281LCY3A3K4ETM.uasset
6E12F9893337B3A56018B8B7E38D03184D9F2845B82573E3E629D8AA3772D8AD  Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/7/CB/19YS22F55XPIYEV7ASGWJO.uasset
EB68E3E1FAAB045D43D1304F37D0C35D7014281F98D7895D700CEDE4FD9DD729  Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/A/1B/Y3W89WVL6CTN8GGMOAELDW.uasset
7829AF66CCFA1483ECF37FA89EA51C56EEF97095A88D5FD73805177FAB189013  Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/0/DE/V3JZ16UNMRALF346YOZDKZ.uasset
F8030B262B81B7268C5C79AA3DE56130FCD821496C96F54BCD75E3817BA0E1D2  Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/E/RI/CB7I57MGAN2O7R2ODC677A.uasset
```

## Planning Evidence

- Current combat/AI/reaction source and canonical defense/paired architecture were refreshed before planning.
- Relevant behavior was checked against official Epic documentation and installed UE 5.6 source.
- OperationPhoenix/OnSight was inspected read-only as a comparative steering precedent. Its pure angular policies informed the design; its GAS/network lifecycle, multi-source input polling, hard-target bypass, disabled refresh path, and source-shape proof were explicitly rejected.
- Eight micro-specs and eight dependency-ordered micro-plans exist, with input/hold work isolated as 04A before action/cancel work in 04B.
- The pre-implementation hostile review is recorded in `docs/superpowers/plans/2026-07-18-combat-action-reaction-stabilization/ADVERSARIAL_AUDIT.md`.
- No runtime source, Blueprint, map, or asset was changed during planning.

## Readiness Audit Closure

- Exact antipodal behavior is fixed to a positive-yaw tie-break inside 0.1 degrees with a bounded tolerance-safe bias.
- `FAttackWarpConfig` owns `FinalFacingTolerance = 10.0f`; antipodal bias, preflight, and acceptance consume that one value.
- Immutable edge intent owns startup/queue determinism; a separate live world-input sample may offset only the exact regular player attack's rotation target.
- Steering is closed to `Disabled`, `Weighted`, `DeadZoneCurve`, and `ConeClamp`, with response, deviation, time/rate/budget, and return-reserve limits.
- Steering cannot change target identity, translation, montage/branch, AI, paired/counter-sync, or defense alignment.
- UE 5.6 modifier update order was verified; the plan updates the exact registered modifier before its target is sampled and suppresses noisy target churn.
- Strict player alignment failure records `Rejected/UnreachableAlignment` before montage start; supported production angles must pass asset validation.
- Effective damage deadline is the earliest canonical Active/Hit begin or earlier surviving legacy hit-enable notify.
- The intentional red crash reproduction runs alone in a disposable command-line Editor process.
- Initiating symptoms now map to micro-plans, regressions, and runtime proof in `ADVERSARIAL_AUDIT.md`.
- Planning must be committed alone before the build/test baseline or runtime edits.

## Required Next Action

Commit the reconciled planning package alone, recheck preserved WIP hashes, run `Tools/Codex/run-agent-baseline.ps1`, and record the result. If green, execute Micro-Plan 01's isolated failing reentrant enemy-attack regression before changing lifecycle behavior. Micro-Plan 04A must land before 04B.

## Slice Log

Add one entry per micro-plan containing commit, changed files/assets, red/green tests, build log, validator/PIE evidence, adversarial findings, proof limits, and exact next action. Verify every entry against live state after compaction or session change.
