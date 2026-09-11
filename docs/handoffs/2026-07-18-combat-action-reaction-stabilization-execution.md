# Combat Action And Reaction Stabilization Execution

Updated: 2026-09-11

## Current State

- Latest baseline after the shared-suite handoff: [September 11 combat verification](../audits/COMBAT_BASELINE_2026-09-11.md). Standard editor build and full headless automation pass: 778/778 unique successful results, zero automation failures/errors and exit 0. The rendered-only surface control explicitly defers its GPU check under NullRHI; visual acceptance remains separate. Source/config/dependency identity is unchanged, as are all 7,948 Content metadata records and 54 protected asset hashes/absences. Six generated PNGs were archived, verified and removed. Shared AnimationAnalysis development proceeds separately; Katana remains pinned to `ba13149d3318f80d3098958cd1d2cd52bba3e5d1`. Next Katana work: review unwarped source-pair facing, grip and intended contact before selecting finisher authoring changes, using the existing preview/evaluator and visual evidence.

- Latest correction: [paired warp tuning and collision ownership](../audits/PAIRED_WARP_TUNING_2026-09-10.md). Opening lethal damage cleared the victim partner before its collision notify, allowing CharacterMovement capsule depenetration. Expected paired death now preserves that link until exact sequence cleanup; unrelated death still cancels. The new regression passes, the full suite passes 776/776, and four transient settings reduce early observed steps from roughly 20–24 cm to 2.7–6.2 cm after the fix. All four final rendered completed/interrupted cases pass on both maps, with 455 frames exported without loss; two native logging replays show zero victim–attacker depenetration corrections. Contact authoring remains unresolved; no assets are saved or quality reference promoted. Evidence: `Saved/Logs/PairedWarpTuning-20260910-223032/`. All 54 protected Content paths are unchanged. Cleanup recycled 451 PNGs and retained four new reviewed frames with explicit retention notices.

- Earlier correction: [paired warp grounding and upright facing](../audits/PAIRED_WARP_GROUNDING_2026-09-10.md). Native hits confirm player mesh/capsules were being used as ground. Paired targets now require walkable environment support and use upright yaw; rendered target-height variation and actor pitch are zero on both maps, with late steps below 1 cm. Editor build, 4 new red/green regressions, 773/773 full headless tests and 16/16 post-fix rendered cases pass; all 2,032 PNGs finish without loss. Victim translation-off reduces the separate early step from 15-17 cm to 2.68 cm; native logs confirm 2.5-2.7 cm input becomes 16-19 cm processed translation with the original modifier. All six contact comparisons remain measured failures. Changed only three runtime files and one new test file; all 54 protected Content paths remain unchanged. Next: transient victim warp-window/placement comparison plus attacker translation-policy reconciliation before selecting saved asset changes or touching contact/damage timing. Evidence: `Saved/Logs/PairedWarpGrounding-20260910-214854/`. Post-verification cleanup moved 10,697 bulk PNGs to the Windows Recycle Bin, retained 11 reviewed frames and consolidated 12 temporary scripts. Retention sidecars and report notices distinguish historical export results from the retained image subset.

- Earlier follow-up: [finisher warp isolation](../audits/FINISHER_WARP_ISOLATION_2026-09-10.md). Both translation-off controls retain substantial victim spikes. Target height varies by approximately 156 cm and drives target pitch near -72 degrees; native logging proves that rotation of the offset mesh can turn less than 0.6 cm local translation into tens of centimetres of world translation. Victim rotation-off retains translation warping, keeps actor pitch at zero and reduces the late-window step below 1 cm on both maps. An early approximately 15 cm step and contact/release failures remain. Final editor build, 9 paired tests, 31 offline tests, 16 rendered matrix cases and 2 native-log replays pass; all 2,087 PNGs finish without loss. Runtime source and all 54 protected Content paths remain unchanged. Next: identify the actual ground hit and correct ground eligibility/upright paired-target rotation, then inspect early translation before contact/timing authoring. No asset correction or quality reference is promoted.

- Earlier follow-up: [sync repair and movement experiment](../audits/FINISHER_MOVEMENT_EXPERIMENT_2026-09-10.md). Opening and overlapping sync states are now reported with preserved nominal/trigger clocks and explicit configured-damage semantics. The editor builds; 9/9 paired-evaluation tests, 30/30 offline tests and 8/8 rendered completed/interrupted control/experiment scenarios pass. Transient movement-permitting collision notifies restore approximately 38 cm attacker and 183–187 cm victim travel on both maps while tested input/cleanup behavior holds. The experiment exposes a 31–46 cm victim step near the 0.87 s warp-window end; provisional contact/release criteria still fail. All 904 PNGs finish without loss, original notify objects are restored and all 54 protected Content paths plus 223 runtime files remain unchanged. Next: isolate the warp discontinuity and attacker translation mismatch before contact/damage timing authoring. No asset correction or quality reference is promoted.

- Earlier follow-up: [finisher contact diagnosis](../audits/FINISHER_CONTACT_DIAGNOSIS_2026-09-10.md). Current source animations have root motion enabled, but both montages disable CharacterMovement across warp windows; existing captures show stationary actors after the opening 141.4-to-80 cm alignment snap. Damage commits at entry. The evaluator omitted the attacker's negative-offset start sync, and attacker translation settings disagree between pair config and warp modifier. Independent pose composition agrees with native blade-gap measurements within 0.00014 cm; four new placement controls still fail unchanged provisional criteria. No source or Content changed during that diagnostic pass. The subsequent experiment above supersedes its reporting defect and movement hypothesis with fresh results.

- Earlier follow-up: [capture sampling reliability](../audits/COMBAT_CAPTURE_SAMPLING_2026-09-10.md). Synchronous PNG work was the repeated stall; bounded background export preserves observation clocks and closes the five inconclusive hold captures. The affected matrix passes 8/8; thirteen rendered hold recordings finish all 1,095 PNGs without loss. Focused checks pass 28/28 headless, 10/10 rendered and 28/28 offline. Fresh completed finishers pass gameplay/capture on both maps; their blade/torso contact intervals have approximately 16.8 ms maximum pose gaps and still fail the provisional geometry with about 44.75 cm minimum separation. Runtime and Content WIP remain unchanged. The subsequent contact diagnosis above determines the corrective sequence.

- Earlier follow-up: [hold recovery and paired evaluation](../audits/PAIRED_ANIMATION_EVALUATION_2026-09-10.md). Full headless automation passes **765/765**, exit 0, including all four real-montage directional hold releases on both maps. Reusable contact profiles, explicit attached-weapon point sources, shared preview/commandlet sampling and bounded relative-root/timing analysis are implemented in the existing editor module. Fresh rendered completed-finisher scenarios pass on both maps, while their intended blade/torso proxy fails with roughly 45 cm minimum gameplay gap against the provisional 8 cm allowance. The counter exercises the same evaluator. Review those contact intervals and their intended geometry before changing assets; no quality reference has been promoted. The report separates pose-gap limitations and unverified desktop UI from passed gameplay/instrument checks. No Content or runtime implementation was changed by this follow-up.

- Capture follow-up: [reusable scenario evaluation](../audits/COMBAT_SCENARIO_EVALUATION_2026-09-09.md) adds a documented build/run/capture/evaluate command, completed/interrupted finisher input recovery with active bystanders on both maps, schema-2 pose/frame identity, fixed 960×540 rendering and selected mechanical references. Full headless automation passes **749/749**; twelve unchanged rendered scenario repeats, nine same-backend overhead runs and two rendered recorder/frame-limit integrations pass. Four deliberately displaced controls fail only the intended displacement assertion; four fresh removals pass the same references. All 54 protected Content paths are unchanged. New names describe behavior and require no local tooling context. Next: use this shared workflow for the remaining input/hold, steering/reachability and contact acceptance work; general feel scoring/plugin packaging remain deferred.
- Earlier September 9 post-repair verification on HEAD `6e170503b5958d84d4c065cfaf6e18704546df7a` plus existing WIP: editor build passed; full automation passed **739/739**, exit 0, using `-DDC-ForceMemoryCache`. Follow `docs/audits/DEFENSE_PROOF_REPAIR_2026-09-09.md` for repair evidence and proof limits. `COMBAT_HEALTH_RESUME_2026-09-09.md` retains the initial seven-failure baseline and diagnosis.
- Existing input/hold work substantially implements Micro-Plan 04A. CombatInput 16/16, EnemyAI 54/54, PairedAnimation 51/51, DeathSystem 14/14, and ActionReaction telemetry 14/14 passed in that full run. Complete acceptance and hostile review remain open; do not reimplement the uncommitted work from the unchecked July plan.
- The six authoring/approval failures are resolved by updating both manifests and Gate A recipe V5 to the current finisher authoring. The current finisher montages are required read-only recipe dependencies, bound to approval with their animations/skeletons; existing refusal/drift checks remain intact.
- Gate A's handoff is repaired by restoring `LightAttack_1.CounterData` to existing `DA_Counter_GateA`, whose participant assignments and marker policy match the counter montages. Exactly one package was saved/reloaded. Focused and full Gate A runs now pass all cases: automatic continuation, one lethal finisher damage event, one completed cleanup, and token release.
- Current finisher assets and edited `Counter_LightAttack_1` are preserved. [Rendered Gate A automation](../audits/AUTOMATED_ANIMATION_ACCEPTANCE_2026-09-09.md) subsequently passed with 47 captured images, while exposing cold-start readiness and stale/occluded capture gaps. Extend automated PIE coverage for Checkpoints 1C/1D and the full input/hold matrix; manual play is not a prerequisite. The earlier intermittent missing physical contact did not recur in either post-repair Gate A run; its cause remains open. Gate B's separate attack-variant/matrix-montage refresh proposals remain unapplied.

## Historical State - July 20

- Branch: `codex/combat-action-reaction-stabilization`
- Branch point: `47ef5723 Merge pull request #122 from noahbutcher97/codex/defense-interaction-design`
- Planning baseline: `3465d228 Plan combat action reaction stabilization`
- Harness hardening: `d7cf5001 Harden defense manifest adversarial test`
- Validation noise fix: `a46bbb76 Deduplicate attack-data cycle validation errors`
- Lifecycle/alignment blocker: `520be7c5 Stabilize combat lifecycle and attack alignment`
- Telemetry foundation: `3fcc5efc Add action reaction telemetry foundation`
- Telemetry unity-build fix: `e1f30236 Fix action telemetry unity build`
- Implementation status: Micro-Plan 01 and the Micro-Plan 02 blocker slice are automated green; the latest extended `Lvl_DefenseMatrix` run had no crash and the user visually accepted rotation; Micro-Plan 07's bounded telemetry foundation is focused-test green, while the next two-map diagnostic checkpoint and remaining behavior are still partial
- Active slice: capture one actor-qualified anomaly per PIE run, then use that evidence to enter Micro-Plan 04A without guessing at asymmetric input or hold ownership
- Authority: `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization-design.md`
- Master plan: `docs/superpowers/plans/2026-07-18-combat-action-reaction-stabilization.md`
- Reconciliation status: lifecycle closure and the alignment blocker slice are focused-test green; DefenseMatrix rotation has manual acceptance, while the current full suite has one asset-WIP-dependent Gate A failure and two-map lifecycle closure, live steering, strict reachability, migration, orbit, and input/hold policy remain planned

## Lifecycle Crash Hardening Evidence

The first runtime slice replaces mutable post-execution reads with snapshotted startup inputs and exact `FAttackInstanceId` ownership. StateTree execution now observes the identity returned by its own call, montage callbacks capture that same identity, and terminal cleanup is idempotent across synchronous consumption, death, replacement, duplicate callbacks, and EndPlay. Token removal now recognizes a pending-kill holder so owner destruction cannot strand capacity. Combat montage startup now stops when a callback makes the owner terminal, and event-driven queue processing removes and copies an entry before invoking callback-capable execution.

Changed source and tests:

```text
Source/KatanaCombat/Private/AI/CombatTokenSubsystem.cpp
Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp
Source/KatanaCombat/Private/AI/EnemyCombatStateTreeTasks.cpp
Source/KatanaCombat/Private/Core/CombatComponent.cpp
Source/KatanaCombat/Private/Debug/DefenseTelemetry.cpp
Source/KatanaCombat/Public/AI/CombatTokenSubsystem.h
Source/KatanaCombat/Public/AI/EnemyCombatAIComponent.h
Source/KatanaCombat/Public/AI/EnemyCombatStateTreeTasks.h
Source/KatanaCombatTest/Private/DefenseArchitectureSourceTests.cpp
Source/KatanaCombatTest/Private/DefenseTelemetryTests.cpp
Source/KatanaCombatTest/Private/EnemyCombatAITests.cpp
```

Deterministic red evidence was captured before the fix:

- `Saved/Logs/Codex-Red-ReentrantSelectionClear-20260719-153508.log`: exit `3`, access violation reading `0x50`.
- `Saved/Logs/Codex-Red-SynchronousConsume-20260719-154022.log`: exit `255`.
- `Saved/Logs/Codex-Red-AttackLifecycle-20260719-155328.log`: exit `255`, stale completion and participant-destruction failures.
- `Saved/Logs/Codex-Red-StateTransitionReentry-20260719-164715.log`: exit `255`, replacement ownership and token assertions failed.
- `Saved/Logs/Codex-Red-StateTreeAdoption-20260719-164740.log`: exit `255`, the task adopted an attack started by another invocation.
- `Saved/Logs/Codex-Red-BoundMontageInterruption-20260719-164931.log`: exit `255`, an interrupted production-bound montage callback reported success.
- `Saved/Logs/Codex-Red-AbortTokenReleaseReentry-20260719-171656.log`: exit `255`, outer abort overwrote replacement state.
- `Saved/Logs/Codex-Red-StartupDependencyInvalidation-20260719-171841.log`: exit `255`, startup invoked an invalidated CombatComponent.
- `Saved/Logs/Codex-Red-DirectGenerationReplacement-20260719-171807.log`: exit `255`, pre-commit replacement generation remained active without AI ownership.
- `Saved/Logs/Codex-Red-PendingTerminationTokenReleaseReentry-20260719-172303.log`: exit `255`, pending termination overwrote replacement state.
- `Saved/Logs/Codex-Red-AttackStartedDirectGenerationReplacement-20260719-172316.log`: exit `255`, post-start replacement generation remained active without AI ownership.
- `Saved/Logs/Codex-Red-LethalBlendCallback-PreviousState-20260720.log`: a synchronous lethal montage-stop callback left a dead owner able to restart its attack montage.
- `Saved/Logs/Codex-Red-QueuedLethalBlendCallback-BoundDeathReset-20260720.log`: exit `3`, event-driven queue execution called `RemoveAt(0)` after death cleanup emptied the queue.

Green evidence after hardening:

- No-UBA compile/link: `Build.bat ... -NoUBA -MaxParallelActions=1`, exit `0`.
- `Saved/Logs/Codex-Green-StateTransitionReentry-20260719-165250.log`: `1/1`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-Green-StateTreeAdoption-20260719-165313.log`: `1/1`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-Green-BoundMontageInterruption-20260719-165336.log`: `1/1`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-AttackStartup-FinalClosure-20260720-091912.log`: `9/9`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-AttackLifecycle-FinalClosure-20260720-091934.log`: `9/9`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-EnemyAI-FinalReviewClosure-20260720-092013.log`: `33/33`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-Defense-FinalReviewClosure-20260720-092029.log`: `139/139`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-Agent-Baseline-20260720-102629-automation.out.log`: `664/664`, zero failures/errors, explicit success marker, exit `0`.
- `Saved/Logs/Codex-EnemyAI-FinalLifecycleClosure-20260720-111326.log`: `39/39`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-Defense-PostLifecycleAudit-20260720-110349.log`: `139/139`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-Agent-Baseline-20260720-111400-automation.log`: `670/670`, zero failures/errors, explicit success marker, exit `0`; its `347` automation warnings match the prior baseline count.
- `Saved/Logs/Codex-Green-LethalBlendCallback-20260720.log`: `1/1`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-Green-QueuedLethalBlendCallback-20260720.log`: `1/1`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-Focused-DefenseTelemetry-20260720.log`: `4/4`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-Focused-EnemyAIAttackStartup-PostMontageGuard-20260720.log`: `11/11`, zero failures/errors, exit `0`.
- `Saved/Logs/Codex-Agent-Baseline-20260720-144041-automation.log`: `673/673`, zero failures/errors, explicit success marker, exit `0`.
- `Saved/Logs/Codex-Agent-Baseline-20260720-160153-automation.out.log`: `680/680`, zero failures/errors, explicit success marker, exit `0`; editor build also exited `0`.

The `680/680` lifecycle/alignment baseline includes `KatanaCombat.DeathSystem`, `KatanaCombat.CombatComponent.MemorySafety`, attack-alignment resolution, terminal montage cleanup, and default debug-HUD configuration. Its `351` automation warnings are unchanged from the `673/673` baseline. The automated Defense PIE gates passed at that content state, but post-fix interactive two-map Checkpoint 1A remains the runtime acceptance gate. Nine user-owned packages were preserved during that baseline; later Editor work expanded the current Content WIP to 34 status entries recorded in the evidence manifest below.

The reopened startup-grant, active-termination, result-retention, token-reset, target-clear, terminal montage-startup, and event-driven queue invalidation findings now have isolated red/green regressions. The `20260720-160153` run is the last fully green baseline before the current content drift; the current `695/695` completed baseline and its single Gate A failure are detailed below. Post-fix manual PIE remains a separate acceptance gate.

## PIE Verification Checkpoints

Use `docs/playtests/COMBAT_STABILIZATION_PIE_CHECKPOINTS.md` during every requested playtest. It separates lifecycle, alignment/orbit, input/arbitration, and reaction/animation gates, with explicit `Should be fixed` and `Not fixed yet` lists. The pre-fix run covered about 6:58 in `Lvl_DefenseMatrix` with three enemies and three sessions in `Lvl_ThirdPerson1` with four enemies. No crash occurred; DefenseMatrix recorded 80 token grants and 80 releases with a maximum of two active tokens, while ThirdPerson exercised all four enemies with a maximum of one active token. Because that run exposed the now-fixed post-death continuation path, Checkpoint 1 is only partial until repeated against the final code.

The post-alignment log is preserved at `Saved/Logs/PIE-20260720-rotation-checkpoint.log` (19,422,039 bytes). It contains seven `Lvl_DefenseMatrix` PIE sessions and no retained `Lvl_ThirdPerson1` session. The longest run lasted about 3:38 with 95 input presses, five input rejections, 12 balanced movement disable/enable pairs, and no crash. The user visually accepted attack rotation. Terminal cleanup, targetless intent, moving-target refresh, and parry remain partial because they were not individually recorded as named scenarios.

The same run exposed a diagnostic blocker: `[INPUT]`, `[PHASE]`, `[MONTAGE]`, and `[MOVEMENT]` lines omit actor and generation identity, so asymmetric lockouts cannot be assigned reliably. No defense telemetry CSV was dumped. The log also contained 7,302 duplicate combo-cycle validation errors for `LightAttack_1` and `LightAttack_2`; commit `a46bbb76` now deduplicates identical per-call errors with a focused red/green regression. It does not modify the assets or decide whether the authored cycle is valid.

## Action/Reaction Telemetry Foundation

`UCombatComponent` now owns a disabled-by-default 1,024-record ring. `Combat.ActionReaction.Debug` or `Combat.Debug.All` enables capture; `Combat.ActionReaction.ClearTelemetry` resets all runtime combatants; `Combat.ActionReaction.DumpTelemetry <path>` writes stable versioned CSV. Records snapshot actor paths and correlate input serial, queue entry, attack generation, hold generation, paired-stage generation, montage instance/source, route, disposition, state, and closed reason codes.

Current emitters cover physical input capture/finalization, queue accept/reject/cancel, execution start/finish, phase/context changes, hold lifecycle, combat-owned movement lock transitions, montage callback acceptance/rejection, terminal reset, and paired-stage marker/start outcomes. Paired marker telemetry preserves the outgoing stage snapshot even when the marker synchronously starts a successor. Reaction policy, AI token, orbit, alignment-error, animation-lane selection, scenario/map, and participant-count emitters remain owned by their later micro-plans or proof director; empty schema fields are not evidence for those domains.

Focused evidence:

- `Saved/Logs/ActionReactionTelemetry-FINAL-20260720.log`: `13/13`, zero failures, exit `0`; this includes exact queued and immediate-execution failure reasons plus cross-actor identity.
- `Saved/Logs/PairedMarkerSnapshot-Behavior-RED-20260720.log`: the new regression observed a counter marker mislabeled as generation `3 / FinisherActive` instead of outgoing generation `2 / CounterActive`.
- `Saved/Logs/PairedMarkerSnapshot-FINAL-20260720.log`: all `18/18` `KatanaCombat.Defense.Chain` tests pass after snapshotting the outgoing marker stage; marker role, montage instance/source/index, unrelated reporter, participant identity, stage-start rollback, and actual emitting montage are covered.
- `Saved/Logs/Codex-Agent-Baseline-20260720-200140-build.out.log`: the first committed-source baseline exposed an anonymous-namespace collision between the defense and action serializers under Unreal unity builds.
- `Saved/Logs/Codex-Agent-Baseline-20260720-200240-build.out.log`: the namespaced serializer rebuild exits `0` under the same unity-build lane.

The final source baseline summary at `Saved/Logs/Codex-Agent-Baseline-20260720-200240-automation-summary.json` discovered and completed `695/695` tests. Its only failed test is `KatanaCombat.Defense.GateA.PIEProof`; five downstream assertions from that test account for the seven failure/error lines and automation exit `255`. Read-only audits attribute that failure to current user-owned content drift: the Gate A manifest reports an attack/counter-reference mismatch plus undeclared `GhostSamurai_Ambush01` and `GhostSamurai_Ambushed01` dependencies, and the runtime marker reports the victim role while policy expects the attacker role. No asset was changed. Gate A automatic counter-to-finisher behavior remains an asset reconciliation gate, not evidence of a telemetry or rotation regression.

## New Playtest Evidence Requiring Reconciliation

- Movement can stop accepting input while attack input remains usable.
- Attack input can stop producing attacks while movement remains usable.
- A committed light-attack hold/freeze and its directional follow-up can be replaced by an ordinary attack at apparently arbitrary progress.
- With no attack target, a player attack does not visibly complete rotation toward movement input.
- During an active motion-warped turn, current player movement input must retain bounded, per-attack influence over rotation.
- No manual parry was observed. The latest full-debug DefenseMatrix capture does show attacker parry windows opening, so absence of authored windows is no longer an accepted explanation; input routing, cone/timing rejection, and presentation remain to be distinguished with telemetry.

Source tracing confirmed multiple mechanisms behind the observations:

- light-hold easing calls `DisableMovement()` while attack routing remains active;
- Recovery-phase attack input executes immediately, and montage startup clears the hold;
- duplicate queued attack types are rejected while phase processing can select newest-first and start multiple entries;
- hold cleanup can force `MOVE_Walking` despite other movement-mode owners;
- Move and combat actions omit relevant Enhanced Input terminal bindings, allowing stale direction/held state.
- normal queue entries discard the captured direction and attack warping rereads mutable movement input at execution;
- regular attack rotation is capped to defense's 180-degree/second rate and 70-degree budget even though AttackData authors 720 degrees/second.
- targeted warp refresh hard-faces the selected actor; targetless warp has no terminal-aware live steering, and Move has no terminal clear.

Micro-Spec/Micro-Plan 04A now separates capture, policy, application, and visible result; defines one normal pending slot; gives hold/follow-up exact-generation ownership; and removes ordinary hold suppression from CharacterMovement-mode ownership. The first Micro-Plan 02 blocker slice now separates attack rate/budget from defense, retains immutable queued world intent, refreshes a live target across replacement generations, handles exact/near-antipodal turns, and exposes 540-degree locomotion. Strict reachability, terminal-aware steering modes, migration, and contact-time actor-yaw acceptance remain outstanding.

DefenseMatrix-only debug loss was traced to map defaults rather than combat-action state. `Lvl_ThirdPerson1` explicitly uses `GM_KatanaCombat_Base`, whose HUD is `ACombatDebugHUD`; `Lvl_DefenseMatrix` had no override and the project default pointed to missing `GM_Samurai`. `Config/DefaultEngine.ini` now points the global default to `GM_KatanaCombat_Base`, guarded by `KatanaCombat.Debug.Configuration.DefaultGameModeProvidesCombatHUD`.

`Combat.Debug.All` now enables defense telemetry as expected, so the next parry attempt can distinguish input rejection, cone rejection, unavailable parry window, normal block, and successful parry. Parry timing or presentation is not part of Micro-Plan 01: diagnose it under Micro-Plan 07, then route a proven input/policy defect to 04B or a readability defect to 06. The deprecated polling `ProcessQueue(float)` path is currently unused and is scheduled for 04A cleanup; the active event-driven queue path was fixed here because it was a lifecycle blocker. A broader callback-boundary audit, including phase-change delegates, remains a later adversarial hardening item unless it blocks the post-fix checkpoint.

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

The current Content worktree has 24 modified, four deleted, and six untracked entries. Their exact paths and SHA-256 values are frozen in `docs/handoffs/evidence/2026-07-20-post-rotation-user-content-wip.md`. These Blueprint, montage, AttackData, paired-data, map, and external-actor changes are user-owned and excluded from source commits. Do not revert, resave, stage, migrate, or reinterpret them without explicit classification.

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

Use `docs/audits/PAIRED_WARP_TUNING_2026-09-10.md` for the current finisher state. Movement and collision ownership have fresh two-map proof; the bounded window/offset candidates still do not justify saved animation settings. Review authored relative facing/placement and intended blade contact before changing contact or damage timing. Keep sampled geometry failures separate from continuous-contact and artistic-quality claims. Continue remaining two-map steering/reachability acceptance with the reusable capture workflow. Separately review the existing defense-matrix regeneration proposals before any package saves. Preserve the existing input/hold implementation and completed lifecycle checks; do not reimplement them from the unchecked July plan.

## Slice Log

Add one entry per micro-plan containing commit, changed files/assets, red/green tests, build log, validator/PIE evidence, adversarial findings, proof limits, and exact next action. Verify every entry against live state after compaction or session change.

### Micro-Plan 01: Lifecycle Crash Hardening

- Commit: `520be7c5 Stabilize combat lifecycle and attack alignment` (shared blocker boundary with Micro-Plan 02 because the reentrant startup and queued-facing contracts meet in `CombatComponent`).
- Assets: none intentionally changed. All nine current pre-verification package hashes match after the full baseline; the Blueprint, montage, maps, and external actors remain excluded user WIP.
- Red/green: isolated access violation, terminal montage restart, and queue invalidation were reproduced; focused startup/telemetry tests and the full `680/680` no-UBA baseline are green.
- Adversarial closure: exact invocation identity, synchronous consumption, real lethal-damage delivery, stale/duplicate completion, production-bound montage interruption, reentrant token-release replacement, dependency invalidation, unowned generation rollback, participant destruction, pending-kill token release, terminal montage startup, event-driven queue invalidation, and EndPlay have focused regression coverage. StateTree non-adoption is additionally guarded by a source architecture test.
- Manual evidence: the latest extended DefenseMatrix run completed without a crash and rotation was visually accepted; the retained log contains no ThirdPerson run and no telemetry dump.
- Proof limit: automated Defense PIE gates are green, but the StateTree branch is not exercised through a directly instantiated task-context unit test. Two-map completion and a telemetry-backed `LightAttack_1` parry attempt are still pending.
- Next action: run one anomaly at a time with both telemetry rings cleared and dumped before PIE teardown.

### Micro-Plan 02: Alignment Blocker Slice

- Commit: `520be7c5 Stabilize combat lifecycle and attack alignment`.
- Source behavior: attack requests use attack-owned 720-degree rate, 180-degree cumulative budget, and 10-degree tolerance; queued attacks retain input-edge world intent; each replacement owns a new generation; moving targets refresh; exact and near-180 targets use the reviewed positive-yaw bias; player locomotion defaults to 540 degrees/second.
- Lifecycle hardening: normal montage end clears stale Active/combo state; repeated terminalization releases attack alignment idempotently; montage-end queue fallback removes/copies entries before callback-capable execution.
- Focused green: `KatanaCombat.AttackAlignment`, `KatanaCombat.ComboRaceCondition`, `KatanaCombat.Defense.Alignment`, `KatanaCombat.Targeting`, and `KatanaCombat.Debug.Configuration` all exit `0`.
- Full baseline: `Saved/Logs/Codex-Agent-Baseline-20260720-160153-automation.out.log` completed `680/680` with zero failures/errors and exit `0`; its no-UBA editor build also exited `0`.
- Manual evidence: the user accepted rotation in the latest extended DefenseMatrix session; this is visible proof for that map, not automated final-yaw or two-map proof.
- Proof limit: automation proves request ownership and the UE modifier's first antipodal yaw frame, not final actor yaw at first contact. Strict preflight, live input steering policies, asset migration, and final telemetry remain unchecked plan items.
- Next action: use the telemetry-backed checkpoint to classify remaining strict-reachability and live-steering behavior without reopening accepted base rotation.

### Micro-Plan 07: Early Telemetry Foundation

- Commits: `3fcc5efc Add action reaction telemetry foundation`; `e1f30236 Fix action telemetry unity build`.
- Assets: none changed or staged. The 34-entry user `Content/` lane remains excluded.
- Source behavior: each combatant owns a disabled-by-default 1,024-record ring with actor/counterpart snapshots, physical input and queue identity, attack/hold/paired generations, montage source, terminal dispositions, and closed reason codes. Console controls clear and dump deterministic versioned CSV before PIE teardown.
- Red/green: the paired auto-continue regression first proved synchronous successor state could mislabel the outgoing marker; the unity-build baseline then exposed serializer helper collisions. Final focused runs are `13/13` telemetry and `18/18` defense chain, and the final unity build exits `0`.
- Adversarial closure: hold press/release edges, synthesized follow-ups, cross-actor stable IDs, unrelated marker reporters, actual notify-source montage, outgoing stage snapshots, bounded retention, CSV escaping/cardinality, disabled capture, and precise queued/immediate terminal reasons are covered.
- Full baseline: `695/695` completed; only asset-dependent `KatanaCombat.Defense.GateA.PIEProof` fails. This is not a green branch baseline.
- Proof limit: reaction, AI token, orbit, alignment-error, animation-lane, scenario/map, and participant-count emitters are not implemented yet. Empty reserved fields do not prove those systems.
- Next action: perform the named asymmetric-input checkpoint in `Lvl_DefenseMatrix`, then repeat in `Lvl_ThirdPerson1`; dump both CSVs before stopping each PIE session and route the proven result into Micro-Plan 04A.
