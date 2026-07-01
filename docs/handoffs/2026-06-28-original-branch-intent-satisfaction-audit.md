# Original Branch Intent Satisfaction Audit - 2026-06-28

## Scope

This audit checks whether `feature/paired-animation-component` satisfies its original branch intent. It uses Git history, source inspection, design docs, and the existing WIP evidence bundle. It does not stage, revert, delete, rename, resave, or validate Unreal assets.

Important boundary: the current checkout contains committed branch work plus substantial uncommitted WIP. A successful source check in this checkout does not prove that either the committed branch or a future PR slice is independently clean.

## Inferred Original Intent

The branch name and commits from `origin/main..HEAD` show these intended outcomes:

1. Scaffold `UPairedAnimationComponent` beside `UCombatComponent`.
2. Cut over paired-animation, finisher, partner, counter, and parry responsibilities from `UCombatComponent` to `UPairedAnimationComponent`, with backward-compatible wrappers.
3. Rewire `AnimNotifyState_CounterWindow` so attack-specific counter data is owned by the paired-animation path.
4. Support paired finishers, specific counter paired animations, sync/collision notifies, input blocking, lifecycle cleanup, and cinematic effects.
5. Keep design docs and automation coverage aligned with the five-component combat architecture.

The branch later accumulated adjacent work: hit detection overhaul/fixes and agent-workflow baseline commits. Those are real branch content, but they are not core to the original paired-animation extraction intent.

## Current Verdict

The branch is directionally aligned with the original paired-animation intent, but it is not yet proven merge-ready as a whole.

Source-level extraction and cutover are mostly satisfied. Real montage, asset, Blueprint default, map, and content-reorg behavior remains unproven. Chain Counter is in scope for this branch; its public source flow, paired completion handoff, readiness reporting, and nonlethal counter policy are now fixture-proven, but asset-backed proof remains required. The current working tree includes extensive uncommitted asset churn that can invalidate branch-level confidence.

## Satisfaction Matrix

| Intent Area | Status | Evidence | Gap |
| --- | --- | --- | --- |
| Component extraction | Mostly satisfied | `ABaseCombatCharacter` creates `PairedAnimationComponent`; `UCombatComponent` caches it and forwards paired/counter APIs. | Needs full automation rerun on current checkout and later on clean PR slice. |
| Counter-window ownership | Mostly satisfied | `AnimNotifyState_CounterWindow` calls `SetCounterWindowData` / `ClearCounterWindowData` on `UPairedAnimationComponent`; `FCounterContext` carries `SpecificCounterData`. | Need runtime test proving authored montage notify data reaches defender-side counter execution. |
| Finisher flow | Partially satisfied | `TryExecuteFinisher` validates finisher data, target, range, path, vulnerability, then calls paired-start flow. | Needs real asset proof with attacker/victim montages, sections, warp targets, sync notifies, and completion. |
| Specific counter paired animations | Partially satisfied | AC3 mode attempts `TryStartPairedAnimationWithTarget` when `SpecificCounterData` is present. | Need asset-backed test that a concrete counter montage pair plays and completes; fallback damage path must remain covered. |
| Chain counter mode | Required branch behavior | Public `TryCounter()` must start from attacker-side parry windows, attack input must advance the chain, authored paired counter data must be used when present, and all Chain exits must clear context. | Asset-backed proof remains required for concrete montages and maps. |
| Partner tracking and lifecycle | Mostly satisfied | Paired partners are added, removed, cleared, and notified on death/cancel/complete; tests cover partner tracking and null safety. | Need runtime proof when one partner dies or montage interrupts during active paired animation. |
| Sync and collision notifies | Partially satisfied | Paired sync/collision notifies read partner data from `UPairedAnimationComponent`; tests check sync/effect properties. | Need real montage notify inspection and PIE/editor proof that collisions and sync effects fire on the intended frames. |
| Editor-time paired analysis | Inherited, not branch-proven | Paired preview and analysis subsystem files exist in `Source/KatanaCombatEditor`; spec marks Phase 5d complete. | These files are not the central committed branch diff. Treat as pre-existing capability unless refreshed by targeted editor proof. |
| AttackData notify migration | In scope | Migration tooling audits/plans canonical phase, hold-start, parry/counter, and paired-readiness evidence for AttackData-driven montages. | Package-save passes require reviewed target lists and explicit approval. |
| Documentation alignment | Partially satisfied | `PAIRED_ANIMATION_SPEC.md`, architecture docs, and gap tracker describe the five-component architecture. | Some claims are stronger than current proof, especially test totals, Phase 5d completeness, and asset-level behavior. |

## Adversarial Findings

- The branch can appear healthier than it is because many tests are structural, property, or null-safety checks. They do not prove authored montage assets, section names, notify placement, or map-level behavior.
- `CounterMode == Chain` now has public Block and attack-input automation coverage, retained target/context coverage, paired completion handoff coverage, and nonlethal counter policy coverage. This still does not prove real authored montages, sections, sync points, or maps.
- `SpecificCounterData` and selected `AttackData::CounterData` are wired through source, but without real montage proof this can still fail through missing sections, incompatible skeletons, null animation assets, or bad motion-warp offsets.
- The current dirty `Content/` state is too large to treat as incidental. Tracked asset deletions plus untracked replacements can mask missing references, broken maps, and invalid data-asset defaults.
- Existing proof for AttackData editor tooling and the 2026-07-01 read-only migration reports were collected in the dirty checkout. They prove the checkout at that moment, not an isolated implementation slice.
- The committed branch contains already-adjacent work beyond original paired animation. Before PR, decide whether hit detection and agent workflow commits stay in this branch or are rebased/split.

## Branch Readiness Gates

Before claiming the branch is complete, collect fresh evidence for these gates:

1. Build `KatanaCombatEditor Win64 Development` with `-NoHotReloadFromIDE` if another UE 5.6 editor holds Live Coding.
2. Run focused automation: `KatanaCombat.PairedAnimation` and `KatanaCombat.CounterSystem`.
3. Run the full `KatanaCombat` automation suite, or document why a narrower suite is sufficient.
4. Audit real paired/counter/finisher assets for montage references, section names, skeleton compatibility, required notifies, sync points, and warp configuration.
5. Open or commandlet-load affected maps and primary Blueprints after content decisions are made.
6. Resolve or explicitly classify every uncommitted `Content/` deletion/import before any PR.
7. Update docs where claims exceed proof, especially Chain Counter public-flow behavior, AttackData readiness reporting, and asset-backed verification.

## Immediate Recommendation

Keep working on this branch only if we treat it as a hardening branch, not as ready-to-merge. Chain Counter is in scope for this branch. The source-level public flow, paired completion handoff, readiness reporting, and nonlethal policy now have fixture-level proof; the branch is not ready until asset proof and dirty Content classification are complete.

The next concrete action is final source verification, followed by asset/content truth:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.PairedAnimation;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

Then run:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.CounterSystem;Quit" -unattended -nopause -NullRHI -nosplash -stdout
```

If both pass, the next risk is not source architecture; it is asset/content truth. Switch to a content verification pass for paired finisher/counter assets, map loadability, remaining notify seeding candidates, and the broader dirty `Content/` lane before any broad package-save migration.

## Evidence Used

- `git log --oneline origin/main..HEAD`
- `git status --short --branch`
- `git diff --name-status origin/main...HEAD -- Source/KatanaCombat Source/KatanaCombatTest docs/specs docs/architecture docs/plans`
- `Source/KatanaCombat/Public/Core/PairedAnimationComponent.h`
- `Source/KatanaCombat/Private/Core/PairedAnimationComponent.cpp`
- `Source/KatanaCombat/Private/Core/CombatComponent.cpp`
- `Source/KatanaCombat/Private/Animation/AnimNotifyState_CounterWindow.cpp`
- `Source/KatanaCombat/Private/Animation/AnimNotifyState_PairedAnimationSync.cpp`
- `Source/KatanaCombat/Private/Animation/AnimNotifyState_PairedAnimationCollision.cpp`
- `Source/KatanaCombatTest/Private/PairedAnimationTests.cpp`
- `Source/KatanaCombatTest/Private/CounterSystemTests.cpp`
- `docs/specs/PAIRED_ANIMATION_SPEC.md`
- `docs/architecture/ARCHITECTURE_QUICK.md`
- `Source/KatanaCombatEditor/Public/AttackDataNotifyGenerationService.h`
- `Source/KatanaCombatEditor/Private/AttackDataNotifyGenerationService.cpp`
- `Source/KatanaCombatEditor/Public/Commandlets/KatanaAssetMigrationTypes.h`
- `Source/KatanaCombatEditor/Private/Commandlets/KatanaAssetMigrationRunner.cpp`
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataNotifyMigrationOperation.cpp`
- `Source/KatanaCombatEditor/Private/Commandlets/Operations/AttackDataTimingMigrationOperation.cpp`
- `Source/KatanaCombatTest/Private/AttackDataEditorToolsTests.cpp`
- `Source/KatanaCombatTest/Private/KatanaAssetMigrationTests.cpp`
- `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit.json`
- `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-plan.json`
- `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-timing-plan.json`
- `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-timing-save.json`
- `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-accepted-plan.json`
- `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-accepted-save.json`
- `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit-after-timing-save.json`
- `docs/handoffs/2026-06-27-wip-classification.md`
- `docs/handoffs/2026-06-27-wip-evidence-annex.md`

## 2026-07-01 Read-Only Asset Proof Update

- `AttackDataNotifyMigration` audit report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit.json`
- Plan report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-plan.json`
- Audit result: 20 rows, 18 would-change rows, 2 failed rows.
- Plan result: 2 reviewed targets, 2 would-change rows, 0 failed rows.
- Timing diagnostics now report section length and phase totals. `LightAttack_9` has a 0.867s section with 1.000s timing total; `LightAttack_6` has a 0.861s section with 1.000s timing total.
- A narrowed save pass was run for `Config/AssetMigrations/AttackDataNotifySaveTargets.txt`.
- Save report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-save.json`
- Saved package: `/Game/ProjectFiles/Animation/Montages/Katana/Heavy/AM_Heavy_Katana_Event`
- Post-save plan report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-plan-after-save.json`
- Post-save result for reviewed targets: `HeavyAttack_1` is unchanged; `LightAttack_1` still would-change because `AM_Light_Combo_1.uasset` was already modified before the save pass and was intentionally not resaved here.
- At this read-only checkpoint, remaining blockers were the two short-section timing failures, the pre-existing `AM_Light_Combo_1.uasset` edit decision, and dirty `Content/` classification. The timing and accepted light-save items are superseded by the update below.

## 2026-07-01 Timing Fix And Accepted Save Update

- Added `AttackDataTimingMigration`, a reviewed-target operation that preserves Windup and Active, clamps Recovery to the remaining section budget, and reports proposed timing fields before save.
- Timing plan report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-timing-plan.json`
- Timing save report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-timing-save.json`
- Saved packages: `/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_6` and `/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_9`
- Fixed timing values: `LightAttack_6` recovery `0.500s -> 0.360883s`; `LightAttack_9` recovery `0.500s -> 0.366738s`.
- Accepted notify save report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-accepted-save.json`
- Saved package: `/Game/ProjectFiles/Animation/Montages/Katana/Light/AM_Light_Combo_1`
- Follow-up audit report: `Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit-after-timing-save.json`
- Follow-up audit result: 20 targets, 18 would-change, 2 unchanged, 0 failed. `LightAttack_1` and `HeavyAttack_1` are unchanged; `LightAttack_6` and `LightAttack_9` are valid timing-wise but still need reviewed notify seeding before broad save claims.
- Remaining blockers: classify the broader dirty `Content/` lane, then decide which remaining 18 notify-seeding candidates are branch-critical assets versus later content work.
