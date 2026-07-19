# Micro-Spec 05: Hit Reactions And Trades

## Purpose

Separate damage, interruption, reaction state, and animation presentation so attacks can trade without arbitrary full-body cancellation while sufficiently forceful hits still interrupt predictably.

## Precedent

Lyra separates source-side damage calculation, target health mutation, team filtering, and cosmetic Gameplay Cues. Unreal's [Gameplay Effects documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-effects-for-the-gameplay-ability-system-in-unreal-engine) likewise treats Gameplay Cues as cosmetic response rather than the gameplay mutation itself. KatanaCombat keeps its native damage path but adopts that separation.

## Typed Query And Decision

Add a pure `FHitReactionResolver` with:

- `FHitReactionQuery`: committed hit, resolved damage, damage ratio, attacker/defender action snapshots, normalized pressure, active-action resistance, impact policy, height, lane, bone provenance, and terminal flags.
- `FHitReactionDecision`: reaction class, reason, whether the primary action is interrupted, action-lock duration, whether finisher vulnerability is granted, and a presentation key.

Closed reaction classes are:

- `DamageOnly`
- `AdditiveFlinch`
- `FullBodyReaction`
- `Stagger`
- `Knockdown`
- `Death`

`EReactionOutcome` remains the post-animation `StandardRecovery`, `Death`, or `Ragdoll` result. It is not duplicated or repurposed.

## Authored Semantics

`UAttackData::StaggerPower` becomes deterministic normalized impact pressure. Its comments and validation change from chance language to pressure language; there is no random roll and no accumulation.

Add a closed impact-policy enum for `Default`, `DamageOnly`, `ForceFullBody`, `ForceStagger`, and `ForceKnockdown`. This is a finite gameplay rule and therefore an enum, not a tag. Open-ended attack semantics continue to use Gameplay Tags only when the same slice adds a runtime and validator consumer.

Attack data also owns a phase-resistance profile for attacks. Conservative defaults preserve light trades and allow authored heavy interruption. Idle, reaction, stagger, paired, and death resistance come from closed system defaults or the active reaction decision, not arbitrary tags.

Initial data-migration baselines are light pressure `0.35`, heavy pressure `0.75`, and special pressure `1.0`; attack resistance defaults are windup `0.25`, active `0.60`, and recovery `0.20`. These values make light hits interrupt windup/recovery but trade with an active committed attack, while heavy hits interrupt all default attack phases. Every active `UAttackData` is audited and assigned intentionally before the resolver is enabled. Values are tuning baselines, not final balance.

`UAttackData::HitStunDuration` is the compatibility source for full-body action-lock duration until it can be renamed through a reviewed asset migration. Presentation entries (`FHitReactionEntry` and `UHitReactionData`) do not independently grant gameplay stun; their duplicate `StunDuration` fields are inventoried, migrated to attack/reaction policy where needed, then deprecated. Explicit stagger duration remains a separate gameplay value.

Existing `ReactionTags` and required-context tag fields have no current runtime selector. This design does not make them authoritative by accident. The migration inventories them; empty fields are deprecated, while any populated semantic value must be mapped to a typed policy or receive an explicit resolver plus validator before it can survive.

`UCombatSettings` owns a closed reaction runtime mode: `Legacy` or `DeterministicLayered`. New source lands in `Legacy`, while tests and the transient proof fixture can explicitly exercise the new mode. Active settings assets switch to `DeterministicLayered` only after pressure data, selector data, slots, AnimBP layers, and validators are all ready. The mode is an intentional compatibility/rollback boundary, not attack semantics.

## Resolution Rules

1. Validate committed damage and terminal state.
2. Death supersedes every nonlethal reaction.
3. Owning paired state follows its existing partner/terminal contract.
4. Explicit impact policy resolves before the default pressure comparison.
5. For default policy, pressure greater than resistance interrupts and selects full-body response.
6. Pressure less than or equal to resistance preserves the primary action and selects additive flinch.
7. Damage ratio may select light/heavy cosmetic intensity but does not alter interruption unless a future accepted design says so.
8. Missing presentation never changes committed gameplay; fallback is deterministic.

## Super Armor Compatibility

`bHasSuperArmor` remains temporarily supported as `DamageOnly` plus maximum default resistance for nonlethal default-policy hits. It is deprecated only after an asset/property inventory. Explicit death and paired terminal rules still win. No asset is silently rewritten.

## State And Finisher Semantics

- Full-body reaction action lock blocks voluntary actions until a cancel boundary or completion.
- Full-body reactions carry a default 0.12-second minimum uncancellable interval before any authored cancel window may take effect.
- Additive flinch owns presentation generation only and does not set action lock.
- Only `Stagger` sets the event-driven stagger vulnerability used by finishers.
- Ordinary `bIsStunned`/action lock no longer grants finisher eligibility.
- Low-health finisher eligibility remains independent.
- Existing stale tests that require ordinary stun to produce `EFinisherTriggerReason::Stunned` are replaced with tests for explicit stagger and action-lock separation.

## Damage Commit Ordering

The target-authorized path snapshots attacker and defender actions before health mutation, resolves accepted damage, and silently commits actual health. It then builds the reaction query from those pre-impact action snapshots, actual applied damage, and the post-health terminal flag. This preserves the exact action to interrupt while still giving death precedence. It commits the reaction decision and exact action interruption before finalizing the contact record. Optional presentation starts after finalization, followed by immutable damage/reaction/health delegates. Every external call is followed by participant/generation revalidation. The generic `ApplyDamage` compatibility path must converge on the same ordering rather than retaining its current broadcast-before-reaction sequence.

`OnDamageReceived` remains a raw committed-damage compatibility event. AI does not infer interruption from it. `UHitReactionComponent` submits an exact external-interruption commit to `UCombatComponent`; attack interruption uses the existing generation-owned attack consume/termination path before presentation. A typed committed-reaction event remains observable context, not token authority. AI releases its token only for an exact interrupted generation, parry, death, or normal completion.

## Contact And Presentation Key

Use existing `EAttackHeight` (`High`, `Middle`, `Low`) and `EIncomingAttackLane` (`Left`, `Center`, `Right`). Preserve exact `BoneName`, impact point/normal, and provenance for IK, VFX, and telemetry. A settings-owned bone map derives height with an explicit middle fallback; source trajectory derives lane.

## Deferred Behavior

Additive flinches do not modify weapon traces, hit confidence, or damage scale in this pass. Any future opt-in requires explicit trace-order, socket-drift, and balance proof.

## Acceptance

- Pure tests exhaust policy, pressure/resistance equality, terminal precedence, missing assets, super armor, and all height/lane keys.
- Integration tests prove additive trades keep attacks and AI tokens active, while full-body interruption terminates exact generations once.
- Tests prove normal action lock is not finisher vulnerability and explicit stagger is.
- PIE shows light trades, heavy interruption, death precedence, and no frozen-pose regression.
