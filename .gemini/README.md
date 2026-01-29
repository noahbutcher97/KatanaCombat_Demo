# Gemini Project Architecture

This directory contains the AI context and agent configuration for the KatanaCombat project.

## Structure

### 🧠 `agents/`
Specialized personas that handle specific tasks.
- **`lead_architect.toml`**: The Orchestrator. Analyzes tasks and routes them.
- **`combat_engineer.toml`**: C++ Logic, State Machines, Input.
- **`technical_animator.toml`**: Montages, Notifies, Timing.
- **`systems_designer.toml`**: Data Assets, Config, Tuning.
- **`qa_engineer.toml`**: Tests, Assertions, Validation.

### 🌐 `context-modes/`
JSON definitions that filter the codebase for specific domains.
- Used by agents to know which files are "in bounds".
- Includes: `animation.json`, `combat-logic.json`, `data-assets.json`, `testing.json`.

### 🪝 `hooks/`
Scripts that run automatically (e.g., on file open).
- `auto-context.ps1`: Detects the active domain based on open files.

### 📜 `config.json`
Main configuration file registering agents, paths, and hooks.

## Usage

**Start with the Lead Architect:**
```bash
/lead_architect "Implement a new dodge roll mechanic"
```
The Architect will analyze the request and tell you which specialized agent (e.g., `combat_engineer`) to use next, or guide the workflow directly.
