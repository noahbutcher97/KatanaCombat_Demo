# Combat Semantic Runtime Audit

Date: 2026-07-02
Branch: `feature/basic-combat-proof-enemy`
Scope: source audit for gameplay-tag, enum, boolean, and data-reference ownership before implementation.

## Executive Verdict

The ownership model is sound, and the implementation slice has made two previously non-authoritative authored tag contracts runtime-authoritative in scoped paths:

- `RequiredContextTags` is consumed by contextual attack resolution for already-linked candidates.
- `Attack.Property.Unblockable` is consumed by `ABaseCombatCharacter` hit/block resolution when concrete `AttackData` is present.

The editor readiness lane now reports authored attack and context tags alongside parry/counter windows, `CounterData`, `FinisherData`, and lethal counter warnings. Context proof remains scoped to filtering already-linked attack candidates; it does not claim global counter or finisher discovery.

## Audit Table

| Semantic | Current Source Of Truth | Desired Owner | Runtime Gap | Validation Or Reporting Gap | Test Needed |
| --- | --- | --- | --- | --- | --- |
| Attack phase and combat state | `EAttackPhase`, `ECombatState`, `EChainCounterState` | Enums | No tag migration needed | None for this slice | Existing phase/window and Chain tests stay canonical |
| Block held state | `UCombatComponent::bIsBlocking` through `BeginBlock`, `EndBlock`, `CanBlockAttackFrom`, `CanBlockHit` | Boolean latch plus facing rule plus attack-property tag gate | `ABaseCombatCharacter` damage and hit-impact classification now reject unblockable attacks; generic `IDamageableInterface` targets still lack hit-aware defense input | Reports now show authored `AttackTags`, including unblockable tags | `Attack.Property.Unblockable` hit bypasses normal block while null `AttackData` preserves legacy block |
| Context-gated attack eligibility | `UAttackData::RequiredContextTags`, `UCombatComponent::ActiveContextTags` | Gameplay tags | `ResolveNextAttackContextual` now filters already-linked candidates by required context tags | C++ context mutators exist; reports now show required context tags | Context-required combo candidate is rejected without context, allowed with matching context, and not revived by emergency fallback |
| Attack capability consistency | `UAttackData::AttackTags`, `ValidateDirectionalFollowUps`, `ValidateTerminalTag` | Gameplay tags plus validation | Directional/terminal tags are validation-only; not all tags need runtime behavior | Validation covers directional and terminal; reports now surface authored semantic tags | Existing validation tests remain; semantic report tests cover authored tag visibility |
| Counter/finisher payloads | `CounterData`, `FinisherData`, `bHasCounterVariant`, `bCanTriggerFinisher` | Data references plus boolean readiness gates | Current Chain branch already routes selected `UAttackData::CounterData`; keep this model | `AttackDataNotifyGenerationService` reports missing/invalid readiness | Existing editor readiness tests remain canonical |
| Hit reaction context tags | `UHitReactionData::RequiredContextTags`, `ReactionTags` | Gameplay tags | No runtime selection path audited in this slice | Out of scope until hit-reaction selection is redesigned | Future modular hit-reaction plan |

## Evidence

- `UAttackData` declares `AttackTags` and `RequiredContextTags` in `Source/KatanaCombat/Public/Data/AttackData.h`.
- `UAttackData::IsDataValid` currently validates `Attack.Capability.CanDirectional` and `Attack.Capability.Terminal` consistency in `Source/KatanaCombat/Private/Data/AttackData.cpp`.
- `UCombatComponent::GetAttackForInput` passes `ActiveContextTags` into `UMontageUtilityLibrary::ResolveNextAttackContextual`.
- `ResolveNextAttackContextual` filters candidates whose `RequiredContextTags` are not satisfied by `ActiveContextTags`.
- `UCombatComponent::CanBlockAttackFrom` remains the facing/state check; `CanBlockHit` layers hit-aware unblockable-tag rejection on top.
- `ABaseCombatCharacter::ApplyDamage_Implementation` and `ABaseCombatCharacter::OnWeaponHitTarget` use `CanBlockHit(HitInfo)` for concrete hit/block classification.
- `AttackDataNotifyGenerationService` reports parry/counter windows, counter/finisher data readiness, `AttackTags`, and `RequiredContextTags`.

## Completed Implementation Slice

The completed slice made these changes:

1. Add shared combat gameplay-tag accessors, then make `RequiredContextTags` filter existing attack-resolution candidates through those accessors. Keep resolver path metadata structural and update comments so this is not mistaken for global context-sensitive attack discovery.
2. Add C++-only context-tag mutators on `UCombatComponent`. Do not expose context writes to Blueprint until Chain/finisher lifecycle ownership is specified.
3. Add `CanBlockHit(const FHitReactionInfo&)` so `ABaseCombatCharacter` block resolution can inspect `Attack.Property.Unblockable` without losing the existing facing check. Preserve normal block behavior when `HitInfo.AttackData` is null.
4. Surface `AttackTags` and `RequiredContextTags` in migration/readiness reports so content-authored semantic tags are visible during audits. Cover every current `FAttackDataNotifyAnalysis` to `FKatanaAssetMigrationRow` copy site: notify migration, timing migration, content readiness, and counter-chain proof migration. Populate semantic fields before montage, section, or timing early returns.

Do not add new defense tags until the broader block/counter outcome matrix is specified.

## Adversarial Notes

- This branch has pre-existing WIP in planned source files. Future implementation must use baseline diffs and hunk staging; plain path staging can capture unrelated work.
- Generic context mutators are not enough to make `Context.ParryCounter` gameplay-complete. A later behavior task must define add/remove ownership for parry success, counter advance, paired cancel, timeout, target death, owner death, and normal completion.
- `Attack.Property.Unblockable` consumption proves only one defense property. Normal block, perfect parry, counter start, guard break, attacker recoil, block VFX/audio, and motion-warped block alignment still need a separate outcome matrix before production tuning.
- Do not chain multiple `Automation RunTests ...` roots inside one `-ExecCmds` string. Use separate `UnrealEditor-Cmd.exe` invocations or a single broad root.
- Generic `IDamageableInterface` targets still expose only `IsBlocking()` and cannot inspect hit attack data. The unblockable slice is scoped to `ABaseCombatCharacter` until a hit-aware defense interface exists.

## Implementation Proof Update

- `RequiredContextTags` is now consumed by attack resolution for existing candidates.
- `Attack.Property.Unblockable` is now consumed by concrete hit/block resolution.
- Semantic combat tag names now route through shared runtime accessors.
- Readiness reports now surface `AttackTags` and `RequiredContextTags`, including invalid non-null `AttackData` rows that return early for missing montage, invalid section, or invalid timing.
- Null `FHitReactionInfo::AttackData` still preserves normal block behavior.
- Unblockable behavior is proven for `ABaseCombatCharacter` damage and impact classification only; generic `IDamageableInterface` targets still need a hit-aware defense API.
- Remaining tag contracts outside this slice, including hit-reaction context tags and future block/recoil tags, still require separate runtime proof.
