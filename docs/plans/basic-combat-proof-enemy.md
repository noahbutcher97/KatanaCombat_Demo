# Basic Combat Proof Enemy

> Created: 2026-07-01
> Branch: `feature/basic-combat-proof-enemy`
> Scope: production-shaped minimal enemy combat AI wiring for proving parryable attacks and Chain Counter runtime flow.

## Intent

KatanaCombat already has `UEnemyCombatAIComponent` and `UCombatTokenSubsystem`, but spawned enemies were still effectively dummies. This branch wires the existing AI component into `AEnemyCharacter`, makes StateTree the canonical project enemy AI driver, and hardens the smallest token-driven attack path needed for a playable proof enemy.

## Current Slice

- `AEnemyCharacter` owns `CombatAIComponent` by default.
- `AEnemyCharacter` defaults to `AEnemyCombatAIController` and auto-possesses AI when placed or spawned.
- `AEnemyCombatAIController` owns the StateTree runtime component; bare C++ fixtures tolerate no assigned StateTree asset.
- `UEnemyCombatAIComponent` remains the owner of combat target, attack selection, token requests, and attack execution state.
- `EnemyCombatStateTreeTasks` exposes StateTree tasks and conditions that call the component instead of duplicating combat/token logic.
- `/Game/ProjectFiles/AI/ST_EnemyCombatProof` is the authored proof StateTree asset for the minimal target -> token -> approach -> attack loop.
- `/Game/ProjectFiles/AI/BP_EnemyCombatAIController` assigns the proof StateTree and is the default controller for `/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter`.
- `EnemyAIProofAssets` is the repeatable commandlet operation that creates, assigns, and re-validates those assets, including a load check for `/Game/ProjectFiles/Levels/Lvl_ThirdPerson1`.
- Automation can inject a deterministic `UCombatTokenSubsystem` when synthetic worlds do not own a normal `GameInstance`.
- Attack execution failure releases tokens and returns to `Circling` or `Idle` instead of leaving enemies stuck in `Approaching`.
- Queued enemies leave the token queue if their combat target is cleared before a token is granted.
- A StateTree token wait that times out or exits while still queued cancels its pending request without releasing an active token.

## Canonical AI Model

- StateTree is the decision-authoring layer for project enemies.
- `UEnemyCombatAIComponent` is the actuator/state contract for combat decisions.
- Behavior Tree task/decorator source is sunset for this enemy path.
- Template `Variant_Combat` and `Variant_SideScrolling` StateTree examples remain reference/template code unless a later cleanup explicitly targets them.

## Acceptance Checks

- Spawned `AEnemyCharacter` instances expose `CombatAIComponent`.
- Spawned `AEnemyCharacter` instances default to a StateTree-capable AI controller.
- `ST_EnemyCombatProof` compiles and is assigned through `BP_EnemyCombatAIController`.
- `BP_EnemyCharacter` defaults to the proof controller, auto-possesses placed/spawned AI, and has at least one usable attack.
- `Lvl_ThirdPerson1` loads headlessly and its placed proof enemies have controller, possession, and attack data ready.
- Setting a combat target enters `Circling`; clearing it returns to `Idle`.
- A configured enemy can request a token, select an attack, and enter `Approaching`.
- A queued enemy receives the token when the current token holder is parried/interrupted.
- A queued enemy that loses its target does not receive the next released token.
- A queued enemy whose StateTree token wait expires is removed from the queue and can retry through the normal recovery loop.
- Failed attack execution cleans up token and selected attack state.
- Active source no longer contains project enemy Behavior Tree task/decorator implementations.

## Explicit Non-Goals

- Full production perception, patrol, alert, or group tactics.
- Level layout edits or broad map resaves beyond required placed-enemy default repair.
- Final animation polish.
- Map-level Chain Counter proof. That still requires a parryable enemy attack montage with `AnimNotifyState_ParryWindow` and editor/PIE evidence.
