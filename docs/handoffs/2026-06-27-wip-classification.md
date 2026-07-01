# KatanaCombat WIP Classification - 2026-06-27

## Scope

This is a read-only classification of the current working tree on `feature/paired-animation-component...origin/feature/paired-animation-component`.
It is based on `git status --short --branch`, `git diff --name-status`, and `git ls-files --others --exclude-standard`.
No files were staged, reverted, deleted, renamed, resaved, or validated in the Unreal Editor during this pass.

## Snapshot

- Tracked changes: 187 total.
- Tracked modified files: 27.
- Tracked deleted files: 160.
- Untracked files before this report was added: 6,457.
- Highest-risk area: tracked `Content/` deletions plus replacement-looking untracked content.
- Line-ending warning: Git reports several text files will be rewritten from LF to CRLF when touched.

## Classification Rules

| Class | Meaning | Default Disposition |
| --- | --- | --- |
| Current feature candidate | Looks related to AttackData notify seeding, migration commandlets, editor tooling, or matching tests/docs. | Keep together only after source diff review and build/test proof. |
| Runtime combat WIP | Looks related to paired animation, AttackData runtime API, montage utilities, target modules, or architecture docs. | Keep separate unless proven required by the current feature. |
| Asset/content WIP | Any `.uasset`, `.umap`, World Partition external actor/object file, imported pack, FX/SFX, or data asset. | Do not stage or clean without explicit confirmation and asset verification. |
| Local/tooling WIP | Local agent, MCP, automation, or helper command files. | Review individually; do not assume project-wide commitability. |
| Historical/audit docs | Bulk audits, code review notes, or handoffs. | Keep separate from implementation PRs unless requested. |

## Current Feature Candidate

Likely intended for the AttackData notify seeding and headless asset migration lane:

- `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`
- `Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h`
- `Source/KatanaCombatEditor/Private/Commandlets/**`
- `Source/KatanaCombatEditor/Public/Commandlets/**`
- `Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp`
- `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`
- `Source/KatanaCombatEditor/KatanaCombatEditor.Build.cs`
- `Source/KatanaCombatEditor/Private/AttackDataTools.cpp`
- `Source/KatanaCombatEditor/Private/Customizations/AttackDataCustomization.cpp`
- `Source/KatanaCombatEditor/Public/AttackDataTools.h`
- `Source/KatanaCombatTest/KatanaCombatTest.build.cs`
- `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`
- `docs/superpowers/plans/2026-06-22-attackdata-notify-canon-cleanup.md`
- `docs/superpowers/plans/2026-06-22-headless-asset-migration-commandlet.md`
- `docs/superpowers/specs/2026-06-22-headless-asset-migration-design.md`

Recommended handling: keep this as the first reviewable lane. Before staging, inspect diffs, build `KatanaCombatEditor Win64 Development`, and run the focused editor automation suites.

## Runtime Combat WIP

These files look like broader runtime paired-animation or combat API work rather than pure editor-tooling:

- `KatanaCombat.uproject`
- `Source/KatanaCombat.Target.cs`
- `Source/KatanaCombatEditor.Target.cs`
- `Source/KatanaCombat/Private/Data/AttackData.cpp`
- `Source/KatanaCombat/Public/Data/AttackData.h`
- `Source/KatanaCombat/Public/Data/AttackConfiguration.h`
- `Source/KatanaCombat/Private/Utilities/MontageUtilityLibrary.cpp`
- `Source/KatanaCombat/Public/Utilities/MontageUtilityLibrary.h`
- `docs/architecture/ARCHITECTURE.md`
- `docs/architecture/ARCHITECTURE_QUICK.md`
- `docs/plans/gap-tracker.md`

Recommended handling: separate from the current feature unless a direct dependency is proven. Review against `docs/specs/PAIRED_ANIMATION_SPEC.md` before implementation or staging.

## Tracked Asset Modifications

These are binary asset edits and cannot be validated from Git text diff alone:

- `Content/ProjectFiles/Animation/Montages/Katana/Light/AM_Light_Combo_1.uasset`
- `Content/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.uasset`
- `Content/ProjectFiles/Data/PDA/HitReaction/HitReactionSettings/DA_HitReaction.uasset`
- `Content/ProjectFiles/Data/PDA/Weapons/DA_Weapon_Katana.uasset`

Recommended handling: require Unreal Editor, commandlet, or UEMCP evidence before staging. These should not ride with source-only editor-tooling work unless they are explicitly part of the same feature.

## Tracked Asset Deletions

Direct tracked `Content/` deletions include character meshes, sequencer content, character/game-mode assets, attack configuration/data assets, motion-warping/settings assets, and levels:

- Deleted legacy character meshes: 2.
- Deleted direct assets outside external actor/object folders, excluding legacy AttackData: 12.
- Deleted legacy AttackData assets: 20 total, split as 11 light, 4 heavy, and 5 directional.
- Deleted levels/maps: `Content/ProjectFiles/Levels/Lvl_Arena.umap`, `Content/ProjectFiles/Levels/Lvl_ThirdPerson.umap`, and `Content/Templates/ThirdPerson/Lvl_ThirdPerson.umap`.
- Deleted World Partition external actors: 54 under `Lvl_Arena`, 70 under `Lvl_ThirdPerson`.
- Deleted World Partition external objects: 2 under `Lvl_Arena`, 2 under `Lvl_ThirdPerson`.

Recommended handling: treat all tracked `Content/` deletions as blocked until explicitly confirmed. Do not stage any deletion bucket as cleanup. These look like a content reorg or replacement set and need editor-level validation.

## Untracked Content Imports And Replacements

The untracked bulk is asset content, not source work:

- `Content/Assets/SFX/**`: 1,968 files.
- `Content/Assets/VFX/**`: 545 files.
- `Content/Assets/Animations/GhostSamurai_Bundle/**`: 994 files.
- `Content/Cyberpunk/**`: 812 files.
- `Content/__ExternalActors__/Cyberpunk/**`: 1,843 files.
- `Content/__ExternalObjects__/Cyberpunk/**`: 36 files.
- `Content/__ExternalActors__/ProjectFiles/Levels/Lvl_ThirdPerson1/**`: 70 files.
- `Content/__ExternalObjects__/ProjectFiles/Levels/Lvl_ThirdPerson1/**`: 2 files.
- New AttackData replacements: 20 files under `DirectionalAttacks/New`, `Heavy/New`, and `Light/New`.
- Other new `Content/ProjectFiles/**` assets: 15 files, including `BP_Player`, camera shake, `GM_KatanaCombat_Base`, `DA_Configuration_Katana`, FX data assets, paired finisher assets, targeting config, `Lvl_ThirdPerson1.umap`, and `M_Showcase.umap`.

Recommended handling: split this into a dedicated content import/reorg lane. Before any commit, verify LFS coverage if applicable, open maps/assets in the editor, and confirm no tracked deletions are accidental replacements.

## Local Tooling And Config WIP

Files that need individual intent checks:

- `.claude/settings.local.json`
- `.mcp.json`
- `Config/Automation/Presets/Katana.json`
- `GenerateAndBuild.bat`
- `AGENTS.md`
- `CLAUDE.md`

Recommended handling: do not mix local settings with source changes. Project workflow docs/config can be committed only if they are intentionally part of the agentic workflow lane.

## Historical Or Audit Docs

Untracked documentation includes:

- `docs/audits/HIT_STATE_SYSTEM_AUDIT_2026-02-05.md`
- `docs/codereview/**`: 132 files.
- `docs/handoffs/uemcp-spawn-blueprint-actor.md`
- Modified guides/API docs: `docs/architecture/API_REFERENCE.md`, `docs/guides/ATTACK_CREATION.md`, `docs/guides/GETTING_STARTED.md`, `docs/guides/TROUBLESHOOTING.md`

Recommended handling: keep audit/code-review dumps out of implementation PRs unless the PR is explicitly a documentation archive. The guide/API docs may belong with AttackData tooling, but need diff review.

## Immediate Recommendation

1. Freeze all `Content/` deletion and import decisions until we have explicit asset intent.
2. First prepare a source/docs-only review lane for AttackData notify generation and headless migration tooling.
3. Keep runtime paired-animation/combat WIP in a separate lane unless dependency review says otherwise.
4. After source/docs lane is clean, run build and focused automation before staging.
5. Only then decide whether the asset reorg/import lane should be restored, split, or committed with editor proof.

## Residual Risk

This report classifies paths and risk from Git metadata only. It does not prove asset referencer health, map loadability, Blueprint defaults, montage notify contents, package save status, or runtime behavior.
