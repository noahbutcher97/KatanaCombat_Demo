# Micro-Spec 06: Animation Layering And Content Migration

## Purpose

Provide animation assets and AnimBP composition for locomoting guard, distinct parry feedback, additive flinches, and full-body interruption without allowing montage topology to decide gameplay.

## Precedent

Epic's [Animation Slots documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-slots-in-unreal-engine) states that montages in the same group interrupt one another and that slots can be layered by bone. Epic's [layered animation guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-layered-animations-in-unreal-engine), [blend mask documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/blend-masks-and-blend-profiles-in-unreal-engine), and [blend node reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-blend-nodes-in-unreal-engine) establish the composition tools. Epic's [Animation Sequence Editor documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-sequence-editor-in-unreal-engine) distinguishes Local Space additive animation from Mesh Space aim-offset use.

Root-motion montages remain globally exclusive in UE 5.6. Overlay assets in this spec therefore contain no root motion.

## Lane Topology

- Keep `DefaultGroup.DefaultSlot` as the existing primary full-body lane for this migration.
- Add `GuardOverlay.Guard` for held guard and block/parry overlay presentation.
- Add `AdditiveReaction.Reaction` for non-owning flinches.
- Do not rename or bulk-migrate the primary lane until a separate inventory proves the value.

Gameplay resolvers select a presentation class/key. Slot/group interruption is a consequence of the selected class, never the policy input.

## Guard Presentation

- Held guard is a looping, non-root-motion upper-body/additive overlay over locomotion.
- Normal block impact is a short context-selected overlay and may temporarily override the held-guard pose without ending guard intent.
- Perfect parry has a visibly distinct defender response and existing attacker recoil/bridge behavior.
- A hard reaction suspends guard activity; guard resumes only through the action policy.
- Missing overlay content falls back to a stable guard pose, not a full-body sliding montage.

## Reaction Presentation

- Additive flinches use the three existing heights and three incoming lanes.
- Sparse data is legal, but fallback order is deterministic: exact height/lane, same height/center, middle/same lane, middle/center, no cosmetic response.
- Full-body reaction uses existing light/heavy assets initially and owns action lock/interruption.
- Stagger and knockdown use explicit full-body presentations and never become additive through fallback.
- Death remains terminal and is not layered over an active attack.

## Weapon-Safe Additive Rule

The first pass must not change active weapon trace trajectory. The AnimBP uses a project-owned attack-safe blend mask that excludes the weapon-driving chain during an active attack. PIE telemetry compares weapon socket transforms with additive response enabled/disabled; drift above the accepted tolerance blocks the asset.

## Content Ownership And Migration

- Imported assets are read-only sources.
- Create project-owned derived sequences/montages under `Content/ProjectFiles/Animation/Defense/` and `Content/ProjectFiles/Animation/HitReactions/Additive/`.
- Modify only explicitly allowlisted project Skeleton, AnimBP, settings, montage, and derived sequence packages.
- Use the established commandlet audit/plan/apply flow. Audit and plan write reports only; apply requires an explicit package-save gate.
- `Lvl_DefenseMatrix`, `Lvl_ThirdPerson1`, and their external actors are excluded from every save allowlist.
- `DA_CombatSettings_Default` and `DA_CombatSettings_DefenseMatrix` switch from `Legacy` to `DeterministicLayered` only in the final reviewed asset apply after every referenced data/animation package validates.

Epic's [Animation Modifiers documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-modifiers-in-unreal-engine) supports repeatable apply/revert transformations, but modifiers may be used only on project-owned derivatives.

## Validation

Validators reject:

- overlay montages with root motion;
- missing slot/group topology;
- primary full-body assets assigned to additive keys;
- stale skeleton or incompatible animation references;
- non-finite blend/timing values;
- missing middle/center fallback;
- imported package paths in the writable manifest;
- unreviewed assets outside the package allowlist.

## Acceptance

- Guard movement visibly uses locomotion with no sliding.
- normal block and perfect parry are visually and audibly distinguishable.
- all nine height/lane keys resolve deterministically, with representative visible coverage for each row and column.
- additive trades do not stop attacks or exceed weapon-socket drift tolerance.
- full-body reactions interrupt and recover without pose freeze or snap.
- commandlet apply modifies only reviewed project-owned packages.
