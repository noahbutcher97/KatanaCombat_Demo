# Research References

Date: 2026-07-18

This appendix records the primary references used to choose the stabilization architecture. It is research context, not runtime authority; the master design and micro-specs own project behavior.

## Action Arbitration

### Lyra Activation Groups

[Abilities in Lyra](https://dev.epicgames.com/documentation/en-us/unreal-engine/abilities-in-lyra-in-unreal-engine) defines Independent, Exclusive Replaceable, and Exclusive Blocking activation groups and uses tag relationships for cancellation/blocking.

Project use: adopt explicit coexist/replace/block decisions in a small pure resolver. Do not adopt GAS, Lyra ability classes, or a new action component.

### Gameplay Ability Cancellation And Tags

[Using Gameplay Abilities](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-gameplay-abilities-in-unreal-engine) documents required, blocked, cancel, and block tags. [Using Gameplay Tags](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-gameplay-tags-in-unreal-engine) defines hierarchical semantic labels.

Project use: keep cancellation and blocking distinct. Use enums for closed outcomes/policies, booleans for local gates/held state, tags only for open-ended authored semantics with a runtime and validator consumer.

## Damage And Reaction Separation

Lyra's damage flow in [Abilities in Lyra](https://dev.epicgames.com/documentation/en-us/unreal-engine/abilities-in-lyra-in-unreal-engine) separates source magnitude, target health, team filtering, and cue presentation. [Gameplay Effects](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-effects-for-the-gameplay-ability-system-in-unreal-engine) treats Gameplay Cues as cosmetic response.

Project use: silently commit actual health, commit deterministic reaction/action state, then play optional presentation and broadcast public events. Keep native KatanaCombat damage instead of adding GAS.

## Animation Composition

### Slots And Groups

[Animation Slots](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-slots-in-unreal-engine) explains that montages in one group interrupt one another and that Slots can be layered by bone.

Project use: keep the existing primary full-body group; create separate non-root-motion guard and additive-reaction groups. Slot conflict is not gameplay policy.

### Layering, Masks, And Additive Type

- [Using Layered Animations](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-layered-animations-in-unreal-engine)
- [Blend Masks And Blend Profiles](https://dev.epicgames.com/documentation/en-us/unreal-engine/blend-masks-and-blend-profiles-in-unreal-engine)
- [Animation Blueprint Blend Nodes](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-blend-nodes-in-unreal-engine)
- [Animation Sequence Editor](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-sequence-editor-in-unreal-engine)

Project use: use Local Space additive derivatives for flinches/guard where the audited source supports them, a reviewed attack-safe blend mask, and visible socket-drift proof. Mesh Space remains appropriate for aim-offset style content, not the default flinch choice.

### Root Motion And Motion Warping

- [Root Motion](https://dev.epicgames.com/documentation/en-us/unreal-engine/root-motion-in-unreal-engine)
- [Motion Warping](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-warping-in-unreal-engine)
- [Animation Notifies](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-notifies-in-unreal-engine)

Project use: keep attack/paired root motion on the exclusive primary path, require overlays to have no root motion, and use authored notify windows plus the existing owned Motion Warping executor for attack alignment. Live player steering changes only the exact regular attack's owned rotation target; it does not poll input or own a second root-motion path.

### Comparative Project Precedent

A read-only pass over OperationPhoenix/OnSight examined its `EOSBlendAlgorithm`, `FOSAttackDirectionParams`, `UOSCombatLib::DispatchBlendAlgorithm`, `UOSGameplayAbility::RefreshWarpTarget`, and motion-warping contract tests. Its angular weighted, sticky/curve, and cone-clamped policies demonstrate useful designer-facing options. KatanaCombat does not copy the surrounding GAS/replication lifecycle, multi-source raw-input fallback, target-breakaway coupling, hard-facing targeted refresh, or source-text tests. It instead uses a world-free resolver, one terminal-aware input sample, exact alignment ownership, actor-yaw tests, and runtime reachability clamps.

## StateTree And Navigation

### StateTree Concurrency

[StateTree Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-state-tree-in-unreal-engine) documents concurrent tasks within active states and task completion driving transitions.

Project use: circling stays `Running`; token request completion owns the transition. Task exit cleans only movement/focus/request state it owns.

### EQS

- [Environment Query System Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/environment-query-system-overview-in-unreal-engine)
- [Environment Query System User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/environment-query-system-user-guide-in-unreal-engine)

Project use: EQS is a valid later backend for scored tactical points, but it does not solve the current self-aborting path-request loop. Start with deterministic annular geometry.

### Avoidance

[Using Avoidance With The Navigation System](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-avoidance-with-the-navigation-system-in-unreal-engine) describes RVO and Detour Crowd as alternative approaches and warns against using both together.

Project use: enable neither in the stabilization slice. Evaluate exactly one only if stable orbit still demonstrates collision/spacing failures.

## Input, Validation, And Tests

- [Enhanced Input](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine) covers modifiers, dead zones, trigger events, and continuous values.
- [Data Validation](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-validation-in-unreal-engine) covers `IsDataValid`, editor validators, and commandlet validation.
- [Automation Test Framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine) covers unit/feature/content tests and disk-state isolation.
- [Animation Modifiers](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-modifiers-in-unreal-engine) supports repeatable apply/revert animation transformations.

Project use: derive one movement-intent edge from post-modifier input, validate every new policy/data boundary, keep tests independent, and run animation transforms only on allowlisted project-owned derivatives.

## Installed UE 5.6 Source Findings

These conclusions are version-specific and should be refreshed if the engine version changes.

| Engine source | Observed behavior | Design consequence |
|---|---|---|
| `Engine/Source/Runtime/AIModule/Private/AIController.cpp` (`MoveToLocation`) | Aborts active movement before submitting the new request; returns a path-following request result. | Do not reissue orbit moves on a timer without a replacement reason; use explicit `FAIMoveRequest` and track result/ID. |
| `Engine/Source/Runtime/NavigationSystem/Private/NavigationSystem.cpp` (`ProjectPointToNavigation`) | Projects through nav data/default query extent. | Project orbit points and handle failure instead of submitting raw geometry. |
| `Engine/Plugins/Animation/MotionWarping/Source/MotionWarping/Private/RootMotionModifier.cpp` | Constant-rate yaw uses delta time multiplied by montage play rate. | Retain KatanaCombat's simulation-time play-rate normalization. |
| `Engine/Plugins/Animation/MotionWarping/Source/MotionWarping/Private/MotionWarpingComponent.cpp` | `OnPreUpdate` broadcasts before modifier updates and active modifiers process root motion only after target/state refresh. | Do not drive steering from a later actor tick; update within the owned modifier pipeline. |
| `Engine/Plugins/Animation/MotionWarping/Source/MotionWarping/Private/RootMotionModifier.cpp` (`URootMotionModifier::Update`, `URootMotionModifier_Warp::Update`) | The update delegate receives current animation position/play rate before the warp subclass samples the named target; changing a target resets the modifier's start transform. | Publish the exact owner's live rotation in that callback, apply meaningful-delta hysteresis, and avoid broad remove/re-add churn. |
| `Engine/Source/Runtime/Engine/Private/Animation/AnimInstance.cpp` | One montage per group, one root-motion montage globally; montage/notify callbacks can reenter and invalidate state. | Separate overlay groups, prohibit overlay root motion, and revalidate after every montage/callback boundary. |
| `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Private/AbilitySystemComponent_Abilities.cpp` | Cancellation iterates active specs under a scope lock; teardown explicitly guards reentry and separates block/cancel behavior. | Use exact generation, idempotent teardown, and distinct voluntary cancellation/external interruption. |
| `Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h` and implementation | desired/orient-to-movement rotation uses `RotationRate`. | Expose locomotion yaw independently; never treat it as attack alignment proof. |
| `Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp` (`DisableMovement`) | Disabling movement sets `MOVE_None`; it is a shared movement mode, not an owner-scoped input gate. | Ordinary hold/action suppression must gate movement application instead of setting/restoring a mode that paired/death may own. |
| `Engine/Source/Runtime/Engine/Private/Pawn.cpp` and `PawnMovementComponent.cpp` (`AddMovementInput`) | The input vector can be forwarded independently from CharacterMovement's active physics mode. | Record capture separately from policy/application and observed movement; a callback firing does not prove locomotion occurred. |
| `Engine/Plugins/EnhancedInput/Source/EnhancedInput/Private/EnhancedPlayerInput.cpp` | `Triggered -> None` emits Completed, while `Ongoing -> None` emits Canceled; neither is another Triggered sample. | Bind terminal events explicitly, clear movement/held state on both, and do not treat Canceled attack input as a release-triggered hold follow-up. |

## Project-Local Precedents

- `FDefenseResolver` already demonstrates immutable query/pure decision/ordered commit.
- defense alignment already owns request generations, one-writer arbitration, turn budgets, and play-rate compensation.
- attack window identity and defense interaction caches already demonstrate stale-callback and duplicate-suppression patterns.
- `KatanaAssetMigrationRunner` already provides audit/plan/apply modes, approved-plan binding, and explicit save gates.
- defense Gate A/Gate B tests already separate headless contract evidence from PIE-visible acceptance.

The new work should reuse these patterns instead of creating parallel frameworks.
