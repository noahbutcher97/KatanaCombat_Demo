# Counter Chain Rollout Inventory

> Created: 2026-07-01
> Branch: `feature/counter-chain-rollout-inventory`
> Scope: inventory and controlled rollout evidence for expanding counter-chain asset coverage after the `LightAttack_1` proof path.

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
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-batch2-save.json`
  - `CounterChainProofMigration`, `ApplyAndSave`, using `Config/AssetMigrations/CounterChainRolloutBatch2Targets.txt`.
  - Result: 3 targets, 3 saved, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-batch2-readiness.json`
  - `ContentReadinessAudit`, using `Config/AssetMigrations/CounterChainRolloutBatch2ReadinessTargets.txt`.
  - Result: 6 targets, 6 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-batch2-postsave-plan.json`
  - `CounterChainProofMigration`, `Plan`, using `Config/AssetMigrations/CounterChainRolloutBatch2Targets.txt`.
  - Result: 3 targets, 3 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-attackdata-audit-after-batch2.json`
  - `AttackDataNotifyMigration`, `Audit`, global scan after batch 2 save.
  - Result: 20 targets, 20 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-branch-critical-readiness-after-batch2.json`
  - `ContentReadinessAudit`, branch-critical target list after batch 2 save.
  - Result: 41 targets, 41 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-light-plan-after-batch2.json`
  - `CounterChainProofMigration`, `Plan`, using `Config/AssetMigrations/CounterChainRolloutLightTargets.txt` after batch 2 save.
  - Result: 10 targets, 5 unchanged, 5 would change, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-batch3-plan.json`
  - `CounterChainProofMigration`, `Plan`, using `Config/AssetMigrations/CounterChainRolloutBatch3Targets.txt`.
  - Result: 5 targets, 5 would change, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-batch3-save.json`
  - `CounterChainProofMigration`, `ApplyAndSave`, using `Config/AssetMigrations/CounterChainRolloutBatch3Targets.txt`.
  - Result: 5 targets, 5 saved, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-batch3-readiness.json`
  - `ContentReadinessAudit`, using `Config/AssetMigrations/CounterChainRolloutBatch3ReadinessTargets.txt`.
  - Result: 10 targets, 10 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-batch3-postsave-plan.json`
  - `CounterChainProofMigration`, `Plan`, using `Config/AssetMigrations/CounterChainRolloutBatch3Targets.txt`.
  - Result: 5 targets, 5 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-attackdata-audit-after-batch3.json`
  - `AttackDataNotifyMigration`, `Audit`, global scan after batch 3 save.
  - Result: 20 targets, 20 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-branch-critical-readiness-after-batch3.json`
  - `ContentReadinessAudit`, branch-critical target list after batch 3 save.
  - Result: 41 targets, 41 unchanged, 0 failed.
- `Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-rollout-light-plan-after-batch3.json`
  - `CounterChainProofMigration`, `Plan`, using `Config/AssetMigrations/CounterChainRolloutLightTargets.txt` after batch 3 save.
  - Result: 10 targets, 10 unchanged, 0 failed.

The initial full light-chain plan run is read-only. Commandlet console warnings about missing counter assets are expected in plan mode when those assets are proposed creations and do not exist yet.

## Canonical State

`LightAttack_1` through `LightAttack_11` are asset-backed counter proof paths. They have:

- `CounterData = Counter_LightAttack_<N>`
- `bHasCounterVariant = true`
- section-scoped `AnimNotifyState_CounterWindow` entries on `AM_Light_Combo_1`, `AM_Light_Combo_2`, and `AM_Light_Combo_3`
- valid `FinisherData`

The light-chain rollout is asset-complete, but it is not authored counter-animation polish. The migration creates nonlethal counter data by copying `Finisher_A` defaults and setting `ReactionType=Counter`.

## Candidate Classification

| Lane | Assets | Evidence | Recommendation |
| --- | --- | --- | --- |
| Asset-backed light rollout | `LightAttack_1` through `LightAttack_11` | Counter data, counter window, and finisher data all present; final full light rollout plan reports 10 unchanged, 0 failed, and 0 warning rows | Treat as current light-chain baseline; future changes should be authored deltas |
| Needs authored/design review | `HeavyAttack_1` through `HeavyAttack_4` | Load and section valid, but no `FinisherData`, no `CounterData`, no `CounterWindow` | Do not run counter-proof migration until finisher/counter intent is defined |
| Needs authored/design review | Directional attacks `B`, `F`, `L`, `None`, `R` | Load and section valid, but no `FinisherData`, no `CounterData`, no `CounterWindow` | Classify whether directional attacks participate in Chain Counter before migration |
| Runtime proof blocker | Enemy parryable attack path | No audited AttackData row reports `has_parry_window=true` | Identify or author one attacker-side `ParryWindow` before claiming map-level chain proof |

## Planned Light Rollout Batches

Completed apply order:

1. `LightAttack_2` and `LightAttack_3` - done
   - Touches only the already-counter-proofed montage family: `AM_Light_Combo_1`.
   - Saved `Counter_LightAttack_2`, `Counter_LightAttack_3`, both AttackData assets, and `AM_Light_Combo_1`.
2. `LightAttack_4` through `LightAttack_6` - done
   - First counter-window edits on `AM_Light_Combo_2`.
   - Saved `Counter_LightAttack_4`, `Counter_LightAttack_5`, `Counter_LightAttack_6`, all three AttackData assets, and `AM_Light_Combo_2`.
3. `LightAttack_7` through `LightAttack_11` - done
   - First counter-window edits on `AM_Light_Combo_3`.
   - Saved `Counter_LightAttack_7`, `Counter_LightAttack_8`, `Counter_LightAttack_9`, `Counter_LightAttack_10`, `Counter_LightAttack_11`, all five AttackData assets, and `AM_Light_Combo_3`.

All planned light rollout batches are complete. Future commandlet save passes should use the same sequence:

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
- Batched application touched three light montages and ten additional AttackData assets while keeping review and rollback scoped by montage.
- Heavy and directional attacks are not safe automatic candidates because the current evidence does not show a full Chain destination via `FinisherData`.
- A map-level proof still requires an enemy attack montage with an attacker-side `ParryWindow`; none is present in the audited 20 AttackData rows.
