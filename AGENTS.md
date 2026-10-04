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

Before building a fresh checkout, install the locked AnimationAnalysis dependency
using `python Tools/AnimationAnalysis/setup_dependency.py`. Re-run it whenever a pull
changes `Tools/AnimationAnalysis/dependency.json`. Builds stop with
`[AnimationAnalysis pin mismatch]` until the installed plugin matches the pin; follow
that message rather than setting `KATANA_ALLOW_PLUGIN_DRIFT`, which is only for
deliberate local plugin edits. The standard baseline runs setup automatically.
See [dependency setup](Tools/AnimationAnalysis/README.md); the generated plugin is
ignored, and shared implementation edits belong in its separate repository.
The PresentationCapture video recorder follows the same contract, from its private
GitHub repository: `python Tools/PresentationCapture/setup_dependency.py` installs its pin
and builds its workers, and builds stop with `[PresentationCapture pin mismatch]` until it does. See
[its setup](Tools/PresentationCapture/README.md). Changes a player sees or feels need a
rendered capture with video; see `.agents/skills/katana-capture/SKILL.md`.

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

Name files, classes, functions, tests, assets, and tools after their purpose, domain, or observable behavior. Names must be understandable to contributors using the tracked repository alone. Do not introduce names based on private workflow gates (such as `GateA`), local-only milestones, machine setup, or AI-tooling conventions. Migrate existing opaque names in scoped changes that update their consumers and documentation together.

The same rule covers every piece of text that lives in the product or its history:
- code comments;
- tooltips and other editor metadata;
- log and error messages;
- test names;
- Content asset and folder names;
- commit messages;
- branch names and pull request titles, which become merge-commit messages.

None of these may use internal planning, phasing or sequencing labels:
- milestone or gate names (`Gate A`);
- plan steps or slices (`step 3a`, `slice 1`);
- plan phases (`Phase 5c`);
- review rounds and task numbers;
- numbered finding or issue codes (`PT-13`, `INPUT-1`, `BUG-2`, `F1-3`, a reviewer's `P1`);
- decision IDs;
- names for investigation runs ("the spike").

Describe the thing itself, so that a reader with only the tracked repository understands it. Write "the rendered parry, counter and finisher test", not "the Gate A proof"; write "procedural blend times for combo transitions", not "BUG-2 FIX". Gameplay vocabulary is not a planning label: attack phases (Windup, Active, Recovery), combo steps and chain stages are fine. Planning documents under `docs/plans/` and `docs/superpowers/`, and agent instruction files, may use their own labels. Code and content that cite those documents must still describe what they refer to.

The capture foundation and portable analysis package are owned by the separate AnimationAnalysis repository; Katana consumes a pinned revision. Apply the [suite architecture contract](docs/architecture/ANIMATION_ANALYSIS_SUITE.md) to the entire existing suite and new work: portable contracts and analysis, a separate Unreal adapter, then Katana-specific adapters, profiles and scenarios. Project classes, skeleton defaults, asset paths, scenario assertions and local setup belong in the project integration. Audit and migrate existing mixed responsibilities; extracting the foundation does not complete the whole suite. Shared core execution and Unreal adapter builds must remain independent of Katana gameplay dependencies. Keep compatibility, evidence provenance and retention intact during scoped migrations.

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

Use short, imperative summaries that say what changed. Don't add planning prefixes: older history has some, such as `CP-3:` or `Phase 6:`, and they are no longer used. Keep the first line specific: `Fix counter window pose matching` or `Update paired animation docs`. Pull requests should describe gameplay/editor impact, list tests run, link issues or plans, and include screenshots or video for visible animation, UI, or asset changes.

Do not include AI attributions, generated-by footers, or assistant co-author trailers in commit messages.

## Codex Workflow

Repo-specific Codex configuration lives in `.codex/config.toml`. Reusable Codex skills live in `.agents/skills/`:
- `katana-verify`: build, test, log parsing, and final verification.
- `katana-feature`: scoped feature planning and implementation.
- `katana-bug-triage`: evidence-first bug diagnosis.
- `docs/guides/HEADLESS_ASSET_MIGRATIONS.md`: commandlet workflow for audit/plan/apply asset migrations with explicit package-save gates.

Prefer these skills over copying `.claude/commands` or `.gemini/agents` behavior. Those directories are useful references, but they target other agent runtimes.

## Agent-Specific Instructions

The current workspace may contain substantial user WIP, especially under `Content/`. Before edits, inspect `git status --short` and keep changes scoped to the requested files. Never revert, delete, rename, resave, or mass-add Unreal assets unless explicitly asked.

Do not run broad destructive commands such as `git reset --hard`, `git clean`, recursive deletes, or blanket asset moves. Do not use `git add .`; stage intentional files by path. When updating agentic workflow files, keep the change limited to `AGENTS.md`, `.codex/`, `.agents/skills/`, and matching docs unless the user authorizes more.
