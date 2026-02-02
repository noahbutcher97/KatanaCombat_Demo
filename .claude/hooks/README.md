# Enhanced Hook System

**Purpose**: Automate quality checks, enforce best practices, and provide contextual reminders throughout the development workflow.

---

## Quick Start

### Currently Active Hooks (Auto-Enabled via config.json)
```
✅ agent-reminder       - Suggests agents for complex tasks (beforeToolCall)
✅ auto-context         - Auto-detects context mode on file open (afterFileOpen)
```

### Available Manual Hooks (Call Explicitly or via Git Hooks)
```
⏸️ after-edit             - Doc update reminders
⏸️ on-file-save           - Quick style checks (disabled by default)
⏸️ pre-commit-diagnostics - Advisory checks before commits
⏸️ before-commit          - Validation enforcer (blocks on critical issues)
```

**Note**: Only hooks listed in `.claude/config.json` run automatically. Manual hooks must be called explicitly via PowerShell or integrated into git hooks.

---

## Hook Inventory

### 1. **agent-reminder** (Auto-Enabled)
**Trigger**: `beforeToolCall` (Claude Code native)
**Purpose**: Reminds to delegate complex tasks to specialized agents

**What it does**:
- Analyzes tool calls for complexity indicators
- Suggests appropriate agents (ue-code-generator, code-auditor, etc.)
- Only triggers for multi-file changes or large edits

**Configuration**:
```powershell
# Disable temporarily (new standard)
$env:CLAUDE_SKIP_AGENT_REMINDERS = "1"

# Or (backward compatible)
$env:CLAUDE_NO_AGENT_REMINDERS = "1"
```

**Complexity Scoring**:
- Multi-file edit (.h + .cpp): +2 points
- Component/system files: +1 point
- New file creation: +2 points
- Large edits (>500 chars): +1 point
- **Threshold**: ≥3 points triggers reminder

---

### 2. **auto-context** (Auto-Enabled)
**Trigger**: `afterFileOpen` (Claude Code native)
**Purpose**: Auto-detects context mode and provides relevant reminders

**What it does**:
- Matches file patterns to context modes
- Shows relevant documentation
- Displays key principles for that domain
- Suggests `/mode` command for full switch

**Detection Rules**:
| File Pattern | Detected Mode | Reminders |
|--------------|---------------|-----------|
| `AnimNotify*`, `Animation/` | animation | Phase notify requirements, timing rules |
| `CombatComponent*`, `ActionQueue*` | combat-logic | Design principles, input buffering |
| `AttackData*`, `Data/` | data-assets | Three-tier architecture, validation |
| `Editor/`, `*Customization*` | editor-ui | Slate APIs, editor patterns |
| `*Test.cpp`, `Tests/` | testing | Test frameworks, existing structure |
| `docs/*.md`, `README.md` | documentation | Doc standards, formatting |

**Configuration**:
```powershell
# Disable temporarily
$env:CLAUDE_DISABLE_AUTO_CONTEXT = "1"
```

---

### 3. **after-edit** (Manual)
**Trigger**: After editing significant files (manual invocation)
**Purpose**: Reminds to update documentation when code changes

**What it checks**:
- **CombatComponent** edits → Update ARCHITECTURE.md, API_REFERENCE.md
- **AttackData** edits → Update ATTACK_CREATION.md, ARCHITECTURE_QUICK.md
- **AnimNotify** edits → Update PHASE_SYSTEM_MIGRATION.md
- **CombatTypes.h** edits → Multiple docs (critical file)
- **Test** files → Update test README

**How to use**:
```powershell
# Call manually after editing
$env:FILE_PATH = "Source/KatanaCombat/Public/Core/CombatComponent.h"
$env:CHANGE_TYPE = "modified"
powershell -ExecutionPolicy Bypass -File .claude/hooks/after-edit.ps1
```

**Or ask Claude**: "Run after-edit hook for CombatComponent.h"

**Configuration**:
```powershell
# Disable
$env:CLAUDE_SKIP_AFTER_EDIT = "1"
```

---

### 4. **on-file-save** (Disabled by Default)
**Trigger**: On file save (manual invocation)
**Purpose**: Quick style and formatting checks for immediate feedback

**What it checks**:
- ✅ Include guards (`#pragma once`)
- ✅ UPROPERTY without Category
- ✅ BlueprintCallable without DisplayName/Tooltip
- ⚠️ Potential null pointer access
- ⚠️ TODO/FIXME/HACK comments count
- ⚠️ Large functions (>100 lines)
- ❌ Tick usage (anti-pattern for this project)
- ❌ Delegates declared outside CombatTypes.h

**Why disabled by default**: Can be noisy for rapid development

**How to enable**:
```powershell
# Enable for session
$env:CLAUDE_SKIP_ON_SAVE = "0"  # Enable (default is disabled)

# Call manually
$env:FILE_PATH = "path/to/file.cpp"
powershell -ExecutionPolicy Bypass -File .claude/hooks/on-file-save.ps1
```

**Configuration**:
```powershell
# Disable
$env:CLAUDE_SKIP_ON_SAVE = "1"
```

---

### 5. **pre-commit-diagnostics** (Auto-Enabled)
**Trigger**: `beforeCommit` (via git hooks if configured)
**Purpose**: Advisory diagnostics check on staged files

**What it does**:
- Detects staged C++ files
- Provides context-specific checklists
- Suggests relevant commands
- Shows reminders based on file types

**Output Example**:
```
🔍 Running pre-commit diagnostics check...
📂 Checking 3 staged C++ files...

⚙️ CombatComponent files modified:
   - Check for null pointer dereferences
   - Validate state transitions use CanTransitionTo()

💡 Recommended Actions:
   1. /check-warnings
   2. /validate-combat
```

**Configuration**:
```powershell
# Skip
$env:CLAUDE_SKIP_DIAGNOSTICS = "1"
```

---

### 6. **before-commit** (Manual)
**Trigger**: Before git commit (manual or via git hook)
**Purpose**: Comprehensive validation enforcer

**What it validates**:
1. **Critical files** - Warns if modifying system-critical files
2. **Component architecture** - Checks compliance for component changes
3. **Phase system** - Validates AnimNotify changes
4. **Test requirements** - Reminds to run tests if test files modified
5. **Documentation dates** - Checks CLAUDE.md has recent date
6. **Commit size** - Warns on large commits (>20 files)
7. **Mixed concerns** - Detects anti-pattern of mixing unrelated changes

**Exit Codes**:
- `0` = Pass (warnings are advisory)
- `1` = Fail (errors found, blocked - use --force to bypass)

**Integration with Git**:
```bash
# Add to .git/hooks/pre-commit
powershell -ExecutionPolicy Bypass -File .claude/hooks/before-commit.ps1
if [ $? -ne 0 ]; then exit 1; fi
```

**Configuration**:
```powershell
# Skip validation
$env:CLAUDE_SKIP_VALIDATION = "1"

# Quick mode
.claude/hooks/before-commit.ps1 -Quick

# Force bypass
.claude/hooks/before-commit.ps1 -Force
```

---

## Configuration System

### Hook Profiles

Defined in `.claude/hooks-config.json`:

**1. default** (Active)
- agent-reminder ✅
- auto-context ✅
- after-edit ✅
- pre-commit-diagnostics ✅
- before-commit ✅

**2. strict** (Maximum validation)
- All hooks enabled, including on-file-save
- Blocks on warnings (not just errors)

**3. minimal** (Critical only)
- auto-context ✅
- before-commit ✅

**4. speed** (Minimal overhead)
- auto-context ✅ only

### Switching Profiles

```powershell
# Set profile
$env:CLAUDE_HOOK_PROFILE = "strict"

# Disable all hooks
$env:CLAUDE_DISABLE_HOOKS = "1"
```

### Context-Specific Settings

Hooks automatically adjust based on active context:

**Testing Context**:
- Disables `on-file-save` (test files have different requirements)

**Documentation Context**:
- Disables `pre-commit-diagnostics` and `on-file-save` (docs don't need code checks)

---

## Workflow Integration

### Example 1: Feature Development Workflow

```bash
# 1. Switch context
/mode combat-logic
# auto-context hook shows: "Design principles, input buffering rules"

# 2. Edit CombatComponent.cpp
# (Open file in IDE)
# auto-context hook: "Combat-logic context detected"

# 3. Make changes
# (Edit code)

# 4. Check for doc updates (manual)
powershell .claude/hooks/after-edit.ps1
# Output: "Update ARCHITECTURE.md, API_REFERENCE.md"

# 5. Before committing
git add .
git commit -m "Add new combo system"
# pre-commit-diagnostics hook: Shows checklist

# 6. Run validation (manual or via git hook)
powershell .claude/hooks/before-commit.ps1
# Output: Validates changes, suggests /validate-combat
```

---

### Example 2: Quick Bug Fix Workflow (Speed Profile)

```bash
# 1. Switch to speed profile
$env:CLAUDE_HOOK_PROFILE = "speed"

# 2. Open file
# Only auto-context hook runs (fast)

# 3. Fix and commit
git commit -m "Fix null pointer in AttackData"
# Minimal overhead, quick commit
```

---

### Example 3: Pre-Release Workflow (Strict Profile)

```bash
# 1. Switch to strict profile
$env:CLAUDE_HOOK_PROFILE = "strict"

# 2. Enable on-save checks
$env:CLAUDE_SKIP_ON_SAVE = "0"  # Enable (default is disabled)

# 3. Every save triggers style check
# Immediate feedback on issues

# 4. Commit blocked on any warnings
# Ensures maximum quality
```

---

## Git Hooks Integration

### Setup Git Pre-Commit Hook

Create `.git/hooks/pre-commit`:

```bash
#!/bin/bash

echo "Running Claude Code validation..."

# Run before-commit hook
powershell -ExecutionPolicy Bypass -File .claude/hooks/before-commit.ps1

exit_code=$?

if [ $exit_code -ne 0 ]; then
    echo ""
    echo "Commit blocked by validation. Fix issues or use:"
    echo "  git commit --no-verify"
    exit 1
fi

exit 0
```

Make executable:
```bash
chmod +x .git/hooks/pre-commit
```

### Bypass When Needed

```bash
# Skip git hooks
git commit --no-verify -m "Quick fix"

# Or disable specific hook
$env:CLAUDE_SKIP_VALIDATION = "1"
git commit -m "message"
```

---

## Performance Impact

| Hook | Trigger Frequency | Overhead | When to Disable |
|------|------------------|----------|-----------------|
| agent-reminder | Tool calls | ~5ms | Never (minimal) |
| auto-context | File open | ~10-20ms | Speed-critical sessions |
| after-edit | Manual | ~50ms | N/A (manual) |
| on-file-save | Every save | ~100-200ms | Rapid development |
| pre-commit-diagnostics | Commit | ~500ms | Fast commits |
| before-commit | Manual/commit | ~1-2s | Trusted changes |

---

## Troubleshooting

### "Hook not running"
**Cause**: Environment variable disabling it
**Fix**: Check `$env:CLAUDE_SKIP_[HOOK]` variables

### "Too many reminders"
**Cause**: Multiple hooks triggering
**Fix**: Use `minimal` or `speed` profile

### "Git hooks not working"
**Cause**: Hooks not executable or not configured
**Fix**:
```bash
chmod +x .git/hooks/pre-commit
git config core.hooksPath .git/hooks
```

### "Hooks slowing development"
**Cause**: on-file-save enabled or strict profile
**Fix**:
```powershell
$env:CLAUDE_HOOK_PROFILE = "speed"
$env:CLAUDE_SKIP_ON_SAVE = "1"
```

---

## Best Practices

### 1. **Use Profiles Appropriately**
- `speed` - Daily development
- `default` - Standard work
- `strict` - Pre-release, critical changes

### 2. **Don't Fight the Hooks**
- If a hook suggests something, there's usually a reason
- Use `/check-warnings` or `/validate-combat` when suggested
- Update docs when reminded

### 3. **Manual Hooks Are Your Friends**
- `after-edit` prevents doc drift
- `before-commit` catches issues early
- Call them explicitly for important changes

### 4. **Context-Aware**
- Hooks adapt to your active context
- Use `/mode` to switch contexts
- Less noise, more relevant feedback

---

## Future Enhancements

- [ ] Integration with CI/CD pipeline
- [ ] Hook execution metrics and analytics
- [ ] Smart hook suggestion based on task
- [ ] Hook chaining and dependencies
- [ ] Visual Studio / Rider integration
- [ ] Webhook notifications for critical issues

---

## Files Created

```
.claude/hooks/
├── agent-reminder.ps1          ← Complex task delegation (auto-enabled)
├── auto-context.ps1            ← Context auto-detection (auto-enabled)
├── after-edit.ps1              ← Documentation reminders (manual)
├── on-file-save.ps1            ← Quick style checks (disabled by default)
├── pre-commit-diagnostics.ps1  ← Advisory pre-commit (manual/git hook)
├── before-commit.ps1           ← Validation enforcer (manual/git hook)
├── post-commit.ps1             ← Post-commit notifications (git hook)
├── session-start.ps1           ← Session initialization (experimental)
└── README.md                   ← This guide
```

### Undocumented/Experimental Hooks

**post-commit.ps1**: Git hook for post-commit notifications and cleanup tasks.

**session-start.ps1**: Experimental hook for session initialization. May be used for context loading or state restoration.

---

**Ready to use!** Hooks are active and will provide contextual guidance throughout your development workflow.
