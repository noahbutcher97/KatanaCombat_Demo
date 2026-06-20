# KatanaCombat Unreal Baseline - 2026-06-20

## Context

- Timestamp: 2026-06-20 15:33:05 -04:00
- Git HEAD: `09d3d8b`
- Worktree state: 203 dirty entries before baseline; treat non-agentic changes as existing user WIP.
- Scope: build and command-line automation baseline for future Codex agent work.

## Build Baseline

Command:

```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -Progress -NoHotReload
```

Result: GREEN. UnrealBuildTool reported `Target is up to date`, `Result: Succeeded`, total execution time 0.71 seconds.

Evidence:

- `Saved/Logs/Codex-Baseline-Build-20260620.log`
- `Saved/Logs/Codex-Baseline-Build-20260620.err.log`

## Automation Baseline

Command:

```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UnrealProjects\5.6\KatanaCombat\KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat;Quit" -NullRHI -NoSplash -Unattended -nopause -stdout
```

Result: GREEN.

- Process exit code: 0
- Automation completion lines: 368
- Explicit non-success test results: 0
- Automation failure/error count from summarizer: 0
- Raw `: Error:` log lines: 0
- Automation warning count from summarizer: 433

Evidence:

- `Saved/Logs/KatanaCombat.log`
- `Saved/Logs/Codex-Baseline-Automation-20260620.log`
- `Saved/Logs/Codex-Baseline-Automation-20260620.err.log`
- `Saved/Logs/Codex-Baseline-Automation-20260620.exitcode.txt`

## Caveats

- The run used `-NullRHI`, so it proves command-line automation health, not rendered asset, Blueprint, montage, or editor-visual behavior.
- The automation log includes warnings emitted by expected negative-path tests, especially weapon socket and null-world diagnostics. These did not produce non-success test results.
- `Source/KatanaCombatTest/README.md` still advertises 126 tests; the current automation log contains 368 completed test result lines. Treat this baseline log as the current evidence.
- The helper script `summarize-automation-log.ps1` was corrected during this baseline to avoid false positives from test names containing `Failure` or `Fails`.
