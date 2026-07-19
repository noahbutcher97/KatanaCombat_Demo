# Micro-Plan 03: Enemy Orbit And StateTree Movement

**Spec:** `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization/03-enemy-orbit-and-state-tree-movement.md`

**Goal:** Replace path-churning radial oscillation with a pure annular orbit planner and an ownership-safe StateTree adapter.

## Task 1: Write Planner Tests First

**Create:** `Source/KatanaCombatTest/Private/EnemyOrbitPlannerTests.cpp`

**Modify:** `Source/KatanaCombatTest/Private/EnemyCombatAITests.cpp`

- [ ] Add red pure tests for clockwise/counter-clockwise tangent, inner/outer radial correction, annulus hysteresis, arc clamping, unchanged destination suppression, and reversal threshold/cooldown.
- [ ] Add red adapter tests for nav projection failure, move request failure, request reuse, focus ownership, speed mode restoration, and token preservation on circle-task exit.
- [ ] Add a source test that fails while the circle task uses positional-boolean `MoveToLocation` or ignores `EPathFollowingRequestResult`.

## Task 2: Implement The Pure Planner

**Create:**

- `Source/KatanaCombat/Public/AI/EnemyOrbitPlanner.h`
- `Source/KatanaCombat/Private/AI/EnemyOrbitPlanner.cpp`

**Modify:** `Source/KatanaCombat/Public/AI/EnemyAITypes.h`

- [ ] Define immutable planner query/result and closed reason codes.
- [ ] Extend circling config with radius hysteresis, bounded arc lookahead, destination replacement distance, failure threshold, and reversal cooldown.
- [ ] Produce finite 2D candidates without world queries or random direction changes.
- [ ] Preserve the current destination when replacement is not justified.

## Task 3: Adapt StateTree And Navigation

**Modify:**

- `Source/KatanaCombat/KatanaCombat.Build.cs`
- `Source/KatanaCombat/Public/AI/EnemyCombatStateTreeTasks.h`
- `Source/KatanaCombat/Private/AI/EnemyCombatStateTreeTasks.cpp`
- `Source/KatanaCombat/Public/AI/EnemyCombatAIComponent.h`
- `Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp`

- [ ] Add `NavigationSystem` dependency.
- [ ] Project planner output to navmesh and build an explicit `FAIMoveRequest`.
- [ ] Audit Blueprint/StateTree references to `GetCirclingDestination()` and `RandomizeCirclingDirection()`. Keep only a deprecated planner wrapper where compatibility is proven necessary; migrate any periodic-random caller explicitly.
- [ ] Track last destination, request/result, issue time, failure count, direction-change time, and owned focus.
- [ ] Do not replace a useful active request. Reverse only after repeated failure and cooldown.
- [ ] Keep circle task `Running`; ensure token task completion controls transition.
- [ ] Remove periodic random direction timers.

## Task 4: Centralize AI Speed/Focus Ownership

- [ ] Capture baseline `MaxWalkSpeed`, `RotationRate`, `bUseControllerDesiredRotation`, and `bOrientRotationToMovement` once in the AI component.
- [ ] Apply `CircleSpeed` and `ApproachSpeed` from AI movement mode, not task-local writes.
- [ ] Restore exact baseline on terminal/idle teardown.
- [ ] In circling mode, use controller-desired rotation and owned target focus at an initial 540-degree/second rate so tangential movement does not turn the enemy away from the player; restore all flags/rates exactly on exit.
- [ ] Set and clear combat-target focus only when the matching task/action still owns it.
- [ ] Update `Source/KatanaCombatEditor/Private/Commandlets/Operations/EnemyAIProofAssetsOperation.cpp` only if generated StateTree bindings require new fields; run audit before apply.

## Task 5: Verify And Commit

- [ ] Build and run `KatanaCombat.EnemyAI.Orbit`, `KatanaCombat.EnemyAI`, token, and StateTree tests.
- [ ] PIE with four enemies for at least five minutes in both levels; collect request count, abort/replacement reason, radius error, speed, direction, focus, and token telemetry.
- [ ] Confirm no periodic oscillation and no requirement to leave/re-enter attack range.
- [ ] Record whether local orbit is adequate; do not add EQS or avoidance without a new accepted finding.
- [ ] Commit: `Stabilize StateTree enemy orbit movement`.

**Gate:** Any unexplained request churn, speed leak, focus clobber, or token loss blocks action-policy work.
