# Micro-Spec 01: Lifecycle Crash Hardening

## Purpose

Make enemy attack start and termination safe when combat execution synchronously consumes the attack, kills an actor, stops a montage, clears selected data, or broadcasts into code that does so.

## Evidence

The reported crash dereferences `SelectedAttack->AttackMontage` after `CombatComponent->ExecuteAttackData(...)`. That call can enter damage, defense, paired, montage, and delegate paths that mutate the AI component before control returns.

Installed UE 5.6 source establishes the relevant pattern:

- `AnimInstance.cpp` montage start/end and notify delegates can execute arbitrary callbacks and invalidate participants.
- `AbilitySystemComponent_Abilities.cpp` performs active-state teardown under explicit reentrancy guards and separates cancellation from blocking.

This follows the engine's snapshot/revalidate model rather than assuming a successful call preserves caller state.

## Contract

`ExecuteAttack()` must:

1. Validate and snapshot owner, combat target, selected `UAttackData`, selected montage, anim instance, combat component, and token ownership before the first external call.
2. Set only the minimum provisional AI state needed to execute.
3. Call `ExecuteAttackData()` without reading mutable `SelectedAttack` afterward.
4. Revalidate the component, owner, target, combat component, anim instance, AI state, and returned attack generation.
5. If the generation is already consumed or the owner became terminal, run exact-once termination and return without binding a montage delegate.
6. Bind completion to the snapshotted montage and exact attack generation.
7. Publish `OnAttackStarted` only after ownership is committed and revalidate after the broadcast.

Every terminal path uses `TerminateActiveAttack()` or one exact lower-level cleanup primitive. Releasing a token, unbinding delegates, clearing selected data, and recording a consumed generation are idempotent.

## Identity Rules

- `FAttackInstanceId` is the gameplay identity.
- Montage pointer is presentation identity only and cannot substitute for attack generation.
- A late montage callback may terminate only the generation it captured.
- A consumed callback received twice records one terminal result and releases one token.
- StateTree observes the generation started by its own task; a newer attack cannot satisfy an older task.

## Required Failure Cases

| Case | Required result |
|---|---|
| `ExecuteAttackData()` returns false | provisional state rolls back; token released once |
| call returns true but attack was synchronously consumed | no dereference/bind; task reaches terminal result |
| owner dies during call | death remains terminal; no recovery timer or broadcast |
| target is destroyed during call | attack terminates or continues only if CombatComponent still owns a valid target-independent generation |
| montage start callback clears `SelectedAttack` | snapshotted montage remains safe; mutable field is not read |
| stale montage-end callback arrives | ignored by generation mismatch |
| `OnAttackStarted` destroys owner | no subsequent owner/component access |

## Scope

This slice changes lifecycle safety only. It does not change attack selection, attack pressure, alignment, orbit, or reaction policy.

## Acceptance

- A focused automation test reproduces synchronous state invalidation without crashing.
- Tests prove exact-once token release, no stale-generation termination, and terminal death precedence.
- `KatanaCombatEditor` builds and `KatanaCombat.EnemyAI` focused tests pass.
- PIE can run the original Defense Matrix attack-start scenario repeatedly without the reported access violation.
