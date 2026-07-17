# Defense Interaction Execution Checkpoint

Date: 2026-07-16
Branch: `codex/defense-interaction-design`
Pre-checkpoint HEAD: `108abf49` (`Harden defense interaction execution plan`)
Status: `SLICE 2 COMPLETE - GO TO TASK 3`

## Authority And Scope

- Accepted design: `docs/superpowers/specs/2026-07-16-defense-interaction-design.md` at `cd197a0f`.
- Reconciled execution plan: `docs/superpowers/plans/2026-07-16-defense-interaction-implementation.md`.
- Task 0A changed no runtime code or assets. Its only writes are this checkpoint and corrections to unsupported plan assumptions.
- The Editor was closed. The worktree contained no user WIP before these documentation edits.
- No design-spec change was required: the unsupported notify GUID was introduced by the plan, not the accepted design.

## Current Baseline

The current branch passed the standard baseline:

- Build: `Saved/Logs/Codex-Agent-Baseline-20260716-190955-build.out.log`, exit `0`.
- Automation: `Saved/Logs/Codex-Agent-Baseline-20260716-190955-automation.out.log`, exit `0`.
- Summary: `Saved/Logs/Codex-Agent-Baseline-20260716-190955-automation-summary.json`.
- Result: 439 completed, 0 failures/errors, `BASELINE GREEN`.

The first build attempt reached runtime-module compile/link but stalled in UBA committed-memory throttling. After an unrelated engine-wide build released `Build.bat`, `Saved/Logs/Defense-Task0A-20260716-190944-build-no-uba.log` completed with `Result: Succeeded`. This is an environment deviation, not a suppressed test failure.

## Installed UE 5.6 Feasibility

| Assumption | Primary evidence | Observed behavior | Status | Required plan treatment |
|---|---|---|---|---|
| A notify can identify its montage instance | `Engine/Source/Runtime/Engine/Public/Animation/ActiveMontageInstanceScope.h`; `AnimMontage.cpp:2737` | `FAnimNotifyMontageInstanceContext::MontageInstanceID` is installed into the event context. | Supported | Reject production windows/markers that lack the context. |
| Runtime code can use notify GUID | `Engine/Source/Runtime/Engine/Public/Animation/AnimTypes.h:356-364` | `FAnimNotifyEvent::Guid` is under `WITH_EDITORONLY_DATA`. | Unsupported, reconciled | Use source animation soft path plus exact runtime `Notifies` array index. Pointer comparison is allowed only to locate the index. |
| Motion Warping returns an owned clone | `MotionWarpingComponent.h:148`; `MotionWarpingComponent.cpp:834` | `AddModifierFromTemplate` duplicates, adds, and returns the clone. | Supported | Configure only the returned clone; never mutate the asset-shared template. |
| Warp rate can follow play-rate changes | `RootMotionModifier.cpp:193-258`; `RootMotionModifier.cpp:464-483` | `Update` stores effective `PlayRate` before invoking the clone's update delegate; constant-rate rotation multiplies by play rate. | Supported with constraint | Bind clone-local update/deactivation callbacks and divide the desired simulation rate by positive effective play rate. Frozen/reverse playback uses fallback. |
| Movement and root motion compose predictably | `CharacterMovementComponent.cpp:2821-2947` | pose/root motion, movement rotation, and root-motion rotation compose during Character Movement. | Supported with constraint | Targeting ticks after Character Movement via prerequisite, disables competing yaw, and validates final actor yaw rather than modifier settings. |
| Montage root motion can be audited headlessly | `AnimMontage.h:865-868`; `AnimMontage.cpp:918` | The three-argument `ExtractRootMotionFromTrackRange` API is current; the two-argument overload is deprecated. | Supported | Pass explicit `FAnimExtractContext` with root extraction enabled. |
| Core ticker provides unscaled deadlines | `Containers/Ticker.h`; `LaunchEngineLoop.cpp:5852` | Core ticker uses application delta, outside world/actor dilation. | Supported with constraint | Retain/remove every ticker handle and validate weak world, actors, interaction, and stage generation. |
| Package save APIs support explicit saves | `UObject/Package.h:1036-1058`; `UObject/SavePackage.h` | UE 5.6 supports default-constructed `FSavePackageArgs`; old convenience construction is deprecated. | Supported with constraint | Save only approved package-ledger entries and post-save reload World Partition packages. |
| Notify-state UObject can own per-playback state | Current `AnimNotifyState_PairedAnimationCollision.*`; UE asset instancing model | The current notify caches owner, collision, movement, and partner state on an asset-shared UObject. Concurrent actors can overwrite it. | Unsupported, reconciled | Canonical stages own leases; the notify becomes a stateless component-record adapter keyed by actor, montage instance, and runtime event ID. |

## Current Runtime Trace

- `UCombatComponent::OnInputEvent` checks eligibility before a common input record. Block Press calls legacy `TryCounter()` before `BeginBlock()` and returns before the normal queue record.
- `BeginBlock()` calls `FaceThreatForBlock()`, which directly calls `SetActorRotation`.
- attack phase entry enables weapon traces; Recovery/None disables them. `AttackStateMachine.AttackGeneration` already supplies an attack generation.
- `UWeaponComponent::ProcessHit()` inserts the actor into `HitActors` before broadcasting the candidate. This can spend budget before target acceptance.
- `ABaseCombatCharacter::OnWeaponHitTarget()` filters, independently computes block, calls the damage interface, then independently presents effects. `ApplyDamage_Implementation()` checks block again. Health/death and delegates can run synchronously inside this call stack.
- `UTargetingComponent` owns several singleton tracking modes and broad target clearing, but no opaque priority/owner handles. `AnimNotifyState_CombatWarp` currently mutates its shared modifier template.
- `UEnemyCombatAIComponent` directly rotates before attack, owns token cleanup in several callbacks, and StateTree polls component state. Generation-keyed consume/termination is required to avoid duplicate release or an indefinitely running task.
- `UPairedAnimationComponent` has broad cleanup and direct global time restoration sites. Stage handoff can restore movement, collision, context, warp, or time that a successor still needs.

## Gate A Read-Only Inventory

Read-only reports under `Saved/Logs/Commandlets/KatanaAssetMigration/` completed successfully:

- `defense-task0a-enemy-plan.json`: one unchanged aggregate row; `Lvl_ThirdPerson1` loaded with four enemies and all four already had usable attacks.
- `defense-task0a-counter-plan.json`: `LightAttack_1` uses `AM_Light_Combo_1`, section `Attack_1` (`0.0` to `1.366667`); counter data and finisher data are assigned; a counter window exists.
- `defense-task0a-readiness.json`: `LightAttack_1` and `Counter_LightAttack_1` load; the attack section is valid.
- `defense-task0a-notify-plan.json`: the Light attack's canonical attack-phase/hold notify set is unchanged.

Current Gate A facts:

- `LightAttack_1` has no parry window and no `Attack.Defense.Parryable` capability tag. Its current tags are combo, directional, hold, and light-attack tags. No parry timing may be inferred.
- The unchanged `EnemyAIProofAssets` plan proves `IA_Block` is Boolean, assigned to `BP_Player`, mapped in `IMC_Combat` to `ThumbMouseButton` and `Gamepad_LeftShoulder`, and not mapped to right mouse.
- The four tracked enemy actor packages are:
  - `Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/7/CB/19YS22F55XPIYEV7ASGWJO.uasset`
  - `Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/6/JS/VGKO8NKW281LCY3A3K4ETM.uasset`
  - `Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/A/1B/Y3W89WVL6CTN8GGMOAELDW.uasset`
  - `Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/6/69/JZK42Z6X6VW6A5NALDH47D.uasset`
- Headless evidence does not prove bridge/counter/finisher pose compatibility, marker quality, sockets/bones, block/parry presentation, VFX/audio, final yaw, displacement, or live Chain continuity. Those remain explicit Gate A Editor/PIE obligations.

## Migration Infrastructure Audit

The current runner snapshots initially dirty packages, records actor outer packages, refuses unapproved dirty saves, saves only reported changed packages, and distinguishes map extensions. `EnemyAIProofAssets` already owns StateTree, controller/enemy/player Blueprints, block input, mapping context, level assignments, and proof-map loading; defense migration must reuse or validate those facts.

Three weaknesses are now explicit plan requirements:

1. `EnemyAIProofAssets` emits one aggregate row, so target count and default false fields can overstate evidence. Defense reports require one row per manifest case plus a package ledger.
2. Current `Apply` reruns an operation without binding it to the reviewed `Plan`. Defense Apply/ApplyAndSave require an approved plan report and reviewed fingerprint, recomputed before mutation.
3. External actor discovery exists, but successful save/reload has not been proven for the defense operation. Every World Partition package must be listed, scope-checked, saved, and reloaded individually.

## Adversarial Closure

The plan now explicitly closes the Task 0A high/medium findings:

- editor-only notify GUID replaced by a runtime source identity;
- distinct Active/Recovery point notifies carry identity-bearing phase context so stale hit-window close cannot affect a newer attack;
- shared notify mutable collision/movement state prohibited;
- shared warp-template mutation replaced by returned-clone ownership and live rate callbacks;
- Character Movement tick/order and one-executor rules made explicit;
- ticker handles and teardown made explicit;
- reviewed plan drift and changed-package scope bound before mutation;
- operation-specific report cardinality and World Partition reload proof required;
- synchronous health, damage, and death callbacks split from silent gameplay mutation so receipt finalization and weapon accounting precede all reentrant delegates and optional presentation.

Existing first-commit registration, immutable receipts/presentation, actor revalidation after cross-actor calls, generation-keyed AI/paired callbacks, bounded tombstones, and overlap-safe leases remain required. No unresolved high/medium feasibility finding blocks Slice 1.

## Stop/Go Decision

`GO` for Task 1 only. The route is test-first: add failing contract/resolver/selector tests, implement typed value contracts and pure decision logic, run focused tests, build, run the Slice 1 adversarial/spec-coverage gate, and commit. This decision does not authorize asset edits, runtime wiring, or Gate A claims.

## Slice 1 Completion

Task 1 is complete. The implementation adds the closed defense enums and canonical identities, immutable prediction/query/decision/receipt contracts, `FDefenseAttackProfile`, semantic defense tags, `UDefenseConfiguration`, a pure resolver, and an interface-backed deterministic table selector. No production input, contact, animation, AI, map, Blueprint, or asset path is wired by this slice.

Test-first evidence included compile failures for absent contracts, executable failures against neutral resolver/selector stubs, and focused red regressions for malformed notify/window identity, paired-takeover contact precedence, null authored data, stale target copies, selector payload semantics, and threat filtering. The implementation then closed each case.

The adversarial pass additionally:

- rejects negative notify/montage IDs and non-finite window timing;
- replaces loop-based angle unwind with bounded finite normalization;
- makes consumed contact win over paired takeover for late traces;
- removes redundant `bHasAttackData` and `bHasActiveParryWindow` authorities;
- requires canonical attack/window identity and explicit intended-defender agreement for perfect parry;
- ignores fabricated defense tags on null-`AttackData` compatibility contacts;
- fails closed on contradictory team relation and invalid geometry;
- retains prediction targets, separate deadlines, actual source socket, threat-lock state, and alignment owner ID;
- exposes selector policy through `IDefensePresentationSelector` for future storage alternatives.

Final evidence is `Saved/Logs/Codex-Agent-Baseline-20260716-201508-*`. `KatanaCombatEditor Win64 Development` built successfully and all 452 `KatanaCombat` tests passed with zero failures/errors. The generated warning count remained 482, identical to the 439-test Task 0A baseline, and no new defense test emitted a warning. Static inspection found no world, component, asset-loading, delegate-broadcast, or mutable actor-state access in `FDefenseResolver`.

`GO` for Task 2 after the Slice 1 commit. Gate A remains unclaimed, and all asset/editor/PIE obligations remain open.

## Slice 2 Completion

Task 2 now provides the native rich-contact commit boundary: target-owned first registration and immutable receipts, reflected bounded tombstones, silent resistance/health/death mutation, source accounting only after acceptance, canonical source finalization, and retained unscaled `FTSTicker` fallback handles. Generic contacts and `ApplyDamage_Implementation` retain immediate compatibility behavior.

The test-first adversarial pass found and closed these material gaps:

- source destruction could invalidate cache lookup or strand world-time fallback;
- retained `NewCommit` receipts could replay source presentation and `OnAttackHit`;
- a caller-mutated receipt could alter source effects instead of using target-owned truth;
- foreign weapon IDs could commit, poison a legitimate cache key, or let stale compatibility generations hit;
- target loss during the target call could still consume source hit budget;
- participant destruction during Blueprint-native team queries could still mutate health;
- health/dying listener destruction could continue later optional dispatch;
- cache and pending results were not reflected, so forced GC collected referenced attack data;
- accepted trace endpoints and target-bone fallback precedence were incomplete;
- the cache-cap fixture advanced the generation it claimed was active.

Current focused evidence:

- final focused build: `Saved/Logs/Defense-Slice2-WarningCleanup-20260716-221838-build.log`, exit `0`;
- full contact/static root: `Saved/Logs/Defense-Slice2-WarningCleanup-20260716-221854-contact.log`, 29 completed, 0 failures/warnings/errors;
- post-audit adjacent roots: Damage 19 in `Saved/Logs/Defense-Slice2-PostAudit-20260716-221532-KatanaCombat-Damage.log`, Weapon 22 in `Saved/Logs/Defense-Slice2-PostAudit-20260716-221547-KatanaCombat-Weapon.log`, and Integration 12 in `Saved/Logs/Defense-Slice2-PostAudit-20260716-221602-KatanaCombat-Integration.log`, all green with zero failures.
- final standard baseline: `Saved/Logs/Codex-Agent-Baseline-20260716-221917-*`, 482 completed, 0 failures/errors, and 482 warnings. The warning count is identical to the pre-Slice 1 baseline; the Contact root contributes no warnings.

### Slice 2 Contract Coverage

| Contract | Status | Evidence boundary |
|---|---|---|
| Target-owned `NewCommit`/`InProgress`/`Cached`, immutable applied damage, monotonic epoch | Proven | Contact cache, replay, reentry, lethal, and canonical-receipt tests. |
| Active-record retention, terminal tombstone aging/cap, lazy source invalidation, GC-safe receipt ownership | Proven | Cache lifecycle/cap/source invalidation/forced-GC tests. |
| Rich target authority, silent gameplay commit, source accounting before observable callbacks | Proven | Architecture gates plus Damage/Weapon/Contact event-order tests. |
| Friendly/invulnerable/dead/i-frame/consumed/neutral policy and enemy-enemy suppression | Proven | Focused outcome and weapon-budget tests. |
| Complete retained trace/hit metadata, independent prediction, exact bone provenance, target-bone precedence | Proven headlessly | Contact context tests; live parent-skeleton coverage remains Gate A evidence. |
| Participant destruction, fallback, duplicate source finalization, malformed/stale identity handling | Proven | Dedicated adversarial regression tests under `KatanaCombat.Defense.Contact`. |
| Authored hit-window producer and atomic perfect-parry attack consumption | Out Of Scope | Task 4 owns the producer and real same-frame consumption race. |
| Full configuration precedence, owned alignment, specialized block/attacker response | Out Of Scope | Task 3 owns these runtime paths. |
| Asset, Blueprint, montage, map, and visible PIE proof | Out Of Scope | Gate A/Task 6 own live proof. |

No in-scope contract remains `Partial` or `Not Implemented`, and no high/medium Slice 2 audit finding is deferred. No asset, Blueprint, montage, map, input, AI, or paired-sequence package changed in Slice 2.

Explicit later-slice dependencies are not Slice 2 failures: Task 4 supplies authored hit-window identity and atomic perfect-parry consumption; Task 3 supplies full defense-configuration precedence plus specialized normal-block/attacker-response presentation and alignment; Gate A supplies live parent-skeleton, asset, Editor, and PIE proof. Slice 2 proves immutable first-contact precedence and pre-consumed contact handling, not the future real parry-window producer.

The exact next action after the Slice 2 commit is the Task 3 context-refresh preflight, followed by failing input-capture tests. Do not begin asset edits or claim visible block/parry behavior yet.

## Slice 3 Completion

Task 3 preflight started from clean commit `adba9ffb`. Live source reinspection confirms that `OnInputEvent()` still returns before any common record for missing settings, paired-input rejection, Block routing, and successful Chain routing; failed Chain preflight still falls through to the normal queue. The accepted boundary remains: Task 3 owns unconditional capture and terminal route disposition, Task 4 owns authored parry-window identity and attack consumption, and Task 5 owns the marker-driven `ParryActive -> CounterWindow` transition.

Input routing and threat-lock ownership are now complete within Slice 3. Input edges are captured before all gates in a bounded monotonic ledger; Block is stateful/terminal; failed `ChainOnly` preflight expires without queue fallthrough; the stale same-call Chain transition now stops at `ParryActive`. Attack execution snapshots use process-monotonic stable IDs, generation-bound intended targets, and evidence-gated predictions.

Threat selection now performs one range-capped `UTargetingComponent` enumeration per opportunity, reuses its LOS/targetability policy, filters explicit hostile active snapshots, normalizes deadlines against simulation time, downgrades stale confidence and credible intent, and delegates deterministic ranking/hysteresis to `FDefenseResolver`. The component owns lock generation, acquisition time, remaining turn budget, same-frame publication coalescing, and the 0.05-second simulation-time guarded timer. Release, death, teardown, and paired takeover clear ownership; held guard revalidates after paired exit.

Step 4 test-first evidence:

- behavioral RED: `Saved/Logs/Defense-Slice3-ThreatLockRed-20260716-2255-tests.log`;
- focused GREEN: `Saved/Logs/Defense-Slice3-ThreatLockGreen-20260716-2258-tests.log`;
- hardened `KatanaCombat.Defense` root: `Saved/Logs/Defense-Slice3-ThreatHardened-20260716-2300-tests.log`, all tests passed;
- final Step 4 build completed successfully at 23:00 local time.

Step 5 is complete. `UTargetingComponent` now arbitrates opaque capability handles by immutable owner/generation/priority/executor identity, suspends lower-priority requests without destroying them, captures and exactly restores movement/controller rotation settings, and enables its `TG_PrePhysics` tick only while a smooth request exists. Smooth facing uses one swept `CharacterMovement::MoveUpdatedComponent` yaw executor per frame with rate and cumulative-budget clamps. Motion-warp requests reserve one unique named target, publish only while active, and remove only their own names. Broad alignment or motion-warp clearing is accepted only for death/component teardown; request-owned and unmanaged targets are protected from cross-owner cleanup.

Guard now owns one `GuardFacing` request tied to the selected threat interaction. Threat refresh synchronizes the request's spent budget before resolution, genuine threat switches release the old handle and allocate a new generation, block release/paired takeover clear only guard ownership, and guard entry no longer calls `SetActorRotation`. The actual character dying path performs terminal release in addition to component teardown.

Step 5 test-first and adversarial evidence:

- compile-safe behavioral RED: `Saved/Logs/Defense-Slice3-AlignmentBehavioralRed-20260716-231032-tests.log`, four discovered tests failing on the neutral arbiter and 90-degree guard snap;
- hardened focused GREEN: `Saved/Logs/Defense-Slice3-AlignmentHardened-20260716-232432-tests.log`, six tests passing for priority/tie ordering, owner-only and broad-clear policy, exact restoration, swept yaw/budget, warp-name ownership, stale target, real death cleanup, and guard integration;
- post-alignment defense root: `Saved/Logs/Defense-Slice3-PostAlignment-Defense-20260716-232509-tests.log`, exit 0;
- adjacent regressions: two counter-input tests in `Saved/Logs/Defense-Slice3-PostAlignment-CounterInput-20260716-232535-tests.log` and seven targeting tests in `Saved/Logs/Defense-Slice3-PostAlignment-Targeting-20260716-232558-tests.log`, all green;
- final Step 5 editor build succeeded after adding the test module's explicit `MotionWarping` dependency; `git diff --check` is clean.

Static audit found no remaining `FaceThreatForBlock`/`FindBlockThreat` path and no exposed alignment-handle value. The one remaining production `SetActorRotation` is the known enemy attack-start call owned by Step 8. Shared `AnimNotifyState_CombatWarp` template mutation and normalized runtime modifier rate remain Step 6; manual guard override remains Step 7; configuration precedence, normal-block presentation, and the AI direct-rotation removal remain Step 8.

Step 6 is complete. `SetupAttackWarp` now owns one `ActiveAttackWarp` alignment request, caps attack-authored rotation at the resolved defense capability, replaces prior attack ownership without leaking handles, tracks moving targets through request updates, and releases its handle from canonical `SetPhase(None)` termination. The custom combat-warp notify creates the engine-owned clone before runtime configuration and never casts or mutates the shared template.

`UTargetingComponent` binds each clone to the active request handle, enforces `ConstantRate`, recomputes the inverse engine cap from UE 5.6's live effective play rate, spends observed composed yaw from the cumulative request budget, and bounds the current frame by the remaining budget. Positive 0.5x/1.0x/2.0x rates remain equivalent in simulation time; world and actor dilation do not multiply the capability. Zero, near-frozen, reverse, unmanaged, duplicate-window, and exhausted-budget paths fail closed. Higher priorities disable but retain a still-valid lower clone; resume returns it to `Waiting`; natural deactivation and owner release unregister only the matching clone. One request cannot register overlapping runtime executors.

Step 6 test-first and regression evidence:

- duplicate-window behavioral RED: `Saved/Logs/Defense-Slice3-MotionWarping-DuplicateRed-20260716-2345-tests.log`;
- final focused GREEN: `Saved/Logs/Defense-Slice3-MotionWarping-FinalFocused-20260716-2350-tests.log`, three tests passing for template isolation, rate/dilation normalization, suspension/resume, budget accounting, duplicate/unmanaged rejection, owner-only release, exact restoration, and attack termination;
- full alignment root: `Saved/Logs/Defense-Slice3-MotionWarping-AlignmentGreen-20260716-2347-tests.log`, nine tests passing;
- adjacent targeting root: `Saved/Logs/Defense-Slice3-MotionWarping-TargetingGreen-20260716-2347-tests.log`, seven tests passing;
- full defense root: `Saved/Logs/Defense-Slice3-MotionWarping-20260716-2348-tests.log`, 63 tests passing with zero failures;
- final `KatanaCombatEditor Win64 Development` build succeeded at 23:49 local time after the actor-dilation assertion was added.

This step proves runtime modifier configuration and ownership, not the later visible final-yaw acceptance gate. Authored root rotation plus warp correction still requires Task 6 telemetry/PIE evidence. Step 7 owns sustained-guard manual steering and must preserve the same total actor-yaw cap.

Step 7 is complete. `APlayerCharacter` forwards normalized yaw input and explicit look completion/cancellation into `UCombatComponent`. Sustained guard keeps one owned `GuardFacing` handle: at or above the configured threshold it swaps only that request from threat tracking to controller-independent manual direction, and below threshold it holds current facing until the 0.10-second `FPlatformTime` delay expires. Resume force-revalidates the locked threat, preserves the same handle and spent interaction budget when identity is unchanged, and returns to automatic target tracking. Manual and automatic actor yaw both execute through the same swept Character Movement cap; higher-priority committed requests remain active over the retained guard request.

Step 7 test-first and adversarial evidence:

- isolated compile RED: `Saved/Logs/Defense-Slice3-ManualOverride-IsolatedRed-20260716-235610-build.log`, missing only the planned manual-input API;
- focused budget/resume GREEN: `Saved/Logs/Defense-Slice3-ManualOverride-Focused-20260717-000040-tests.log`;
- zero-threshold behavioral RED: `Saved/Logs/Defense-Slice3-ManualThreshold-Red-20260717-000211-tests.log`;
- hardened threshold/priority GREEN: `Saved/Logs/Defense-Slice3-ManualThreshold-Green-20260717-000313-tests.log`;
- final alignment regression: `Saved/Logs/Defense-Slice3-ManualInputRoute-Retry-20260717-000522-tests.log`, 12 tests passed, including player-look routing and all motion-warp ownership cases;
- final editor build: `Saved/Logs/Defense-Slice3-ManualInputRoute-Retry-20260717-000522-build.log`, succeeded with `-MaxParallelActions=6` after the uncapped attempt hit machine paging-file pressure (`C3859/C1076`).

Static review remains clean for direct rotation in the player, combat, and targeting defense path. No assets changed. Step 8 now owns full configuration precedence, specialized normal-block/attacker-response presentation, and removal of the remaining enemy AI attack-start rotation snap.

Step 8 is complete. Effective defense configuration now resolves newest active scoped stance override, component override, the owner's current `UCombatSettings`, then the C++ default object. Contact geometry, guard/threat policy, attack warp, and presentation consume that central result. Scoped override storage is GC-safe and releases by opaque owner handle.

Normal block commits defender and attacker payloads before gameplay mutation. Defender montage/impact and attacker recoil use only the finalized resolution; actual contact point, normal, surface, and bone remain authoritative. Impact precedence is specialized defender row, attack profile, explicit generic defender row, defense defaults, pooled blocked effects, then weapon fallback. Recoil selects from the attacker's configuration, falls back to a usable generic recoil, then safely blends out. Presentation entry points reject the wrong actor owner, and retained/cached receipts cannot replay either side.

The final adversarial pass found and closed three additional gaps:

- a specialized row without impact assets skipped the explicit generic row;
- an exact recoil row without a usable montage skipped generic recoil;
- a late same-frame threat publication could be lost after the first coalesced scan.

Late same-frame updates now schedule one next-tick refresh. Invalid montage sections fail closed. Enemy attack execution no longer calls `SetActorRotation`; explicit intent reaches `ExecuteAttackData`, and the owned active-attack warp is the only attack-facing path. Source gates cover all defense owners and prove combat-warp clone creation precedes runtime configuration without shared-template mutation.

Final Slice 3 evidence:

- configuration precedence RED/GREEN: `Saved/Logs/Defense-Slice3-ConfigPrecedence-Red-20260717-001115-build.log`, `Saved/Logs/Defense-Slice3-ConfigPrecedence-Green-20260717-001528-build.log`, and `Saved/Logs/Defense-Slice3-ConfigPrecedence-Green-20260717-001630-tests.log`;
- fallback adversarial RED/GREEN: `Saved/Logs/Defense-Slice3-Fallbacks-Red-20260717-0042-tests.log` and `Saved/Logs/Defense-Slice3-Fallbacks-Green-20260717-0045-tests.log`;
- enemy direct-facing source gate RED/GREEN: `Saved/Logs/Defense-Slice3-EnemyFacing-Red-20260717-0034-tests.log` and `Saved/Logs/Defense-Slice3-EnemyFacing-Green-20260717-0034-tests.log`;
- final editor build: `Saved/Logs/Defense-Slice3-Final-20260717-0053-build.log`, succeeded;
- final full defense root: `Saved/Logs/Defense-Slice3-FullDefense-Final-20260717-0051-tests.log`, 72 completed and 0 failed;
- adjacent roots: `Saved/Logs/Defense-Slice3-CounterInput-20260717-0050-tests.log`, 2/2 passed, and `Saved/Logs/Defense-Slice3-Targeting-20260717-0050-tests.log`, 7/7 passed.

No C++ high/medium Slice 3 finding is deferred. No asset, Blueprint, montage, map, input asset, or configuration package changed, so visible normal-block animation and final composed-yaw acceptance remain Task 6 Editor/PIE proof rather than a headless claim. Task 4 still owns authored parry-window identity, attack consumption, `ParryStagger`, and bridge preflight; Task 5 owns retained paired-stage migration and cleanup. `GO` for Task 4 after the Slice 3 commit.

## Slice 4 Completion

Task 4 now owns canonical attacker-side Hit, Parry, and Counter windows keyed by attack generation, exact source-animation notify index, and montage-instance ID. Consuming an attack installs the consumed marker before cleanup or callbacks, retires all matching window records, prevents delayed notifies from reopening the generation, disables its trace, rejects queued continuation, and emits immediate native plus deferred public termination events. Counter notify compatibility data is published only after a canonical open succeeds.

Block Press resolves and commits perfect parry through the public defense path. Contact-first ordering retains its committed hit; input-first ordering consumes the whole source generation and rejects later contacts. Presentation selection cannot rewrite the committed result. The source attacker receives the configured `ParryStaggerDuration`, paired parry data is always nonlethal, and malformed or unavailable bridge data falls back to the generation-keyed no-montage `ParryActive` stage without reopening the attack.

Enemy attack ownership now captures the exact generation and routes consumption, montage completion, and legacy parry/counter callbacks through one idempotent termination function. Token release and attack-end broadcast occur once, and the StateTree task recognizes the consumed generation instead of waiting for montage completion.

The adversarial pass retired legacy Chain feature tests that directly toggled windows, assigned `ChainState`, or invoked protected helpers. Their replacement evidence enters through canonical windows and public input. It also closed delayed-window record retention, consumed-generation reopen, invalid counter-notify compatibility publication, non-finite bridge budgets, signed generation overflow, and an untracked StateTree zero-generation wait.

Final evidence:

- editor build succeeded with `-MaxParallelActions=4`;
- `Saved/Logs/Defense-Slice4-Final-20260717-0219-Defense.log`: 89 completed, 0 failures/errors;
- `Saved/Logs/Defense-Slice4-Final-20260717-0219-EnemyAI.log`: 12 completed, 0 failures/errors;
- `Saved/Logs/Defense-Slice4-Final-20260717-0219-CounterSystem.log`: 13 completed, 0 failures/errors;
- `git diff --check` reports no whitespace errors (line-ending conversion warnings only).

Slice 4 does not claim the retained multi-stage sequence. Task 5 must replace the name-only bridge marker, world timers, broad paired cleanup, and shared collision/time restoration with generation-keyed stage transitions and scoped leases. In particular, authored bridge completion must not expose `None` or restore input, collision, warp, context, or time ownership between successful stages. No asset package changed in Slice 4, and the branch is not merge-ready until those Task 5 contracts and later live proof gates are complete. `GO` for Task 5 after the Slice 4 commit.
