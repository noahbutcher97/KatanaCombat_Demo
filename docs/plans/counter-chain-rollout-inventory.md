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
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-batch1-save.json`
  - `CounterChainProofMigration`, `ApplyAndSave`, using `Config/AssetMigrations/CounterChainRolloutBatch1Targets.txt`.
  - Result: 2 targets, 2 saved, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-batch1-readiness.json`
  - `ContentReadinessAudit`, using `Config/AssetMigrations/CounterChainRolloutBatch1ReadinessTargets.txt`.
  - Result: 4 targets, 4 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-batch1-postsave-plan.json`
  - `CounterChainProofMigration`, `Plan`, using `Config/AssetMigrations/CounterChainRolloutBatch1Targets.txt`.
  - Result: 2 targets, 2 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-attackdata-audit-after-batch1.json`
  - `AttackDataNotifyMigration`, `Audit`, global scan after batch 1 save.
  - Result: 20 targets, 20 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-branch-critical-readiness-after-batch1.json`
  - `ContentReadinessAudit`, branch-critical target list after batch 1 save.
  - Result: 41 targets, 41 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-light-plan-after-batch1.json`
  - `CounterChainProofMigration`, `Plan`, using `Config/AssetMigrations/CounterChainRolloutLightTargets.txt` after batch 1 save.
  - Result: 10 targets, 2 unchanged, 8 would change, 0 failed.

The initial full light-chain plan run is read-only. Commandlet console warnings about missing counter assets are expected in plan mode when those assets are proposed creations and do not exist yet.

## Canonical State

`LightAttack_1` through `LightAttack_3` are asset-backed counter proof paths. They have:

- `CounterData = Counter_LightAttack_<N>`
- `bHasCounterVariant = true`
- section-scoped `AnimNotifyState_CounterWindow` entries on `AM_Light_Combo_1`
- valid `FinisherData`

The other light-chain attacks are structurally ready for the same commandlet migration pattern, but they are not authored counter-animation polish. The current migration creates nonlethal counter data by copying `Finisher_A` defaults and setting `ReactionType=Counter`.

## Candidate Classification

| Lane | Assets | Evidence | Recommendation |
| --- | --- | --- | --- |
| Already proven | `LightAttack_1` through `LightAttack_3` | Counter data, counter window, finisher data all present; batch 1 readiness and idempotence are clean | Do not re-migrate unless idempotence fails |
| Commandlet-safe light rollout | `LightAttack_4` through `LightAttack_11` | Post-batch full light rollout plan reports 8 would change, 0 failed, and 0 warning rows | Apply in small montage batches after review |
| Needs authored/design review | `HeavyAttack_1` through `HeavyAttack_4` | Load and section valid, but no `FinisherData`, no `CounterData`, no `CounterWindow` | Do not run counter-proof migration until finisher/counter intent is defined |
| Needs authored/design review | Directional attacks `B`, `F`, `L`, `None`, `R` | Load and section valid, but no `FinisherData`, no `CounterData`, no `CounterWindow` | Classify whether directional attacks participate in Chain Counter before migration |
| Runtime proof blocker | Enemy parryable attack path | No audited AttackData row reports `has_parry_window=true` | Identify or author one attacker-side `ParryWindow` before claiming map-level chain proof |

## Planned Light Rollout Batches

Recommended apply order:

1. `LightAttack_2` and `LightAttack_3` - done
   - Touches only the already-counter-proofed montage family: `AM_Light_Combo_1`.
   - Saved `Counter_LightAttack_2`, `Counter_LightAttack_3`, both AttackData assets, and `AM_Light_Combo_1`.
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
