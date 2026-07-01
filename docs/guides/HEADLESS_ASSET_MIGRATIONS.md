# Headless Asset Migrations

Use `KatanaAssetMigration` for repeatable editor-only asset migration passes. Start in `Audit` or `Plan` mode. Do not run `ApplyAndSave` unless the exact target list has been reviewed. Relative `-TargetsFile` and `-ReportPath` values resolve from the project directory.

## AttackData Notify Migration

Audit all AttackData assets:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=Audit -AllowGlobalScan -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-audit.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Plan a reviewed target list:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=Plan -TargetsFile="Config/AssetMigrations/AttackDataNotifyTargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-plan.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Apply in memory without saving:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=Apply -TargetsFile="Config/AssetMigrations/AttackDataNotifyTargets.txt" -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-apply.json" -unattended -nopause -NullRHI -nosplash -stdout
```

Apply and save only reviewed targets:

```powershell
&"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -run=KatanaAssetMigration -Operation=AttackDataNotifyMigration -Mode=ApplyAndSave -TargetsFile="Config/AssetMigrations/AttackDataNotifyTargets.txt" -AllowPackageSave -ReportPath="Saved/Logs/Commandlets/KatanaAssetMigration/attackdata-notify-save.json" -unattended -nopause -NullRHI -nosplash -stdout
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

## Counter And Paired Readiness Reporting

Parry, counter, and paired-animation support starts as readiness reporting. Do not automatically seed parry/counter windows or save packages from this operation until an audit report and reviewed target list identify exact assets and missing fields.

Readiness rows should report `CounterData`, `FinisherData`, parry/counter window presence, paired sync/collision notify presence, paired montage section validity, and lethal counter-data warnings.
