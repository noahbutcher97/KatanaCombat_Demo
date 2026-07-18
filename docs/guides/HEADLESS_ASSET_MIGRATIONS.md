# Headless Asset Migrations

Use `KatanaAssetMigration` for repeatable editor-only asset migration passes. Start in `Audit` or `Plan` mode. Do not run `ApplyAndSave` until the exact target list or fixed authoring recipe has been reviewed. Relative `-TargetsFile` and `-ReportPath` values resolve from the project directory.

## Defense Proof Migration

`DefenseProofMigration` validates and repairs one reviewed defense manifest without scanning unrelated content. Defense manifests use `schemaVersion: 2` and must declare a nonempty, duplicate-free `proofCases` array; Audit emits one row for every declared case so runtime proof cannot silently omit a requirement. The target file must contain exactly one non-comment line, such as:

```text
Tools/Codex/manifests/defense-gate-a.json
```

Run read-only Audit and Plan passes first:

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseProofMigration -Mode=Audit -TargetsFile="Config/AssetMigrations/DefenseGateATargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-audit.json" -unattended -nopause -NullRHI -nosplash -stdout
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseProofMigration -Mode=Plan -TargetsFile="Config/AssetMigrations/DefenseGateATargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-plan.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Review every proposed change and package-ledger entry. Apply modes require the unchanged schema-v2 Plan report and its exact `plan_fingerprint`:

```powershell
$planPath = "Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-plan.json"
$fingerprint = (Get-Content $planPath -Raw | ConvertFrom-Json).plan_fingerprint
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseProofMigration -Mode=Apply -TargetsFile="Config/AssetMigrations/DefenseGateATargets.txt" "-ApprovedPlanReport=$planPath" "-ApprovedPlanFingerprint=$fingerprint" -AllowTimingMutation -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-apply.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Close the Editor and inspect `git status --short` before saving. Change `-Mode=Apply` to `-Mode=ApplyAndSave`, add `-AllowPackageSave`, and use a distinct save report. Keep `-AllowTimingMutation` only when the reviewed Plan includes parry-window edits. Do not use `-AllowDirtyPackages` unless the initial dirty state was separately reviewed. The operation rejects manifest or asset drift, changed-package mismatches, unregistered tags, unsupported block keys, unresolved assets, non-idempotent apply, and post-save Audit failures.

### Gate A Proof Asset Authoring

`DefenseProofAuthoring` owns the versioned, deterministic Gate A recipe. It has a fixed destination set and does not accept a target list or discover packages globally. Use it to create missing proof assets, rewrite recipe-owned montages, and validate existing paired/configuration/Guard assets. A mismatch in those validated assets fails closed instead of repairing them. Run `DefenseProofMigration` afterward to validate the manifest and dependency closure.

```powershell
$editor = "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$plan = "Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-authoring-plan.json"
& $editor "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseProofAuthoring -Mode=Plan "-ReportPath=$plan" -unattended -nopause -NullRHI -nosplash -stdout
$fingerprint = (Get-Content $plan -Raw | ConvertFrom-Json).plan_fingerprint
& $editor "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseProofAuthoring -Mode=Apply "-ApprovedPlanReport=$plan" "-ApprovedPlanFingerprint=$fingerprint" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-a-authoring-apply.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Review the Plan row and ledger before Apply. Version 4 binds the exact recipe constants, direct source packages and animation skeletons, current destination packages, canonical Gate A manifest, planned additions, and package ledger. A loaded dirty source or destination package invalidates approval even when the destination does not yet have an on-disk file. Close the Editor before persistence, rerun with `-Mode=ApplyAndSave -AllowPackageSave`, then run a fresh `-Mode=Audit`. Success requires exactly the approved packages to save/reload and the post-save Audit to report `Unchanged`. Any bound recipe, dependency, destination, manifest, or plan drift invalidates the old fingerprint.

### Gate B Defense Matrix Authoring

`DefenseMatrixAuthoring` owns the fixed, deterministic Gate B recipe. It rejects `-TargetsFile` and `-AllowGlobalScan`. Recipe V11 creates three logical height families, each with distinct Left, Center, and Right AttackData variants; lane variants must never share one AttackData asset. V11 also binds `spine_01` to `Middle`.

```powershell
$editor = "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$plan = "Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-b-authoring-plan.json"
& $editor "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseMatrixAuthoring -Mode=Audit -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-b-authoring-audit.json" -unattended -nopause -NullRHI -nosplash -stdout
& $editor "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseMatrixAuthoring -Mode=Plan "-ReportPath=$plan" -unattended -nopause -NullRHI -nosplash -stdout
$fingerprint = (Get-Content $plan -Raw | ConvertFrom-Json).plan_fingerprint
& $editor "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=DefenseMatrixAuthoring -Mode=Apply "-ApprovedPlanReport=$plan" "-ApprovedPlanFingerprint=$fingerprint" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/defense-gate-b-authoring-apply.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Close the Editor, inspect the Plan ledger and `git status --short`, then rerun the approved Apply command with `-Mode=ApplyAndSave -AllowPackageSave` and a distinct report path. Run a fresh `DefenseMatrixAuthoring -Mode=Audit`, followed by `DefenseProofMigration -Mode=Audit -TargetsFile="Config/AssetMigrations/DefenseGateBTargets.txt"`. Both audits must report `Unchanged` before runtime proof. The accepted V11 run used fingerprint `5AD2313E317B118CE2148AD8257C9C59E0A8237B`; this is historical evidence, not a reusable approval token. Always read the fingerprint from a fresh reviewed Plan because recipe or asset drift invalidates prior approval.

## AttackData Notify Migration

Audit an explicit reviewed target list. Global scan is intentionally unsupported:

```powershell
$editor = "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$targets = "Config/AssetMigrations/AttackDataNotifyTargets.txt"
$plan = "Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-plan.json"
& $editor "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=Audit "-TargetsFile=$targets" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Plan a reviewed target list:

```powershell
& $editor "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=Plan "-TargetsFile=$targets" "-ReportPath=$plan" -unattended -nopause -NullRHI -nosplash -stdout
$fingerprint = (Get-Content $plan -Raw | ConvertFrom-Json).plan_fingerprint
```

Apply in memory without saving:

```powershell
& $editor "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=Apply "-TargetsFile=$targets" "-ApprovedPlanReport=$plan" "-ApprovedPlanFingerprint=$fingerprint" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-apply.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Apply and save only reviewed targets:

```powershell
& $editor "KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=ApplyAndSave "-TargetsFile=$targets" "-ApprovedPlanReport=$plan" "-ApprovedPlanFingerprint=$fingerprint" -AllowPackageSave -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-save.json" -unattended -nopause -NullRHI -nosplash -stdout
```

For the paired-animation branch, the remaining reviewed notify candidates live in `Config/AssetMigrations/AttackDataNotifyRemainingTargets.txt`. Use a distinct report path such as `attackdata-notify-remaining-save.json` so the original accepted-save proof and remaining-candidate save proof stay separate.

Use `-AllowDirtyPackages` only after confirming the affected package was already dirty before the commandlet and the current owner accepts saving that pre-existing state.

Do not pass `-AllowTimingMutation` to `AttackDataNotifyMigration`. This operation requires valid AttackData timing and refuses timing rewrites.

The commandlet writes JSON rows with target, montage, section, status, section timing, manual phase timing, proposed timing fields, legacy notifies found, stale canonical notifies found, planned removals, planned additions, changed packages, saved packages, warnings, and errors.

## Branch-Critical Content Readiness Audit

Use `ContentReadinessAudit` to prove that a reviewed content slice loads without saving packages. This is read-only and requires an explicit target list; global scan is intentionally unsupported.

Audit the paired-animation branch-critical content slice:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=ContentReadinessAudit -Mode=Audit -TargetsFile="Config/AssetMigrations/BranchCriticalContentTargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/branch-critical-content-readiness.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Rows include generic package load fields plus deeper checks for `UAttackData`, `UAttackConfiguration`, and `UPairedAnimationData`. Treat `Failed` rows as branch blockers. Treat `WouldChange` rows as review debt, usually missing canonical AttackData notifies or branch-readiness warnings.

## AttackData Timing Migration

Use `AttackDataTimingMigration` only for reviewed targets whose manual timing exceeds the montage section. This operation preserves Windup and Active, then clamps Recovery to the remaining section budget. It fails instead of guessing when Windup plus Active already exceeds the section length.

Plan reviewed targets:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataTimingMigration -Mode=Plan -TargetsFile="Config/AssetMigrations/AttackDataTimingTargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-timing-plan.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Apply and save only reviewed targets:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataTimingMigration -Mode=ApplyAndSave -TargetsFile="Config/AssetMigrations/AttackDataTimingTargets.txt" -AllowPackageSave -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-timing-save.json" -unattended -nopause -NullRHI -nosplash -stdout
```

## Counter Chain Proof Migration

Use `CounterChainProofMigration` only for reviewed counter-chain proof targets. Each target line has three fields:

```text
AttackData|CounterDataPackage|TemplatePairedData
```

Example target list: `Config/AssetMigrations/CounterChainProofTargets.txt`.

Plan the reviewed target:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=CounterChainProofMigration -Mode=Plan -TargetsFile="Config/AssetMigrations/CounterChainProofTargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-proof-plan.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Apply and save only after the plan report names the exact packages:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=CounterChainProofMigration -Mode=ApplyAndSave -TargetsFile="Config/AssetMigrations/CounterChainProofTargets.txt" -AllowPackageSave -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/counter-chain-proof-save.json" -unattended -nopause -NullRHI -nosplash -stdout
```

This operation creates or updates a nonlethal `UPairedAnimationData` counter asset from a valid paired-data template, sets `AttackData::bHasCounterVariant` and `CounterData`, and seeds a section-scoped `AnimNotifyState_CounterWindow` with the specific counter data when needed. It rejects global scans and stays behind the same explicit package-save gate as other mutating asset migrations.

Re-run `ContentReadinessAudit` on the AttackData and counter data target list, then re-run `CounterChainProofMigration` in `Plan` mode. Expected post-save result is `Unchanged`.

Readiness rows report `CounterData`, `FinisherData`, parry/counter window presence, paired montage section validity, and lethal counter-data warnings.

## Enemy AI Proof Assets

Use `EnemyAIProofAssets` to create or refresh the minimal playable enemy AI proof slice. This operation does not use a target file. It creates or updates `/Game/ProjectFiles/AI/ST_EnemyCombatProof` and `/Game/ProjectFiles/AI/BP_EnemyCombatAIController`, assigns that controller and a fallback attack on `/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter`, and loads `/Game/ProjectFiles/Levels/Lvl_ThirdPerson1` to validate placed enemies. It also creates or repairs Boolean `/Game/ProjectFiles/Input/Actions/IA_Block`, assigns it and the combat mapping context on `/Game/ProjectFiles/Core/Actors/Character/BP_Player`, maps block to Thumb Mouse Button and Gamepad Left Shoulder in `/Game/ProjectFiles/Input/IMC_Combat`, and removes the deprecated Right Mouse Button block mapping so that binding remains available for heavy attack.

Plan the proof asset update:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=EnemyAIProofAssets -Mode=Plan -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/enemy-ai-proof-assets-plan.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Apply and save only after reviewing the planned package list:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=EnemyAIProofAssets -Mode=ApplyAndSave -AllowPackageSave -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/enemy-ai-proof-assets-save.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Re-run `Plan` after saving. Expected post-save result is `Unchanged`, with a warning that records how many `AEnemyCharacter` actors loaded from `Lvl_ThirdPerson1` and how many already had usable attacks. Review the report for every package before allowing saves: depending on detected drift, the operation may save the StateTree, controller Blueprint, enemy Blueprint, player Blueprint, `IA_Block`, and `IMC_Combat`. It should not resave the level or external actor packages unless a placed enemy is missing required defaults.
