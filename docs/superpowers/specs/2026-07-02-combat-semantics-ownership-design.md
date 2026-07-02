# Combat Semantics Ownership Design

Date: 2026-07-02
Status: Accepted design direction before implementation

## Problem

KatanaCombat uses a mix of gameplay tags, enums, booleans, and data references. That mix is valid, but it must be intentional. The failure mode to avoid is split-brain behavior where designers author tags that imply gameplay, while C++ booleans or enums silently make the real decision.

## Ownership Rules

### Enums

Use enums for closed runtime identities and state machines. These values are code contracts and should remain switch-friendly, testable, and hard to author incorrectly.

Examples: `ECombatState`, `EAttackPhase`, `EInputType`, `EAttackType`, `EAttackDirection`, `EChainCounterState`, `ECounterSystemMode`, `EPairedReactionType`, and `EReactionOutcome`.

Do not replace phase, combat state, chain state, input identity, or resolved outcome with tags.

### Booleans

Use booleans for local runtime latches or direct authored gates with one clear meaning.

Examples: `bIsBlocking`, `bBlockCombatInput`, `bParryWindowActive`, `bCounterWindowActive`, `bCanTriggerFinisher`, and `bHasCounterVariant`.

Authored booleans that duplicate data references must be validated. If `bHasCounterVariant` is true, `CounterData` should exist unless the missing data is deliberately reported as a readiness gap. If `bCanTriggerFinisher` is true, `FinisherData` should exist unless the branch is intentionally relying on a fallback.

### Gameplay Tags

Use gameplay tags for open-ended authored semantics: capabilities, properties, style categories, and contextual eligibility. Tags are appropriate when content needs composable rules or future expansion without adding a new C++ field for every case.

Examples: `Attack.Capability.CanCombo`, `Attack.Capability.CanDirectional`, `Attack.Property.Unblockable`, `Context.ParryCounter`, and future defense tags such as `Attack.Defense.BlockInterruptible` or `Attack.Defense.Parryable`.

Gameplay-relevant tags must have concrete runtime, validation, or reporting consumers before asset behavior is claimed.

Semantic tag names should have one code-facing accessor surface. Runtime, editor tooling, and tests should call shared combat tag accessors instead of repeating string literals for `Attack.Property.Unblockable`, `Context.ParryCounter`, or `Context.LowHealthFinisher`.

### Data References

Use object references for payload ownership: the montage, paired animation, hit reaction, VFX, audio, or other authored asset that will actually play or spawn.

Examples: `CounterData`, `FinisherData`, attack montages, hit reaction data, impact VFX, impact audio, and future block/recoil animation data.

Do not use a tag to imply that a payload exists. Tags answer when and why something is eligible; data references answer what concrete asset is used.

## Defense And Counter Application

Defense resolution should follow this shape:

1. Read attacker `UAttackData`, defender state, active context tags, attack phase/window state, facing/alignment, and team policy.
2. Consume authored attack properties and requirements from tags, including unblockable or future block-interruptible/parryable semantics.
3. Return one explicit resolved enum outcome such as hit, block, perfect parry, counter start, guard break, or unblockable hit.
4. Execute the outcome through existing components using data references for animations, paired data, VFX, and audio.

This keeps the authored layer flexible while preserving deterministic runtime state.

## Current Reconciliation Targets

- `Attack.Type.*` tags overlap with `EAttackType`. Keep `EAttackType` as the hard input and branch identity. Use type tags only for queries, grouping, or content validation unless a future design proves broader need.
- `RequiredContextTags` is the contextual eligibility contract and is now evaluated by runtime attack resolution for already-linked resolver candidates. Global discovery of arbitrary context-sensitive attacks is a separate design task.
- `Attack.Property.Unblockable` is the authored unblockable contract and is now inspected by `ABaseCombatCharacter` hit/block resolution when concrete `AttackData` is present.
- `EResolutionPath` should stay structural unless a task explicitly changes its contract. A normal combo gated by `Context.ParryCounter` is still a normal combo path; the tag controls eligibility, not the outcome identity.
- `bHasCounterVariant` and `bCanTriggerFinisher` remain acceptable only if validation treats missing `CounterData` or `FinisherData` as an explicit readiness warning.
- Chain Counter state remains enum-driven. Tags may gate which attacks or follow-ups are eligible in a Chain context, but they do not replace `EChainCounterState`.

## Validation Requirements

- Asset validation should report contradictions between tags, booleans, and data references.
- Commandlet readiness reports should include tag-driven capability/context warnings alongside `CounterData`, `FinisherData`, parry/counter window, paired sync, and collision readiness. Reports should surface authored semantic tags as soon as `AttackData` is non-null, even when montage, section, or timing validation fails.
- Tests should prove runtime consumers for gameplay-relevant tags before docs or tools describe those tags as authoritative.
- Runtime context tags need explicit lifecycle ownership before gameplay systems produce them. A context tag is not complete until its add, remove, cancel, timeout, owner-death, partner-death, and normal-completion paths are specified and tested. Until then, context-tag write APIs should remain C++ only; Blueprint write access can be added after lifecycle ownership is proven.
- Null `FHitReactionInfo::AttackData` should preserve existing normal block behavior. The unblockable tag only bypasses block when concrete attack data exists and carries the tag.

## Non-Goals

- Do not migrate runtime state machines to tags.
- Do not add booleans for every new defense property.
- Do not auto-seed parry, counter, sync, or collision windows from tags without reviewed timing policy and asset-level proof.
- Do not treat unblockable-tag consumption as the full block system. Normal block, perfect parry, counter start, guard break, attacker recoil, block VFX/audio, and block alignment require a separate outcome matrix.
- Do not claim unblockable behavior for generic `IDamageableInterface` implementers until they have a hit-aware defense path. This slice targets `ABaseCombatCharacter` damage and hit-impact classification.
