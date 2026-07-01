# Counter Chain Rollout Inventory

> Created: 2026-07-01
> Branch: `feature/counter-chain-rollout-inventory`
> Scope: inventory and plan-only evidence for expanding counter-chain asset coverage after the `LightAttack_1` proof path.

## Current Evidence

Fresh commandlet evidence was generated from current `main` after PR #115:

- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-attackdata-audit.json`
  - `AttackDataNotifyMigration`, `Audit`, global scan.
  - Result: 20 targets, 20 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-branch-critical-readiness.json`
  - `ContentReadinessAudit`, explicit branch-critical target list.
  - Result: 41 targets, 41 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-light-plan.json`
  - `CounterChainProofMigration`, `Plan`, using `Config/AssetMigrations/CounterChainRolloutLightTargets.txt`.
  - Result: 10 targets, 10 would change, 0 failed.

The plan run is read-only. It did not save packages. Commandlet console warnings about `Counter_LightAttack_2..11` are expected because those assets are proposed creations and do not exist yet.

## Canonical State

`LightAttack_1` is the only asset-backed counter proof path. It has:

- `CounterData = Counter_LightAttack_1`
- `bHasCounterVariant = true`
- a section-scoped `AnimNotifyState_CounterWindow` on `AM_Light_Combo_1`
- valid `FinisherData`

The other light-chain attacks are structurally ready for the same commandlet migration pattern, but they are not authored counter-animation polish. The current migration creates nonlethal counter data by copying `Finisher_A` defaults and setting `ReactionType=Counter`.

## Candidate Classification

| Lane | Assets | Evidence | Recommendation |
| --- | --- | --- | --- |
| Already proven | `LightAttack_1` | Counter data, counter window, finisher data all present | Do not re-migrate unless idempotence fails |
| Commandlet-safe light rollout | `LightAttack_2` through `LightAttack_11` | Plan accepted all 10, each with 3 planned changes and 0 errors | Apply in small montage batches after review |
| Needs authored/design review | `HeavyAttack_1` through `HeavyAttack_4` | Load and section valid, but no `FinisherData`, no `CounterData`, no `CounterWindow` | Do not run counter-proof migration until finisher/counter intent is defined |
| Needs authored/design review | Directional attacks `B`, `F`, `L`, `None`, `R` | Load and section valid, but no `FinisherData`, no `CounterData`, no `CounterWindow` | Classify whether directional attacks participate in Chain Counter before migration |
| Runtime proof blocker | Enemy parryable attack path | No audited AttackData row reports `has_parry_window=true` | Identify or author one attacker-side `ParryWindow` before claiming map-level chain proof |

## Planned Light Rollout Batches

Recommended apply order:

1. `LightAttack_2` and `LightAttack_3`
   - Touches only the already-counter-proofed montage family: `AM_Light_Combo_1`.
   - Lowest-risk next package-save batch.
2. `LightAttack_4` through `LightAttack_6`
   - First counter-window edits on `AM_Light_Combo_2`.
3. `LightAttack_7` through `LightAttack_11`
   - First counter-window edits on `AM_Light_Combo_3`.

Each batch should use `CounterChainProofMigration` in this sequence:

1. `Plan` with an exact reviewed target file.
2. `ApplyAndSave` with `-AllowPackageSave`.
3. `ContentReadinessAudit` over the touched AttackData and counter data assets.
4. `CounterChainProofMigration` `Plan` again to prove idempotence.

## Expected Per-Target Changes

For each light-chain target, the plan report proposes:

- Create `/Game/ProjectFiles/Data/PDA/Paired/Counters/Counter_<AttackName>` from `Finisher_A`.
- Set the corresponding `UAttackData` counter fields.
- Seed a section-scoped `AnimNotifyState_CounterWindow` on the owning light montage section.

## Adversarial Notes

- This rollout proves asset wiring, not final authored animation quality.
- Template-derived counter data may be visually wrong even when commandlet validation is green.
- Broad application would touch three light montages and ten AttackData assets; batching by montage keeps review and rollback small.
- Heavy and directional attacks are not safe automatic candidates because the current evidence does not show a full Chain destination via `FinisherData`.
- A map-level proof still requires an enemy attack montage with an attacker-side `ParryWindow`; none is present in the audited 20 AttackData rows.
