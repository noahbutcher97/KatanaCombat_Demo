---
name: katana-feature
description: Plan and implement KatanaCombat feature work safely. Use when the user asks to add, implement, extend, refactor, or wire gameplay, combat, paired-animation, editor-tool, data-asset, test, or documentation behavior in this Unreal Engine 5.6 project.
---

# Katana Feature

## Workflow

1. Inspect `git status --short` and identify unrelated WIP before edits.
2. Read this file, root `AGENTS.md`, then the narrowest project docs for the feature domain.
3. For multi-file or ambiguous work, produce a short plan that names files, tests, and acceptance checks before implementation.
4. Keep edits within the requested system. Do not opportunistically resave assets, clean generated folders, or refactor neighboring systems.
5. Add or update tests in `Source/KatanaCombatTest/` when behavior changes.
6. Run the `katana-verify` ladder before final reporting.

## Architecture Rules

Preserve the combat invariants in `CLAUDE.md`:

- Phases are exclusive; windows may overlap.
- Input is always buffered.
- Parry checks the attacker's parry window from defender-side logic.
- Hold checks button state at the window boundary.
- Cross-component delegates and shared combat types belong in `Public/CombatTypes.h`.

Read `references/architecture-routing.md` for file families and docs to open by feature area.

## Implementation Boundaries

Use Unreal Engine C++ naming and reflection conventions. Prefer existing components and data assets over new abstractions unless a new boundary removes real duplication or matches the current architecture.

Avoid touching binary assets unless the task explicitly requires Editor work. If asset work is required, document the exact assets touched and the live verification evidence used.

## Completion

Finish with a concise change summary, verification evidence, skipped checks, and any remaining risk. Do not claim runtime or animation behavior without build, automation, commandlet, UEMCP, or Editor evidence.
