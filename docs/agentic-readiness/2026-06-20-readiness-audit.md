# Agentic Codex Readiness Audit - 2026-06-20

## Summary

KatanaCombat is suitable for Codex-based agentic workflows after adding a Codex-native instruction/config layer and reducing the risk of accidental asset or WIP mutation. The repo already has useful Claude/Gemini workflow material, a strong test module, CI definitions, and rich architecture docs. The main gaps were Codex visibility and operational guardrails.

## Findings

- `AGENTS.md` existed as a concise contributor guide but did not yet route Codex through project-specific workflow anchors.
- `.mcp.json` defined `uemcp`, but live `codex mcp list` did not initially expose `uemcp`; native `codex mcp add` registration was required for current CLI discovery.
- No `.codex/` project layer existed before this work.
- No `.agents/skills/` repo skills existed before this work.
- `.claude/` and `.gemini/` contain valuable workflow ideas, but they are runtime-specific and should be ported selectively.
- `git status --porcelain` showed 199 changed entries: 160 deleted, 7 modified, and 32 untracked. Most are Unreal assets or project content. Agentic workflows must treat this as user WIP unless explicitly told otherwise.

## Implemented File Set

- `AGENTS.md`: expanded with Codex read order, verification ladder, dirty-worktree rules, and skill routing.
- `.codex/config.toml`: project-local Codex config and `uemcp` MCP definition.
- `.codex/hooks.json`: no active hooks by default.
- `.codex/hooks/pre-tool-use-safety.cmd`: dormant Windows entrypoint for optional future safety hook enablement.
- `.codex/hooks/pre-tool-use-safety.ps1`: dormant deterministic command safety hook template.
- `.codex/rules/default.rules`: prompt-only command policy for high-risk git and submit operations.
- `.agents/skills/katana-verify/`: verification workflow and log-summary helper.
- `.agents/skills/katana-feature/`: feature workflow and architecture routing reference.
- `.agents/skills/katana-bug-triage/`: evidence-first bug triage workflow.
- Global Codex MCP registry: `uemcp` was added with `codex mcp add` using the project UEMCP environment values.

## Remaining Operator Actions

1. Restart Codex from `D:\UnrealProjects\5.6\KatanaCombat` so `.codex/`, rules, and repo skills load.
2. In the new session, run `/mcp` or `codex mcp list` and confirm `uemcp` still appears.
3. Run one low-risk pilot task using `katana-verify` to validate the workflow before broad code or asset work.
