# Codex Agentic Readiness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ready KatanaCombat for Codex-based agentic development without touching gameplay code or Unreal assets.

**Architecture:** Keep durable repo guidance in `AGENTS.md`, Codex runtime configuration in `.codex/`, reusable workflows in `.agents/skills/`, and evidence in `docs/agentic-readiness/`. Start with no active project hooks. Port concepts from `.claude/` and `.gemini/` selectively rather than copying runtime-specific commands.

**Tech Stack:** Codex CLI project config, Codex hooks/rules, Codex skills, PowerShell, Unreal Engine 5.6 C++ project conventions.

## Global Constraints

- Do not edit gameplay source, `Content/`, maps, Blueprints, or generated folders for this readiness pass.
- Preserve existing user WIP; do not revert or clean dirty files.
- Run destructive git commands only after explicit user approval and path/state review.
- Verify syntax and command-policy scaffolding before claiming completion.

---

### Task 1: Baseline Audit And Instruction Routing

**Files:**
- Modify: `AGENTS.md`
- Create: `docs/agentic-readiness/2026-06-20-readiness-audit.md`

**Interfaces:**
- Consumes: current repo docs, `CLAUDE.md`, `.claude/INDEX.md`, `.mcp.json`, `git status`.
- Produces: root read order, workflow constraints, and audit summary for future Codex sessions.

- [x] Inspect current guidance, MCP, and dirty-worktree state.
- [x] Expand `AGENTS.md` with Codex-specific workflow rules.
- [x] Create the readiness audit document.
- [x] Confirm no gameplay source or assets were edited.

### Task 2: Codex Project Layer

**Files:**
- Create: `.codex/config.toml`
- Create: `.codex/hooks.json`
- Create: `.codex/hooks/pre-tool-use-safety.cmd`
- Create: `.codex/hooks/pre-tool-use-safety.ps1`
- Create: `.codex/rules/default.rules`

**Interfaces:**
- Consumes: `.mcp.json` UEMCP values and Codex manual config/hook/rules schema.
- Produces: Codex-native project config, prompt-based command policy, and dormant hook templates.

- [x] Add `uemcp` MCP config to `.codex/config.toml`.
- [x] Add `uemcp` to the live Codex MCP registry with `codex mcp add`.
- [x] Add high-risk command rules.
- [x] Add dormant pre-tool safety hook files for optional future enablement.
- [x] Disable active project hooks by default.
- [x] Test the dormant safety hook with dangerous and benign command samples.
- [ ] Restart Codex in a fresh session.
- [x] Confirm `uemcp` appears in live Codex MCP discovery in the current CLI.
- [ ] Confirm `uemcp` remains visible after restarting Codex.

### Task 3: Repo Skills

**Files:**
- Create: `.agents/skills/katana-verify/SKILL.md`
- Create: `.agents/skills/katana-verify/references/verification-ladder.md`
- Create: `.agents/skills/katana-verify/scripts/summarize-automation-log.ps1`
- Create: `.agents/skills/katana-feature/SKILL.md`
- Create: `.agents/skills/katana-feature/references/architecture-routing.md`
- Create: `.agents/skills/katana-bug-triage/SKILL.md`
- Create: `.agents/skills/katana-bug-triage/references/evidence-lanes.md`

**Interfaces:**
- Consumes: `CLAUDE.md`, `Source/KatanaCombatTest/README.md`, `.claude/agents/README.md`, `.claude/commands/README.md`.
- Produces: three focused Codex skills discoverable from `.agents/skills/`.

- [x] Scaffold skills with the Codex skill initializer.
- [x] Replace generated placeholders with concise project workflows.
- [x] Add references and deterministic log-summary script.
- [ ] Forward-test skills on one low-risk pilot task.

### Task 4: Verification

**Files:**
- Validate all created files.

**Interfaces:**
- Consumes: PowerShell parser, JSON parser, Codex execpolicy, skill validator, `git status`.
- Produces: final verification evidence.

- [x] Parse JSON and PowerShell files.
- [x] Run the skill validator for each new skill.
- [x] Test the PowerShell log-summary script on the existing log.
- [x] Test `.codex/rules/default.rules` with `codex execpolicy check`.
- [x] Report remaining manual restart/trust steps.
