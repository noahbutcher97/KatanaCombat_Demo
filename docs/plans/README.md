# Development Plans

This folder contains implementation plans for significant features.

## Structure

```
plans/
├── README.md                         ← This file
├── combat-polish-plan.md             ← Normal attack effects + camera (ACTIVE)
├── gap-tracker.md                    ← Gap coverage matrix (REFERENCE)
├── preview-tool-refactor-plan.md     ← PT-11 refactor (COMPLETE, pending merge)
├── archive/                          ← Completed/superseded plans
│   ├── gap-mitigation-plan-2026-02-03-pre-split.md  ← Original monolith
│   └── ...
└── *.md                              ← Active plans
```

## Plan Lifecycle

1. **Created**: Plan written during planning phase
2. **Active**: Plan file in `plans/` root during implementation
3. **Completed**: Moved to `archive/` with date suffix

## Current Active Plans

| Plan | Status | Description |
|------|--------|-------------|
| [Combat Polish](combat-polish-plan.md) | Planning | Normal attack hitstop, camera shake, audio, VFX, camera awareness |
| [Gap Tracker](gap-tracker.md) | Reference | 149 gaps tracked: 51+ done, ~84 pending, 14 deferred |
| [Preview Tool Refactor](preview-tool-refactor-plan.md) | Complete (pending merge) | PT-11 "God Widget" decomposition on feature branch |

## Recently Archived

| Plan | Archived | Description |
|------|----------|-------------|
| [Gap Mitigation (Monolith)](archive/gap-mitigation-plan-2026-02-03-pre-split.md) | 2026-02-03 | Split into combat-polish, gap-tracker, preview-tool-refactor |
| [Paired Animation (Pre-restructure)](archive/paired-animation-plan-2026-01-31-pre-restructure.md) | 2026-01-31 | Initial paired animation implementation |
| [Death System](archive/death-system-2025-01-29-COMPLETED.md) | 2025-01-29 | Directional death animations, ragdoll, bIsDead flag |

## Audits

- [Unified Audit Synthesis 2026-02-03](../audits/AUDIT_SYNTHESIS_2026-02-03.md) - Cross-referenced findings from two parallel audits (38 unified action items)
- [Claude Audit 2026-02-03](../audits/AUDIT_CLAUDE_2026-02-03.md) - 5-agent analytical audit (code bugs, gap verification, research, docs, architecture)
- [Copilot Audit 2026-02-03](../audits/AUDIT_COPILOT_2026-02-03.md) - System-level audit with implementation roadmap

## Supporting Documents

- [Audit Synthesis 2026-01-30](../audits/archive/AUDIT_SYNTHESIS_2026-01-30.md) - Documentation completeness audit (historical)
- [Log Analysis 2026-01-30](../reference/LOG_ANALYSIS_2026-01-30.md) - Bug pattern analysis from test logs

## Upcoming Work

See `CLAUDE.md` "Active Development" section for current implementation status.
