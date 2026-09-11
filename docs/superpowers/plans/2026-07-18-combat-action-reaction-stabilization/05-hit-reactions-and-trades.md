# Micro-Plan 05: Hit Reactions And Trades

**Spec:** `docs/superpowers/specs/2026-07-18-combat-action-reaction-stabilization/05-hit-reactions-and-trades.md`

**Goal:** Commit deterministic reaction decisions after actual health, support additive trades/full-body interruption, and separate action lock from finisher vulnerability.

## Task 1: Add Pure Resolver Red Tests

**Create:** `Source/KatanaCombatTest/Private/HitReactionResolverTests.cpp`

- [ ] Cover all reaction classes, reason codes, explicit impact policies, pressure/resistance equality, finite/range fallback, all action phases, super-armor compatibility, and missing presentation.
- [ ] Cover lethal precedence, paired suppression/partner path, low-health cosmetic intensity isolation, all nine height/lane keys, and bone fallback provenance.
- [ ] Add a test proving pressure does not accumulate and uses no randomness.

## Task 2: Define Reaction And Resistance Contracts

**Create:**

- `Source/KatanaCombat/Public/Reaction/HitReactionResolver.h`
- `Source/KatanaCombat/Private/Reaction/HitReactionResolver.cpp`

**Modify:**

- `Source/KatanaCombat/Public/CombatTypes.h`
- `Source/KatanaCombat/Public/Data/AttackData.h`
- `Source/KatanaCombat/Public/Data/CombatSettings.h`
- `Source/KatanaCombat/Private/Data/CombatSettings.cpp`

- [ ] Add closed impact policy, reaction class/reason, per-phase resistance profile, query, decision, presentation key, and committed-reaction event.
- [ ] Add `Legacy`/`DeterministicLayered` reaction runtime mode to CombatSettings; class and existing asset defaults remain `Legacy` during source rollout.
- [ ] Re-document `StaggerPower` as deterministic pressure and retain its serialized property name.
- [ ] Implement pure resolution with death/paired precedence and `pressure > resistance` interruption.
- [ ] Keep `EReactionOutcome` unchanged as post-animation state.

## Task 3: Commit Health Then Reaction Once

**Modify:**

- `Source/KatanaCombat/Public/Core/HitReactionComponent.h`
- `Source/KatanaCombat/Private/Core/HitReactionComponent.cpp`
- `Source/KatanaCombat/Public/Characters/BaseCombatCharacter.h`
- `Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp`
- `Source/KatanaCombatTest/Private/DamageApplicationTests.cpp`
- `Source/KatanaCombatTest/Private/DefenseContactTests.cpp`

- [ ] Extend `FCommittedHitReactionDamage` with immutable reaction decision and exact interrupted action identity; keep `FSilentHealthCommit` in the enclosing character-owned commit to avoid circular ownership.
- [ ] Snapshot attacker and defender action identities before health mutation. In rich contact, silently commit actual health, then build the query from pre-impact action snapshots plus actual damage and post-health death state.
- [ ] Commit interruption/action-lock/stagger state before finalizing the contact, then start presentation and broadcast delegates last.
- [ ] Make generic `ApplyDamage` converge on the same commit/dispatch sequence.
- [ ] Revalidate after montage operations, action cancellation, health/death handling, and every broadcast.
- [ ] Preserve `OnDamageReceived` as compatibility notification; do not make it an interruption signal.

## Task 4: Own Exact Reaction Lifecycle

**Modify:**

- `Source/KatanaCombat/Public/Core/HitReactionComponent.h`
- `Source/KatanaCombat/Private/Core/HitReactionComponent.cpp`
- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`

- [ ] Add monotonic reaction generation and exact active full-body/additive presentation records.
- [ ] Full-body reaction publishes primary action owner, generation, minimum lock, cancel state, and resistance.
- [ ] Additive reaction publishes presentation only and never blocks/cancels the primary action.
- [ ] Stale montage/notify callbacks close only their generation.
- [ ] Missing montage preserves gameplay state and uses timeout/explicit completion so the actor cannot remain locked forever.

## Task 5: Integrate Exact External Interruption And AI Tokens

**Modify:**

- `Source/KatanaCombat/Public/AI/EnemyCombatAIComponent.h`
- `Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp`
- `Source/KatanaCombat/Public/Core/CombatComponent.h`
- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- `Source/KatanaCombatTest/Private/EnemyCombatAITests.cpp`

- [ ] Add `CommitExternalInterruption` to validate the pre-impact action owner/generation and dispatch to its existing lifecycle owner before presentation.
- [ ] For Attack, consume/terminate the exact `FAttackInstanceId` through the existing internal attack event path; do not use a public reaction/damage delegate as authority.
- [ ] Ignore additive/damage-only results for active token lifecycle.
- [ ] Terminate the exact attack generation once for full-body interrupt, stagger, knockdown, or death, including montage callback reentry.
- [ ] Retain existing parry/counter and normal-completion reasons without double release.
- [ ] Audit and retire any generic `OnDamaged()` binding that still releases tokens without an exact committed interruption.

## Task 6: Separate Lock From Finisher Vulnerability

**Modify:**

- `Source/KatanaCombat/Private/Core/HitReactionComponent.cpp`
- `Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp`
- `Source/KatanaCombatTest/Private/HitReactionTests.cpp`
- `Source/KatanaCombatTest/Private/PairedAnimationTests.cpp`

- [ ] Remove ordinary `IsStunned()` from finisher eligibility and `ExecuteFinisher` admission.
- [ ] Keep action lock for animation/input policy; make explicit stagger the event-driven finisher trigger.
- [ ] Make `UAttackData::HitStunDuration` the compatibility action-lock source. Stop presentation rows from independently applying `StunDuration`.
- [ ] Replace stale tests expecting stun-based finishers with explicit stagger and low-health cases.
- [ ] Preserve compatibility enum values only where external API stability requires them; mark sunset behavior in docs/tests.

## Task 7: Migrate Active Attack Data Deliberately

**Create:**

- `Source/KatanaCombatEditor/Public/Commandlets/Operations/ActionReactionDataMigrationOperation.h`
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/ActionReactionDataMigrationOperation.cpp`
- `Tools/Codex/manifests/action-reaction-data.json`

**Modify:**

- `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
- `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`

- [ ] Audit every active `UAttackData`, `StaggerPower`, `HitStunDuration`, attack type, super-armor use, duplicate presentation `StunDuration`, reaction tags, required-context tags, and existing reaction settings without saving.
- [ ] Register audit/plan/apply routing and test option validation, approved-plan binding, dirty-package rejection, and explicit save gates.
- [ ] Commit migration source/tests after a successful no-save audit: `Add impact pressure data migration`.
- [ ] Produce a reviewed plan assigning initial light `0.35`, heavy `0.75`, special `1.0`, and justified overrides.
- [ ] Migrate populated duplicate stun/tag semantics to the typed authority, then deprecate only fields proven unconsumed or empty.
- [ ] Apply only after report review and explicit package allowlist; rerun audit to prove convergence.
- [ ] Add validators for finite pressure/resistance/policy and unreviewed active zero-pressure attacks.
- [ ] Leave active CombatSettings assets in `Legacy`; focused tests and the transient proof fixture explicitly opt into `DeterministicLayered` until animation assets land.

## Task 8: Verify And Commit

- [ ] Run reaction resolver, damage, hit reaction, death, AI, defense, and paired suites.
- [ ] PIE-prove light active-phase trade, light windup/recovery interrupt, heavy interrupt, exact token release, explicit stagger finisher, and lethal precedence.
- [ ] Confirm additive response does not alter trace/damage yet.
- [ ] Commit source/tests first: `Resolve deterministic hit reactions and trades`.
- [ ] Commit reviewed data assets separately: `Tune initial impact pressure profiles`.

**Gate:** Any damage-driven raw AI cancellation, ordinary-stun finisher eligibility, token imbalance, or nondeterministic decision blocks animation migration.
