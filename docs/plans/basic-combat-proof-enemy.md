# Basic Combat Proof Enemy

> Created: 2026-07-01
> Branch: `feature/basic-combat-proof-enemy`
> Scope: production-shaped minimal enemy combat AI wiring for proving parryable attacks and Chain Counter runtime flow.

## Intent

KatanaCombat already has `UEnemyCombatAIComponent`, `UCombatTokenSubsystem`, and Behavior Tree tasks, but spawned enemies were still effectively dummies. This branch wires the existing AI component into `AEnemyCharacter` and hardens the smallest token-driven attack path needed for a playable proof enemy.

## Current Slice

- `AEnemyCharacter` owns `CombatAIComponent` by default.
- `UEnemyCombatAIComponent` remains the owner of combat target, attack selection, token requests, and attack execution state.
- Automation can inject a deterministic `UCombatTokenSubsystem` when synthetic worlds do not own a normal `GameInstance`.
- Attack execution failure releases tokens and returns to `Circling` or `Idle` instead of leaving enemies stuck in `Approaching`.
- Queued enemies leave the token queue if their combat target is cleared before a token is granted.

## Acceptance Checks

- Spawned `AEnemyCharacter` instances expose `CombatAIComponent`.
- Setting a combat target enters `Circling`; clearing it returns to `Idle`.
- A configured enemy can request a token, select an attack, and enter `Approaching`.
- A queued enemy receives the token when the current token holder is parried/interrupted.
- A queued enemy that loses its target does not receive the next released token.
- Failed attack execution cleans up token and selected attack state.

## Explicit Non-Goals

- Full production perception, patrol, alert, or group tactics.
- Authored Behavior Tree/Blackboard asset creation.
- Final animation polish.
- Map-level Chain Counter proof. That still requires a parryable enemy attack montage with `AnimNotifyState_ParryWindow` and editor/PIE evidence.
