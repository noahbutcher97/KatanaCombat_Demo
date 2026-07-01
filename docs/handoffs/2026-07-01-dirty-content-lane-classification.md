# Dirty Content Lane Classification - 2026-07-01

## Scope

This pass classifies the current dirty `Content/` tree after the targeted timing and accepted notify-save work. It does not stage, revert, delete, rename, or clean assets.

Evidence lives under `docs/handoffs/evidence/2026-07-01-content-lane/`:

- `git-status-short-branch.txt`
- `git-diff-name-status.txt`
- `git-untracked-files.txt`
- `content-lane-manifest.tsv`
- `content-lane-counts.txt`

## Current Counts

| Lane | Count | Recommendation |
| --- | ---: | --- |
| `accepted-asset-proof` | 4 | Keep with this branch if the AttackData migration proof lane stays. Stage only by exact path. |
| `attackdata-reorg-and-notify-candidates` | 40 | Likely branch-critical, but needs referencer and map-load proof before broad inclusion. |
| `branch-critical-gameplay-content-candidates` | 17 | Review next; this contains paired/finisher, Blueprint, weapon, hit reaction, input, and settings assets. |
| `map-and-gamemode-reorg` | 206 | High risk. Do not include until affected maps and external actors are load-tested. |
| `bulk-import-or-template-content` | 6505 | Treat as a separate content-import lane unless explicitly pulled into this branch. |
| `content-unclassified-needs-owner` | 3 | Needs owner decision before PR: `LS_Takedown`, `BP_CameraShake_Attack`, `GM_Samurai`. |

Content status totals: 6609 untracked, 160 deleted, 6 modified.

## Accepted Asset Proof

These are the only content assets intentionally saved in this pass:

- `Content/ProjectFiles/Animation/Montages/Katana/Heavy/AM_Heavy_Katana_Event.uasset`
- `Content/ProjectFiles/Animation/Montages/Katana/Light/AM_Light_Combo_1.uasset`
- `Content/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_6.uasset`
- `Content/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_9.uasset`

Proof reports:

- `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-timing-save.json`
- `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-accepted-save.json`
- `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit-after-timing-save.json`

## Branch-Critical Readiness Audit

The minimal branch-critical target list is `Config/AssetMigrations/BranchCriticalContentTargets.txt`. It covers the current AttackData replacements, accepted montage edits, paired/finisher assets, combat settings, input, characters, game mode, and active maps. It intentionally excludes deleted replacement paths and the bulk import lane.

Initial read-only audit proof:

- Report: `Saved/Logs/Commandlets/KatanaAssetMigration/branch-critical-content-readiness.json`
- Operation: `ContentReadinessAudit`
- Result: 41 targets, 0 failed, 23 unchanged, 18 `WouldChange`, 0 saved
- Map load proof: `/Game/ProjectFiles/Levels/Lvl_ThirdPerson1` and `/Game/ProjectFiles/Levels/M_Showcase` loaded as `UWorld`
- Paired data proof: `Finisher_A` loaded and passed paired-animation validation; light-chain AttackData rows with `FinisherData` passed referenced paired-data validation

The 18 `WouldChange` rows were branch-readiness debt, not load failures. They were AttackData assets with remaining canonical notify-seeding work:

- Directional: `DA_DirectionalAttack_B`, `DA_DirectionalAttack_F`, `DA_DirectionalAttack_L`, `DA_DirectionalAttack_None`, `DirectionalAttack_R`
- Heavy: `HeavyAttack_2`, `HeavyAttack_3`, `HeavyAttack_4`
- Light: `LightAttack_2` through `LightAttack_11`

Follow-up notify save proof:

- Target list: `Config/AssetMigrations/AttackDataNotifyRemainingTargets.txt`
- Plan report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-remaining-plan.json` - 18 targets, 18 `WouldChange`, 0 failed
- Save report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-remaining-save.json` - 18 targets, 18 saved, 0 failed
- Curated readiness re-audit: `Saved/Logs/Commandlets/KatanaAssetMigration/branch-critical-content-readiness-after-remaining-notifies.json` - 41 targets, 41 unchanged, 0 failed
- Global AttackData notify re-audit: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit-after-remaining-save.json` - 20 targets, 20 unchanged, 0 failed
- Final pre-merge readiness audit: `Saved/Logs/Commandlets/KatanaAssetMigration/branch-critical-content-readiness-final-premerge.json` - 41 targets, 41 unchanged, 0 failed
- Final pre-merge notify audit: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit-final-premerge.json` - 20 targets, 20 unchanged, 0 failed

Counter-chain follow-up proof: `LightAttack_1` now reports `CounterData` set to `Counter_LightAttack_1`, and `AM_Light_Combo_1` has a section-scoped `CounterWindow` carrying that specific counter data. Targeted readiness and post-save idempotence audits are clean. This proves one asset-backed counter path, not broad counter animation coverage or map-level player-facing flow.

## Adversarial Notes

The AttackData reorg lane contains paired tracked deletions and untracked `New/` replacements. That may be intentional, but Git status alone cannot prove references now resolve to the replacements.

The map/game-mode lane includes external actor/object churn. A clean source build does not prove those maps load.

The bulk import lane dominates the dirty tree. Per final merge direction, it is accepted with this branch rather than split out. Future cleanup should owner-review these assets, prune unused imports, and add narrower asset manifests for subsequent content work.

## Future Commit Work

Expand counter-chain coverage beyond the single `LightAttack_1` proof path with authored counter-specific animations and a map-level playtest. Separately review the accepted bulk-import content and prune or reorganize any assets that are not needed by the current playable slice.
