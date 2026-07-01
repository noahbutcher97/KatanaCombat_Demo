# Branch WIP Lane Manifest

Date: 2026-07-01
Branch: `feature/paired-animation-component`

## Branch-Critical Source And Docs

- Reconciled spec/docs from Task 0:
  - `CLAUDE.md`
  - `docs/specs/PAIRED_ANIMATION_SPEC.md`
  - `docs/architecture/ARCHITECTURE_QUICK.md`
  - `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`
  - `docs/plans/gap-tracker.md`
  - `docs/handoffs/2026-06-28-original-branch-intent-satisfaction-audit.md`
  - `docs/handoffs/2026-06-30-branch-wip-lane-manifest.md`
- Runtime Chain Counter implementation candidates:
  - `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`
  - `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`
  - `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
  - `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`
- Editor migration/readiness reporting candidates:
  - `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`
  - `Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h`
  - `Source/KatanaCombatEditor/Private/Commandlets/**`
  - `Source/KatanaCombatEditor/Public/Commandlets/**`
  - `Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp`
  - `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`
- Build/module files tied to the editor migration lane:
  - `Source/KatanaCombatEditor/KatanaCombatEditor.Build.cs`
  - `Source/KatanaCombatTest/KatanaCombatTest.build.cs`

## Asset-Proof Candidates

- Counter-capable player AttackData candidates:
  - `Content/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/**`
  - `Content/ProjectFiles/Data/PDA/Attack/AttackData/Heavy/New/**`
  - `Content/ProjectFiles/Data/PDA/Attack/AttackData/DirectionalAttacks/New/**`
- Attack configuration candidate:
  - `Content/ProjectFiles/Data/PDA/Attack/AttackConfigurations/DA_Configuration_Katana.uasset`
- Paired finisher/counter candidates:
  - `Content/ProjectFiles/Data/PDA/Paired/Finishers/Finisher_A.uasset`
  - `Content/ProjectFiles/Data/PDA/Paired/Finishers/Finishers_A_Attacker.uasset`
  - `Content/ProjectFiles/Data/PDA/Paired/Finishers/Finishers_A_Victim.uasset`
- Map/Blueprint proof candidates:
  - `Content/ProjectFiles/Core/Actors/Character/BP_Player.uasset`
  - `Content/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.uasset`
  - `Content/ProjectFiles/Levels/Lvl_ThirdPerson1.umap`
  - `Content/ProjectFiles/Levels/M_Showcase.umap`

## Unrelated Imports Or Reorg Candidates

- Large third-party/import lanes not required for the first Chain proof:
  - `Content/Assets/Animations/GhostSamurai_Bundle/**`
  - `Content/Assets/SFX/**`
  - `Content/Assets/UI/**`
  - `Content/Assets/VFX/**`
  - `Content/Cyberpunk/**`
  - `Content/__ExternalActors__/Cyberpunk/**`
  - `Content/__ExternalObjects__/Cyberpunk/**`
- Historical/audit documentation lanes:
  - `docs/audits/**`
  - `docs/codereview/**`
  - `docs/handoffs/uemcp-spawn-blueprint-actor.md`
- Local/tooling intent checks:
  - `.mcp.json`
  - `.claude/settings.local.json`
  - `GenerateAndBuild.bat`
  - `Config/Automation/**`

## Merge-Blocked Content Churn

- Tracked deletion buckets are blocked until explicitly confirmed:
  - legacy AttackData deletions under `Content/ProjectFiles/Data/PDA/Attack/AttackData/**`
  - legacy map deletions for `Lvl_Arena`, `Lvl_ThirdPerson`, and matching World Partition external actors/objects
  - legacy character/game-mode/settings/weapon related asset deletions
- Binary package modifications are blocked unless tied to a reviewed proof path:
  - `Content/ProjectFiles/Animation/Montages/Katana/Light/AM_Light_Combo_1.uasset`
  - `Content/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.uasset`
  - `Content/ProjectFiles/Data/PDA/HitReaction/HitReactionSettings/DA_HitReaction.uasset`
  - `Content/ProjectFiles/Data/PDA/Weapons/DA_Weapon_Katana.uasset`
  - `Content/ProjectFiles/Input/IMC_Combat.uasset`
- No `Content/` deletion, move, rename, or package save should be committed from this branch lane without Editor/UEMCP/commandlet proof.

## Required Before Commit Or Save

- `git status --short --branch`
- `git diff --name-status`
- `git ls-files --others --exclude-standard`
- User approval for any package-save or mass asset lane.
