# KatanaCombat Architecture Routing

## Core Runtime

- Combat state, attack execution, input queue: `Source/KatanaCombat/Public/Core/CombatComponent.h` and matching `Private/Core`.
- Targeting and soft lock: `TargetingComponent`.
- Weapon traces and hit detection: `WeaponComponent`.
- Damage reception, hit reactions, death: `HitReactionComponent`.
- Finishers, counters, sync, partner state: `PairedAnimationComponent` and `docs/specs/PAIRED_ANIMATION_SPEC.md`.

## Data

- Attack definitions: `Source/KatanaCombat/Public/Data/AttackData.h`.
- Attack packages: `AttackConfiguration`.
- Tuning: `CombatSettings`, `HitReactionSettings`, motion/targeting settings.
- Shared enums, structs, and cross-component delegates: `Source/KatanaCombat/Public/CombatTypes.h`.

## Animation

- Phase transitions: `AnimNotify_AttackPhaseTransition`.
- Overlapping windows: `AnimNotifyState_ComboWindow`, `HoldWindow`, `ParryWindow`, `CounterWindow`.
- Paired animation sync/collision: `AnimNotifyState_PairedAnimationSync`, `AnimNotifyState_PairedAnimationCollision`.

## Editor Tooling

- Editor module: `Source/KatanaCombatEditor/`.
- Slate views and paired-animation preview tooling: `Public/Views`, `Private/Views`.
- Keep editor-only dependencies out of runtime modules.

## Tests

- Automation module: `Source/KatanaCombatTest/`.
- Test helpers: `Public/CombatTestHelpers.h`.
- Name paths as `KatanaCombat.<System>.<Behavior>`.
