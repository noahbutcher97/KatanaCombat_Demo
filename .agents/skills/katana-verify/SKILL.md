---
name: katana-verify
description: Verify KatanaCombat changes with the right Unreal build, automation test, log parsing, static inspection, and diff review. Use when the user asks to verify, validate, run tests, check build health, confirm a fix, prepare a final report, or finish any KatanaCombat code/config/docs change.
---

# Katana Verify

## Verification Ladder

Select the smallest check set that proves the change:

1. Docs or agent-config only: inspect the diff, check Markdown/TOML/JSON/PowerShell syntax as applicable, and confirm no gameplay or asset files changed.
2. C++ module, `.Build.cs`, or `.Target.cs` changes: build `KatanaCombatEditor Win64 Development`.
3. Combat behavior changes: build, then run the narrowest matching `Automation RunTests KatanaCombat.<Category>` path.
4. Broad combat changes: run all `KatanaCombat` automation tests.
5. Asset, montage, Blueprint, level, or data-asset changes: require Unreal Editor, commandlet, UEMCP, or screenshot/log evidence before claiming behavior.

Read `references/verification-ladder.md` for exact commands and log checks.

## Required Preflight

Run `git status --short` before verification. If unrelated user WIP exists, avoid cleanup and report only the files relevant to the task. Do not use `git add .`, `git reset --hard`, `git clean`, broad checkout, or recursive delete commands.

Confirm the project root is `D:\UnrealProjects\5.6\KatanaCombat` before using absolute Unreal commands.

## Automation Test Caveat

The command-line automation test process may not return cleanly. If it appears hung after the expected test window, inspect `Saved/Logs/KatanaCombat.log` and use `scripts/summarize-automation-log.ps1` to count completed, failed, and error lines. Report the process caveat honestly.

## Final Report Format

End with:

- Files changed by this task.
- Verification commands run and their result.
- Any skipped checks and why.
- Residual risk, especially if assets or Editor-only behavior were not exercised.

Never claim tests, build, or runtime behavior passed without fresh evidence from the current turn.
