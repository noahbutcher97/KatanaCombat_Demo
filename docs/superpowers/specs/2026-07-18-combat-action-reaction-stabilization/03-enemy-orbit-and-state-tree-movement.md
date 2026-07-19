# Micro-Spec 03: Enemy Orbit And StateTree Movement

## Purpose

Replace the current oscillating circling behavior with stable annular orbit movement that respects navigation, does not churn path requests, and remains compatible with concurrent StateTree attack-token work.

## Evidence And Precedent

The current task issues `MoveToLocation` on every update and ignores its result. UE 5.6 `AIController.cpp` shows `MoveToLocation()` aborts active movement before creating the next `FAIMoveRequest`, making frequent replacement a direct source of jitter.

Epic's [StateTree overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-state-tree-in-unreal-engine) states that tasks in an active state run concurrently and task completion can drive transitions. Epic's [EQS overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/environment-query-system-overview-in-unreal-engine) supports scored tactical locations, but deterministic orbit geometry is sufficient for this slice. Epic's [navigation avoidance guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-avoidance-with-the-navigation-system-in-unreal-engine) says RVO and Detour Crowd are alternative systems and should not be combined.

## Planner Contract

Add a pure `FEnemyOrbitPlanner` with immutable input and output. Input includes owner/target positions, stable direction, desired radius, radius hysteresis, arc lookahead, prior destination, and thresholds. Output includes candidate destination, whether a new move is justified, and a reason code.

The planner:

- treats the desired orbit as an annulus, not an exact radius;
- applies radial correction only outside the inner/outer hysteresis band;
- advances tangentially by a bounded arc lookahead;
- retains one direction until repeated navigation failure or obstruction justifies reversal;
- avoids random periodic flips;
- requires meaningful destination displacement before replacement.

The StateTree adapter projects candidates to navmesh with `UNavigationSystemV1::ProjectPointToNavigation`, submits an explicit `FAIMoveRequest`, stores request/result state, and does not reissue while the current request remains useful.

Existing Blueprint-callable `GetCirclingDestination()` and `RandomizeCirclingDirection()` are reference-audited before removal. During migration, `GetCirclingDestination()` may remain as a deprecated wrapper over the pure planner. Periodic random reversal has no authoritative runtime caller; a populated Blueprint reference must be migrated explicitly rather than silently redirected.

## Movement Ownership

`UEnemyCombatAIComponent` owns the movement mode and captures the character's baseline `MaxWalkSpeed` once:

- `Approaching` uses `ApproachSpeed`.
- `Circling` uses `CircleSpeed`.
- attack/recovery/terminal states restore or set their explicit mode.
- StateTree tasks request a mode; concurrent tasks do not independently overwrite speed.

It also captures/restores baseline CharacterMovement rotation flags and `RotationRate`. Circling uses controller-desired rotation plus owned target focus so the enemy can move tangentially while facing the player; orient-to-movement is disabled only for that owned mode. The initial AI facing rate is 540 degrees/second. Attack Motion Warping supersedes movement facing through existing alignment priority and remains independently configured.

The circle task remains `Running`. The token-request task may complete the state. On exit, circle cleanup stops only movement/focus it owns. A granted token is not released by circle-task exit.

## Focus And Failure

- Circling owns AI focus at a declared priority and faces the combat target while moving.
- Cleanup clears focus only when the same task/action still owns that focus.
- `Failed` move requests increment a bounded counter.
- Reversal requires repeated failure and a cooldown; one transient failure does not flip direction.
- Projection failure retains the prior valid request or reports a stable failure; it does not submit an unprojected point.

## Deferred Options

- EQS may later replace only destination selection if obstacles or tactical scoring require it.
- RVO or Detour may be evaluated after base orbit is stable; exactly one may be enabled in a dedicated proof slice.
- Group slot/coordinated angular spacing is deferred until four-agent PIE evidence proves local tangent planning insufficient.

## Acceptance

- Pure tests cover annulus behavior, tangent direction, hysteresis, destination-change thresholds, and reversal policy.
- StateTree tests cover running/completion semantics, result handling, speed restoration, focus ownership, and token preservation.
- Tests cover exact restoration of CharacterMovement rotation flags/rate after circle, attack, death, StateTree exit, and EndPlay.
- A source regression test rejects positional-boolean `MoveToLocation` use in the orbit task.
- Four enemies in each playtest level move continuously without rapid left/right oscillation, path-request churn, or all-at-once token attacks.
- Telemetry records destination, projection, move result, request replacement reason, direction, radius error, speed mode, and focus owner.
