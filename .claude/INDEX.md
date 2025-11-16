# Claude Code Infrastructure - Navigation Hub

**Quick Reference**: All `.claude/` infrastructure components for the KatanaCombat project

---

## 📂 Directory Structure

```
.claude/
├── commands/          ← Slash commands (/mode, /check-warnings, etc.)
├── agents/            ← Specialized AI agents (router, ue-code-generator, etc.)
├── hooks/             ← Auto-triggers (beforeToolCall, afterFileOpen)
├── scripts/           ← PowerShell utilities (context-tracker, agent-coordinator)
├── context-modes/     ← Context mode configs (animation, combat-logic, etc.)
├── diagnostics/       ← Diagnostics system documentation
├── git-hooks-templates/ ← Git integration templates
├── config.json        ← Active hook configuration
├── hooks-config.json  ← Hook profiles and settings
├── diagnostics-config.json  ← False positive filters
└── INDEX.md           ← This file
```

---

## 🎯 Quick Start by Use Case

### "I want to..."

#### ...switch context to a specific domain
→ **Commands**: `/mode [name]` | `/mode list`
→ **Modes**: animation, combat-logic, data-assets, editor-ui, testing, documentation, full
→ **Docs**: [`.claude/context-modes/README.md`](context-modes/README.md)

#### ...validate my code before committing
→ **Commands**: `/validate-combat` | `/check-warnings` | `/pre-commit`
→ **Hooks**: `before-commit.ps1`, `pre-commit-diagnostics.ps1`
→ **Docs**: [`.claude/diagnostics/README.md`](diagnostics/README.md)

#### ...implement a new feature with AI assistance
→ **Agents**: `router` (auto-selects) | `pipeline-feature` (full workflow)
→ **Commands**: `/clarify` (gather requirements first)
→ **Docs**: [`.claude/agents/README.md`](agents/README.md)

#### ...fix a bug systematically
→ **Agents**: `pipeline-bugfix` (diagnose → fix → verify)
→ **Commands**: `/fix-crash` | `/post-fix`
→ **Docs**: [`.claude/agents/README.md`](agents/README.md)

#### ...check code quality and compliance
→ **Agents**: `design-compliance-auditor`, `code-auditor`
→ **Commands**: `/full-audit` | `/sync-docs`
→ **Docs**: [`.claude/agents/README.md`](agents/README.md)

#### ...understand the command system
→ **Docs**: [`.claude/commands/README.md`](commands/README.md)
→ **Visual Guide**: [`.claude/commands/VISUAL_GUIDE.md`](commands/VISUAL_GUIDE.md)
→ **Summary**: [`.claude/commands/COMMAND_SUMMARY.md`](commands/COMMAND_SUMMARY.md)

---

## 🔧 Component Overview

### 1. Commands (Slash Commands)
**Location**: `.claude/commands/*.md`

| Command | Purpose | Execution Time |
|---------|---------|----------------|
| `/mode [name]` | Switch context mode | Instant |
| `/mode status` | Show context system status | Instant |
| `/mode suggest` | Get AI mode recommendation | ~10s |
| `/clarify` | Interactive requirements gathering | 2-5 min |
| `/validate-combat` | Comprehensive combat validation | 2-3 min |
| `/check-warnings` | Detailed diagnostics analysis | 1-2 min |
| `/diagnostics-dashboard` | Health monitoring | 1 min |
| `/sync-docs` | Code-documentation sync check | 3-5 min |
| `/full-audit` | Complete system audit | 8-12 min |
| `/fix-crash` | Systematic crash debugging | 5-10 min |
| `/post-fix` | Post-fix verification | 2-3 min |
| `/pre-commit` | Pre-commit validation | 1-2 min |
| `/generate-tests` | Test generation | 3-5 min |

**Documentation**: [commands/README.md](commands/README.md)

---

### 2. Agents (Specialized AI)
**Location**: `.claude/agents/*.md`

| Agent | Purpose | Use When |
|-------|---------|----------|
| `router` | Meta-agent for task routing | Unsure which agent to use |
| `ue-code-generator` | Code implementation | Implementing features/fixes |
| `design-compliance-auditor` | Architecture validation | Checking design compliance |
| `code-auditor` | Quality review | Code review, best practices |
| `pipeline-feature` | Full feature workflow | Complex feature implementation |
| `pipeline-bugfix` | Bug diagnosis pipeline | Unclear bug root cause |

**Documentation**: [agents/README.md](agents/README.md)

---

### 3. Hooks (Auto-Triggers)
**Location**: `.claude/hooks/*.ps1`

#### Auto-Active (in config.json)
| Hook | Trigger | Purpose |
|------|---------|---------|
| `agent-reminder` | beforeToolCall | Suggests agents for complex tasks |
| `auto-context` | afterFileOpen | Auto-detects context mode |

#### Manual (Call Explicitly)
| Hook | Purpose | When to Use |
|------|---------|-------------|
| `after-edit` | Doc update reminders | After significant code changes |
| `on-file-save` | Quick style checks | Continuous validation (optional) |
| `pre-commit-diagnostics` | Advisory checks | Before commits |
| `before-commit` | Validation enforcer | Git pre-commit hook |

**Documentation**: [hooks/README.md](hooks/README.md)

---

### 4. Context Modes
**Location**: `.claude/context-modes/*.json`

| Mode | Focus | Token Reduction | Use When |
|------|-------|-----------------|----------|
| `animation` | AnimNotify, montages, phase transitions | ~75% | Animation work |
| `combat-logic` | State machines, input, components | ~70% | Combat mechanics |
| `data-assets` | AttackData, settings, config | ~80% | Attack design |
| `editor-ui` | Custom editors, Slate UI | ~85% | Editor tooling |
| `testing` | Unit/integration tests | ~80% | Writing tests |
| `documentation` | Markdown docs, guides | ~90% | Documentation |
| `full` | All files (no filtering) | 0% | Exploration, multi-domain |

**Documentation**: [context-modes/README.md](context-modes/README.md)
**Planned Features**: [context-modes/INTELLIGENT_SWITCHING.md](context-modes/INTELLIGENT_SWITCHING.md)

---

### 5. Scripts (Utilities)
**Location**: `.claude/scripts/*.ps1`

| Script | Purpose | Usage |
|--------|---------|-------|
| `context-tracker.ps1` | Context switch analytics | `powershell .claude/scripts/context-tracker.ps1 -Action status` |
| `agent-coordinator.ps1` | Agent management | `powershell .claude/scripts/agent-coordinator.ps1 -Action validate` |
| `get-context-files.ps1` | Context file filtering | Called by diagnostics commands |

---

### 6. Configuration Files

#### config.json
**Purpose**: Active hook configuration (which hooks auto-run)
**Current**: 2 auto-active hooks (`agent-reminder`, `auto-context`)

#### hooks-config.json
**Purpose**: Hook behavior settings
**Features**: 6 hooks defined, environment variable controls

#### diagnostics-config.json
**Purpose**: False positive filters for IDE diagnostics
**Features**: Blueprint-exposed filters, editor-only patterns, categorization rules
**Documentation**: [diagnostics/README.md](diagnostics/README.md)

---

## 🌐 Environment Variables

### Hook Control (Standard: CLAUDE_SKIP_*)
| Variable | Purpose | Default | Backward Compat |
|----------|---------|---------|------------------|
| `CLAUDE_SKIP_AGENT_REMINDERS` | Disable agent suggestions | Off | `CLAUDE_NO_AGENT_REMINDERS` |
| `CLAUDE_DISABLE_AUTO_CONTEXT` | Disable auto-context detection | Off | - |
| `CLAUDE_SKIP_AFTER_EDIT` | Disable doc reminders | Off | - |
| `CLAUDE_SKIP_ON_SAVE` | Disable save-time checks | Off (already disabled) | - |
| `CLAUDE_SKIP_DIAGNOSTICS` | Disable diagnostics hook | Off | - |
| `CLAUDE_SKIP_VALIDATION` | Disable validation hook | Off | - |

### Feature Control
| Variable | Purpose | Default |
|----------|---------|---------|
| `CLAUDE_AUTO_SWITCH_CONTEXT` | Enable intelligent context switching | Off (planned feature) |

### Usage
```powershell
# PowerShell
$env:CLAUDE_SKIP_AGENT_REMINDERS = "1"  # Disable

# Bash/Zsh
export CLAUDE_SKIP_AGENT_REMINDERS=1
```

---

## 📚 Documentation Hub

### Master Guides
- **[SYSTEMIC_TOOLS.md](SYSTEMIC_TOOLS.md)** - Implementation summary, all phases
- **[INDEX.md](INDEX.md)** - This file (navigation hub)
- **[CHANGELOG.md](CHANGELOG.md)** - Infrastructure change history

### Subsystem Guides
- **[commands/README.md](commands/README.md)** - Command system (1,800 lines)
- **[agents/README.md](agents/README.md)** - Agent ecosystem (1,200 lines)
- **[hooks/README.md](hooks/README.md)** - Hook system (1,000 lines)
- **[context-modes/README.md](context-modes/README.md)** - Context modes (800 lines)
- **[diagnostics/README.md](diagnostics/README.md)** - Diagnostics system (900 lines)

### Quick References
- **[commands/VISUAL_GUIDE.md](commands/VISUAL_GUIDE.md)** - Command cheat sheet
- **[commands/COMMAND_SUMMARY.md](commands/COMMAND_SUMMARY.md)** - Command inventory

---

## 🎓 Common Workflows

### Workflow 1: Feature Development
```bash
# 1. Clarify requirements
/clarify

# 2. Switch to relevant context
/mode data-assets        # Design attack
/mode animation          # Set up montage
/mode combat-logic       # Implement mechanics

# 3. Use pipeline for full implementation
# (Claude automatically suggests router/pipeline-feature)

# 4. Write tests
/mode testing
/generate-tests

# 5. Validate before commit
/validate-combat
/pre-commit
```

### Workflow 2: Bug Investigation
```bash
# 1. Use diagnostic tools
/fix-crash

# 2. Or use pipeline for systematic approach
# (Use pipeline-bugfix agent)

# 3. Verify fix
/post-fix
/validate-combat
```

### Workflow 3: Code Review
```bash
# 1. Check compliance
/sync-docs

# 2. Full audit
/full-audit

# 3. Run diagnostics
/check-warnings

# 4. Monitor health
/diagnostics-dashboard
```

### Workflow 4: Pre-Commit
```bash
# 1. Run validation
/pre-commit

# 2. Check warnings
/check-warnings

# 3. Validate combat (if changed)
/validate-combat

# 4. Commit (git hooks will run before-commit.ps1)
git commit
```

---

## 🔍 Troubleshooting

### Command Not Found
- Ensure file is in `.claude/commands/` with `.md` extension
- Restart Claude Code

### Hook Not Running
- Check `.claude/config.json` for hook entry
- Verify PowerShell script has correct path
- Check environment variables (CLAUDE_SKIP_* disables hooks)

### Agent Not Responding
- Check agent file exists in `.claude/agents/`
- Ensure agent has access to required tools
- Try `router` agent for auto-selection

### Context Mode Not Working
- Verify `.json` file exists in `.claude/context-modes/`
- Check file pattern syntax (glob patterns)
- Use `/mode current` to verify active mode

---

## 🚀 Extension Points

### Adding a New Command
1. Create `.claude/commands/my-command.md`
2. Add command description and instructions
3. Reference in `commands/README.md`
4. Test with `/my-command`

### Adding a New Context Mode
1. Create `.claude/context-modes/my-mode.json`
2. Define include/exclude patterns
3. Add to `auto-context.ps1` detection rules
4. Document in `context-modes/README.md`

### Adding a New Hook
1. Create `.claude/hooks/my-hook.ps1`
2. Add to `.claude/hooks-config.json`
3. Add to `.claude/config.json` if auto-active
4. Document in `hooks/README.md`

### Adding a New Agent
1. Create `.claude/agents/my-agent.md`
2. Define agent purpose and tools
3. Add routing logic to `router.md`
4. Document in `agents/README.md`

---

## 📊 System Health

**Current Status**: ✅ All systems operational

**Component Coverage**:
- Commands: 13 commands
- Agents: 6 agents
- Hooks: 6 hooks (2 auto-active)
- Context Modes: 7 modes
- Scripts: 3 utilities

**Documentation Coverage**: 100% (all components documented)

**Integration Status**:
- ✅ Commands ↔ Scripts: Fully integrated
- ✅ Hooks ↔ Commands: Properly linked
- ✅ Agents ↔ Tools: Fully mapped
- ✅ Context Modes ↔ Hooks: Integrated

---

**Last Updated**: 2025-11-14
**Maintenance**: Update this index when adding new components
**Questions**: See subsystem READMEs or [SYSTEMIC_TOOLS.md](SYSTEMIC_TOOLS.md)
