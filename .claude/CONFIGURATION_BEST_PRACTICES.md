# Configuration Best Practices - KatanaCombat Claude Integration

**Date**: 2025-11-19
**Status**: Active Standard

---

## Philosophy

**Principle**: Configuration should be **persistent**, **discoverable**, and **consistent** across all tool invocations.

**Problem with Environment Variables**: Environment variables don't persist across separate PowerShell invocations in Claude Code, leading to inconsistent state and user confusion.

**Solution**: Use **flag files** for persistent feature toggles, reserve environment variables only for **transient context data**.

---

## Configuration Patterns

### ✅ Pattern 1: Feature Toggles (Use Flag Files)

**Use Case**: Settings that should persist across sessions and tool invocations

**Examples**:
- Auto-switching enabled/disabled
- Hook enablement (diagnostics, validation, reminders)
- Debug modes
- Feature flags

**Implementation**:

```powershell
# Check if feature is enabled
$featureEnabled = Test-Path ".claude/.feature-name-enabled"

# Enable feature
New-Item -Path ".claude/.feature-name-enabled" -ItemType File -Force | Out-Null

# Disable feature
if (Test-Path ".claude/.feature-name-enabled") {
    Remove-Item ".claude/.feature-name-enabled" -Force
}
```

**File Naming Convention**:
- Location: `.claude/`
- Pattern: `.feature-name-enabled` (hidden file with `-enabled` suffix)
- Examples:
  - `.claude/.auto-switch-enabled`
  - `.claude/.skip-diagnostics-enabled`
  - `.claude/.debug-mode-enabled`

**Benefits**:
- ✅ Persists across tool invocations
- ✅ Discoverable (can list with `ls .claude/.*.enabled`)
- ✅ No environment pollution
- ✅ Works in all shells (PowerShell, Bash, etc.)
- ✅ Easy to check status (`Test-Path`)

---

### ✅ Pattern 2: Transient Context Data (Use Environment Variables)

**Use Case**: Data passed from Claude Code to hooks **per invocation**

**Examples**:
- `$env:FILE_PATH` - File being edited
- `$env:CHANGE_TYPE` - Type of change (new/modified/deleted)
- `$env:TOOL_NAME` - Tool being invoked
- `$env:TOOL_ARGS` - Tool arguments

**Implementation**:

```powershell
# Read context data (set by Claude Code)
$filePath = $env:FILE_PATH
$changeType = $env:CHANGE_TYPE
```

**Why Environment Variables Are OK Here**:
- ✅ Data is transient (only relevant for current invocation)
- ✅ Set by external system (Claude Code)
- ✅ Different for each hook invocation
- ✅ Not user-configurable

---

### ✅ Pattern 3: Structured Configuration (Use JSON Files)

**Use Case**: Complex configuration with multiple settings

**Examples**:
- Context mode definitions
- Learning system data
- Analytics/tracking data
- User preferences

**Implementation**:

```powershell
# Load configuration
$configPath = ".claude/settings.json"
if (Test-Path $configPath) {
    $config = Get-Content $configPath | ConvertFrom-Json
    $setting = $config.featureName
}

# Save configuration
$config | ConvertTo-Json -Depth 10 | Set-Content $configPath
```

**File Examples**:
- `.claude/settings.json` - User preferences
- `.claude/.context-learning.json` - ML learning data
- `.claude/.context-history.json` - Analytics data
- `.claude/context-modes/*.json` - Mode definitions

---

## Migration Guide

### From Environment Variable to Flag File

**Before** (Environment Variable):
```powershell
# Check
if ($env:CLAUDE_AUTO_SWITCH_CONTEXT -eq "1") {
    # Feature enabled
}

# Enable (doesn't persist!)
$env:CLAUDE_AUTO_SWITCH_CONTEXT = "1"
```

**After** (Flag File):
```powershell
# Check
if (Test-Path ".claude/.auto-switch-enabled") {
    # Feature enabled
}

# Enable (persists!)
New-Item -Path ".claude/.auto-switch-enabled" -ItemType File -Force | Out-Null

# Disable
Remove-Item ".claude/.auto-switch-enabled" -Force -ErrorAction SilentlyContinue
```

---

## Current Status (2025-11-19)

### ✅ Migrated to Flag Files

- **Auto-switching**: `.claude/.auto-switch-enabled`
  - Used by: `context-tracker.ps1`, `auto-context.ps1`, `/mode` command
  - Status: ✅ Fully migrated

### ⚠️ Pending Migration (Feature Toggles)

These should be migrated to flag files:

1. **`CLAUDE_SKIP_DIAGNOSTICS`** → `.claude/.skip-diagnostics-enabled`
   - File: `hooks/pre-commit-diagnostics.ps1:10`
   - Benefit: Persistent diagnostic skipping

2. **`CLAUDE_SKIP_VALIDATION`** → `.claude/.skip-validation-enabled`
   - File: `hooks/before-commit.ps1:10`
   - Benefit: Persistent validation skipping

3. **`CLAUDE_SKIP_AFTER_EDIT`** → `.claude/.skip-after-edit-enabled`
   - File: `hooks/after-edit.ps1:10`
   - Benefit: Persistent after-edit hook skipping

4. **`CLAUDE_SKIP_AGENT_REMINDERS`** / **`CLAUDE_NO_AGENT_REMINDERS`** → `.claude/.skip-agent-reminders-enabled`
   - File: `hooks/agent-reminder.ps1:13`
   - Benefit: Persistent reminder skipping (consolidate two vars into one)

5. **`CLAUDE_SKIP_ON_SAVE`** → `.claude/.skip-on-save-enabled`
   - File: `hooks/on-file-save.ps1:9`
   - Benefit: Persistent on-save hook skipping

### ✅ Keep As Environment Variables (Context Data)

- `FILE_PATH` - Transient, set by Claude Code
- `CHANGE_TYPE` - Transient, set by Claude Code
- `TOOL_NAME` - Transient, set by Claude Code
- `TOOL_ARGS` - Transient, set by Claude Code

---

## Decision Tree

```
Is this a setting that should persist across invocations?
├─ YES → Use Flag File (.claude/.feature-enabled)
│   └─ Examples: Auto-switch, skip hooks, debug mode
│
└─ NO → Is it per-invocation context data?
    ├─ YES → Use Environment Variable ($env:VAR)
    │   └─ Examples: FILE_PATH, TOOL_NAME
    │
    └─ NO → Is it complex structured data?
        └─ YES → Use JSON File (.claude/config.json)
            └─ Examples: Mode definitions, analytics
```

---

## Commands for Managing Flag Files

### Manual Enable/Disable

```powershell
# Enable auto-switching
New-Item -Path ".claude/.auto-switch-enabled" -ItemType File -Force

# Disable auto-switching
Remove-Item ".claude/.auto-switch-enabled" -Force

# Check status
if (Test-Path ".claude/.auto-switch-enabled") { "ENABLED" } else { "DISABLED" }
```

### List All Flag Files

```powershell
# PowerShell
Get-ChildItem -Path ".claude" -Filter ".*-enabled" -File

# Bash
ls -la .claude/.*-enabled
```

---

## User-Facing Commands

Instead of asking users to manually create flag files, provide commands:

```bash
# Good: User-friendly command
/mode auto enable

# Bad: Manual flag file manipulation
New-Item -Path ".claude/.auto-switch-enabled"
```

**Implementation**: Slash commands should handle flag file creation/deletion internally.

---

## Testing

When testing hooks with feature toggles:

```powershell
# Test with feature enabled
New-Item -Path ".claude/.feature-enabled" -ItemType File -Force
& ".claude/hooks/hook-script.ps1"

# Test with feature disabled
Remove-Item ".claude/.feature-enabled" -Force -ErrorAction SilentlyContinue
& ".claude/hooks/hook-script.ps1"

# Cleanup
Remove-Item ".claude/.feature-enabled" -Force -ErrorAction SilentlyContinue
```

---

## Summary

| Pattern | Use Case | Persistence | Example |
|---------|----------|-------------|---------|
| **Flag File** | Feature toggles | ✅ Persistent | `.claude/.auto-switch-enabled` |
| **Environment Variable** | Context data | ❌ Transient | `$env:FILE_PATH` |
| **JSON File** | Structured config | ✅ Persistent | `.claude/settings.json` |

**Golden Rule**: If the user expects it to "stick" across sessions → **Use a flag file**.

---

## Migration Priority

**High Priority** (affects user experience):
1. ✅ Auto-switching (DONE)
2. Agent reminders (consolidate two vars)
3. Hook skipping (diagnostics, validation, etc.)

**Low Priority** (internal/testing):
- Test script environment variables (fine as-is)

---

**Last Updated**: 2025-11-19
**Related Files**:
- `.claude/commands/mode.md` - Auto-switch commands
- `.claude/hooks/auto-context.ps1` - Auto-switch implementation
- `.claude/scripts/context-tracker.ps1` - Status tracking