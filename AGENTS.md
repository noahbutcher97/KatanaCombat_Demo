# Repository Guidelines

## Purpose And Read Order

This file is the root instruction layer for Codex agents working in KatanaCombat. Start here, then read the narrowest deeper reference needed for the task:

1. `CLAUDE.md` for current combat-system rules, architecture principles, known issues, and test caveats.
2. `docs/architecture/ARCHITECTURE_QUICK.md` before changing runtime combat architecture.
3. `docs/specs/PAIRED_ANIMATION_SPEC.md` before finisher, counter, sync, or paired-animation work.
4. `Source/KatanaCombatTest/README.md` before adding or running automation tests.
5. `.claude/INDEX.md` only as historical workflow context; do not copy Claude-specific commands directly into Codex behavior.

## Project Structure & Module Organization

KatanaCombat is an Unreal Engine 5.6 C++ project. Runtime code lives in `Source/KatanaCombat/`, with public API in `Public/` and implementations in `Private/`. Editor-only tooling lives in `Source/KatanaCombatEditor/`. Automation tests live in `Source/KatanaCombatTest/`, with helpers in `Public/CombatTestHelpers.h` and suites in `Private/*Tests.cpp`.

Assets, maps, animation assets, and data assets live under `Content/`; engine and project settings live in `Config/`. Treat `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`, and IDE caches as generated. `Tools/UE5-Source-Query/` is supporting developer tooling, not gameplay code.

## Build, Test, and Development Commands

Run the standard Codex baseline:
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "Tools\Codex\run-agent-baseline.ps1"
```

This builds `KatanaCombatEditor`, runs all `KatanaCombat` automation tests with `;Quit`, writes timestamped evidence under `Saved/Logs/`, and exits nonzero on failure.

Generate project files:
```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\GenerateProjectFiles.bat" -project="KatanaCombat.uproject" -game -rocket
```

Build the editor target:
```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" KatanaCombatEditor Win64 Development -Project="KatanaCombat.uproject" -Progress -NoHotReload
```

Run all automation tests:
```powershell
"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "KatanaCombat.uproject" -ExecCmds="Automation RunTests KatanaCombat" -unattended -nopause -NullRHI -nosplash -stdout
```

If the command-line test process does not exit cleanly, inspect `Saved/Logs/KatanaCombat.log` for `Test Completed` results.

Summarize the latest automation log with:
```powershell
powershell -ExecutionPolicy Bypass -File ".agents/skills/katana-verify/scripts/summarize-automation-log.ps1"
```

## Coding Style & Naming Conventions

Follow Unreal Engine C++ conventions: `U`, `A`, `F`, `E`, and `I` prefixes where appropriate, PascalCase types/functions, and camelBack variables as enforced by `.clang-tidy`. Keep cross-component combat delegates and shared enums/structs in `Source/KatanaCombat/Public/CombatTypes.h`; keep component-internal declarations near the owning component. Prefer UE containers and smart pointer types over raw ownership.

Core combat rules to preserve:
- Phases are exclusive; windows may overlap.
- Input is always buffered; combo windows affect timing, not capture.
- Parry is defender-side logic checking the attacker's parry window.
- Hold logic checks current button state at the window boundary, not duration.

## Testing Guidelines

Name automation suites with the `KatanaCombat.*` path pattern, for example `KatanaCombat.CombatComponent.InputBuffering`. Add focused tests in `Source/KatanaCombatTest/Private/` and reuse `CombatTestHelpers.h` for world, character, and attack data setup. Cover state transitions, null safety, component interaction, and animation-window behavior when changing combat logic.

Use the smallest verification ladder that proves the change:
- Docs/config only: inspect diff and affected links or syntax.
- C++ build/config changes: build `KatanaCombatEditor Win64 Development`.
- Combat behavior changes: build, then run relevant `Automation RunTests KatanaCombat.<Category>`.
- Asset, Blueprint, montage, or map changes: verify in the Unreal Editor or through UEMCP/automation evidence before claiming behavior.

## Commit & Pull Request Guidelines

Recent history uses short, imperative summaries, sometimes with scope prefixes such as `CP-3:` or `Phase 6:`. Keep the first line specific: `Fix counter window pose matching` or `Update paired animation docs`. Pull requests should describe gameplay/editor impact, list tests run, link issues or plans, and include screenshots or video for visible animation, UI, or asset changes.

## Codex Workflow

Repo-specific Codex configuration lives in `.codex/config.toml`. Reusable Codex skills live in `.agents/skills/`:
- `katana-verify`: build, test, log parsing, and final verification.
- `katana-feature`: scoped feature planning and implementation.
- `katana-bug-triage`: evidence-first bug diagnosis.

Prefer these skills over copying `.claude/commands` or `.gemini/agents` behavior. Those directories are useful references, but they target other agent runtimes.

## Agent-Specific Instructions

The current workspace may contain substantial user WIP, especially under `Content/`. Before edits, inspect `git status --short` and keep changes scoped to the requested files. Never revert, delete, rename, resave, or mass-add Unreal assets unless explicitly asked.

Do not run broad destructive commands such as `git reset --hard`, `git clean`, recursive deletes, or blanket asset moves. Do not use `git add .`; stage intentional files by path. When updating agentic workflow files, keep the change limited to `AGENTS.md`, `.codex/`, `.agents/skills/`, and matching docs unless the user authorizes more.
