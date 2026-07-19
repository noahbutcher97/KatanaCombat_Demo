# Combat Action And Reaction Stabilization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use `superpowers:executing-plans` or `superpowers:subagent-driven-development`. Implement one micro-plan at a time and stop at every acceptance gate.

**Goal:** Stabilize AI attack lifecycle, attack alignment and live steering, orbit movement, action cancellation, hit reactions, trades, and animation composition, then prove the complete behavior in automation and PIE.

**Architecture:** Existing components retain lifecycle ownership. `UCombatComponent` projects one canonical action snapshot and applies pure voluntary-action decisions. `UHitReactionComponent` commits pure reaction decisions. `UEnemyCombatAIComponent` owns token and movement mode. `UTargetingComponent` remains the only alignment executor and consumes a pure, per-attack live-steering policy only for regular player attacks. AnimBP composes presentation but decides no gameplay.

**Tech Stack:** Unreal Engine 5.6 C++, StateTree, NavigationSystem, Gameplay Tags, Motion Warping, Enhanced Input, Unreal Automation, Data Validation, Katana asset-migration commandlet, PowerShell, Editor/PIE.

**Authority:** Implement against `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization-design.md` and its eight linked micro-specs. Current source is the compatibility baseline. Stop and reconcile the spec and plan if implementation evidence contradicts either.

## Workspace Boundary

At planning time, preserve these user-owned changes without modification:

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

No implementation task may save those packages. Never use `git add .`; stage exact source, test, document, manifest, and reviewed asset paths.

## Pre-Implementation Readiness Gate

Before Step 1:

1. Commit this planning package alone on the feature branch; no `Content/` or runtime source path may be staged.
2. Verify all local Markdown links, ASCII, trailing whitespace, and symptom traceability in `ADVERSARIAL_AUDIT.md`.
3. Recompute the eight preserved WIP hashes and stop on any mismatch.
4. Run `Tools/Codex/run-agent-baseline.ps1` and summarize its automation log.
5. Record the planning commit, baseline evidence, process state, and exact next test in the execution handoff.

After this gate passes, planning is frozen. A new high/medium empirical contradiction may reopen only its owning micro-spec and plan.

## Dependency Order

| Step | Micro-plan | Depends on | Commit boundary |
|---|---|---|---|
| 1 | [Lifecycle crash hardening](2026-07-18-combat-action-reaction-stabilization/01-lifecycle-crash-hardening.md) | current main | `Fix reentrant enemy attack startup` |
| 2 | [Alignment and locomotion rotation](2026-07-18-combat-action-reaction-stabilization/02-alignment-and-locomotion-rotation.md) | 1 | preflight source, migration tooling, reviewed assets, then enforcement source |
| 3 | [Enemy orbit and StateTree movement](2026-07-18-combat-action-reaction-stabilization/03-enemy-orbit-and-state-tree-movement.md) | 1 | `Stabilize StateTree enemy orbit movement` |
| 4A | [Input availability and hold commitment](2026-07-18-combat-action-reaction-stabilization/04a-input-availability-and-hold-commitment.md) | 1-2 | `Stabilize input availability and hold commitment` |
| 4B | [Action arbitration and cancel windows](2026-07-18-combat-action-reaction-stabilization/04-action-arbitration-and-cancel-windows.md) | 2, 4A | runtime source, migration tooling, then reviewed timing assets |
| 5 | [Hit reactions and trades](2026-07-18-combat-action-reaction-stabilization/05-hit-reactions-and-trades.md) | 1, 4B | resolver/runtime source, data tooling, then reviewed tuning assets |
| 6 | [Animation layering and content migration](2026-07-18-combat-action-reaction-stabilization/06-animation-layering-and-content-migration.md) | 4B-5 | presentation source, animation tooling, then reviewed binary assets |
| 7 | [Validation and visible proof](2026-07-18-combat-action-reaction-stabilization/07-validation-and-visible-proof.md) | 1-6 | `Add action reaction proof coverage` and docs closure |

Steps 2 and 3 may be implemented independently after Step 1, but commit and verify them separately. Do not combine runtime policy and binary asset changes.

## Universal Slice Protocol

Before every micro-plan, after compaction/session change, and after unexpected workspace changes:

```powershell
git status --short --branch
git log -3 --oneline
git diff --name-status HEAD~1..HEAD
```

Read the master spec, that step's micro-spec/plan, the narrowest current source, prior commit diff, and `Source/KatanaCombatTest/README.md`. Re-check risky UE APIs against installed UE 5.6 source instead of trusting remembered line numbers.

Before Step 1, run the full baseline once and record immutable hashes for the dirty map and every existing untracked external-actor file. Recompute them at every asset slice and final acceptance; a mismatch caused by this branch is a stop condition.

For every implementation slice:

1. Add the smallest focused failing automation test first. Compile-safe scaffolding may return a deliberately wrong neutral result.
2. Build with UBA disabled:

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="$PWD\KatanaCombat.uproject" -Progress -NoHotReload -NoUBA -NoUBTMakefiles -MaxParallelActions=1
```

3. Run the focused root with `;Quit` and inspect the generated log.
4. Implement only the micro-spec contract; rerun build and focused tests.
5. Run applicable source checks, validators, commandlet audit, and PIE proof.
6. Perform the hostile checklist in `ADVERSARIAL_AUDIT.md`; fix every high/medium finding and add regression coverage.
7. Run `git diff --check`, inspect the complete diff, and confirm user WIP is unchanged.
8. Commit only the slice using the listed imperative message.

## Cross-Slice Invariants

- Snapshot before any external call and revalidate participants/generations afterward.
- Mutate and clean up before broadcasts; delegates are last.
- Death and owning paired sequences are terminal for voluntary cancellation.
- Every physical input edge is captured before eligibility and ends with a reason-coded disposition; suppression, queueing, and rejection are distinct.
- A committed hold and its exact-generation continuation cannot be replaced through the normal attack buffer.
- Normal attack buffering has one pending last-input-wins slot and starts at most one action per arbitration boundary.
- Every normal or hold-owned attack carries the physical edge/release's immutable world-space facing intent through execution.
- Targetless attack facing uses the closed source order and terminal-zero input cannot reuse stale movement direction.
- Live movement steering is a separate terminal-aware signal: it may offset only the exact regular player attack's rotation target within authored/reachable limits and may not retarget, move translation, or affect AI/paired/defense owners.
- Strict alignment failure occurs before montage start with a terminal reason; it never silently consumes player input or permits a partial damaging start.
- Ordinary action suppression never acquires or restores CharacterMovement mode; only explicit paired/death/reaction owners may do so.
- Additive response never releases an AI token or changes traces/damage.
- Gameplay decides animation class; montage groups never decide gameplay.
- Attack alignment uses attack settings; defense alignment uses defense settings.
- StateTree task exit releases only state, focus, movement, and requests it owns.
- New Gameplay Tags require a runtime consumer, validator, and tests in the same slice.
- Do not report source/asset checks as visible behavior proof.

## Review And Handoff

Maintain `docs/handoffs/2026-07-18-combat-action-reaction-stabilization-execution.md` during implementation. Record HEAD, active step, changed files/assets, exact evidence, unresolved findings, dirty-WIP classification, and next action. Update it before a long command, at each commit, and before pausing.

Each commit is a review gate. A reviewer should not need to understand later animation assets to validate the crash fix or orbit planner. Do not open one giant undifferentiated PR; keep commits independently auditable even if the branch remains open until all steps are proven.

## Final Acceptance

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "Tools\Codex\run-agent-baseline.ps1"
powershell -NoProfile -ExecutionPolicy Bypass -File ".agents/skills/katana-verify/scripts/summarize-automation-log.ps1"
git diff --check
git status --short
```

Then complete both-level PIE proof without saving either map, restart the Editor, repeat the core scenarios, and reconcile architecture/test docs to proven behavior. In-scope `Partial` or `Not Implemented` items block merge. The pre-implementation hostile review is recorded in [ADVERSARIAL_AUDIT.md](2026-07-18-combat-action-reaction-stabilization/ADVERSARIAL_AUDIT.md).
