# KatanaCombat Verification Ladder

## Build

Generate project files:

```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\GenerateProjectFiles.bat" -project="D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -game -rocket
```

Build editor target:

```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -Progress -NoHotReload
```

## Automation Tests

Run all KatanaCombat tests:

```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat" -unattended -nopause -NullRHI -nosplash -stdout
```

Run one category:

```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat.DeathSystem" -unattended -nopause -NullRHI -nosplash -stdout
```

If the command does not exit cleanly, check:

```powershell
powershell -ExecutionPolicy Bypass -File ".agents/skills/katana-verify/scripts/summarize-automation-log.ps1"
```

## Static Checks

For `.json` files:

```powershell
Get-Content path\file.json -Raw | ConvertFrom-Json | Out-Null
```

For `.ps1` scripts:

```powershell
$null = [System.Management.Automation.PSParser]::Tokenize((Get-Content path\file.ps1 -Raw), [ref]$null)
```

For `.toml`, use a parser if available; otherwise run `codex mcp list` or the related Codex command that consumes the config.
