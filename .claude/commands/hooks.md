# Hooks Manager - Manage Development Hooks

Manage development hooks (on-save checks, pre-commit validation, etc.) using flag files.

This command provides a unified interface for enabling/disabling various development hooks.

---

## Available Hooks

### Quick Style Check (on-save)
**Trigger**: When files are saved
**Checks**:
- Include guards
- UPROPERTY without Category
- BlueprintCallable without DisplayName/Tooltip
- TODO/FIXME/HACK comments
- Large functions
- Tick usage (anti-pattern)
- Delegate declarations outside CombatTypes.h

**Flag File**: `.claude/.skip-on-save-enabled`

### Pre-Commit Diagnostics
**Trigger**: Before git commit
**Checks**:
- IDE warnings and errors
- Critical issues in staged files
- Component-specific validation

**Flag File**: `.claude/.skip-diagnostics-enabled`

### Pre-Commit Validation
**Trigger**: Before git commit
**Checks**:
- Critical file modifications
- Component architecture compliance
- AnimNotify phase system
- Test file coverage
- Documentation date updates
- Large commit warnings
- Mixed concern detection

**Flag File**: `.claude/.skip-validation-enabled`

### After-Edit Reminders
**Trigger**: After file edits
**Checks**:
- Documentation update requirements
- Architecture doc alignment
- API reference updates

**Flag File**: `.claude/.skip-after-edit-enabled`

### Agent Delegation Reminders
**Trigger**: During complex operations
**Checks**:
- Multi-file edits
- Component/system files
- New file creation
- Large edits

**Flag File**: `.claude/.skip-agent-reminders-enabled`

---

## Commands

### `/hooks status` - Show Hook Status

Show current enable/disable status of all hooks.

**Implementation**:
```bash
powershell.exe -Command @"
Write-Host '# 🔧 Development Hooks Status'
Write-Host ''
Write-Host '## Hook States'
Write-Host ''
$hooks = @(
    @{Name='on-save'; Flag='.claude/.skip-on-save-enabled'; Description='Quick style check on file save'},
    @{Name='diagnostics'; Flag='.claude/.skip-diagnostics-enabled'; Description='Pre-commit IDE diagnostics'},
    @{Name='validation'; Flag='.claude/.skip-validation-enabled'; Description='Pre-commit validation enforcer'},
    @{Name='after-edit'; Flag='.claude/.skip-after-edit-enabled'; Description='Documentation update reminders'},
    @{Name='agent-reminders'; Flag='.claude/.skip-agent-reminders-enabled'; Description='Agent delegation suggestions'}
)

foreach (`$hook in `$hooks) {
    `$status = if (Test-Path `$hook.Flag) { '🔴 DISABLED' } else { '✅ ENABLED' }
    Write-Host `"  **`$(`$hook.Name)**: `$status`"
    Write-Host `"     Description: `$(`$hook.Description)`"
    Write-Host `"     Toggle: /hooks skip `$(`$hook.Name) | /hooks enable `$(`$hook.Name)`"
    Write-Host ''
}

Write-Host ''
Write-Host '## Quick Actions'
Write-Host '  - Disable hook: `/hooks skip [hook-name]`'
Write-Host '  - Enable hook: `/hooks enable [hook-name]`'
Write-Host '  - Disable all: `/hooks skip all`'
Write-Host '  - Enable all: `/hooks enable all`'
"@
```

---

### `/hooks skip [hook-name]` - Disable a Hook

Disable a specific development hook by creating its flag file.

**Requested Hook**: {{ARGS}}

**Valid Hook Names**:
- `on-save` - Quick style check on file save
- `diagnostics` - Pre-commit IDE diagnostics
- `validation` - Pre-commit validation enforcer
- `after-edit` - Documentation update reminders
- `agent-reminders` - Agent delegation suggestions
- `all` - Disable all hooks

**Implementation**:

For specific hook (e.g., `/hooks skip on-save`):
```bash
powershell.exe -Command @"
`$hookName = '{{ARGS}}'
`$flagFiles = @{
    'on-save' = '.claude/.skip-on-save-enabled'
    'diagnostics' = '.claude/.skip-diagnostics-enabled'
    'validation' = '.claude/.skip-validation-enabled'
    'after-edit' = '.claude/.skip-after-edit-enabled'
    'agent-reminders' = '.claude/.skip-agent-reminders-enabled'
}

if (`$hookName -eq 'all') {
    foreach (`$flag in `$flagFiles.Values) {
        New-Item -Path `$flag -ItemType File -Force | Out-Null
    }
    Write-Host '🔴 All hooks DISABLED'
} elseif (`$flagFiles.ContainsKey(`$hookName)) {
    New-Item -Path `$flagFiles[`$hookName] -ItemType File -Force | Out-Null
    Write-Host `"🔴 Hook '`$hookName' DISABLED`"
} else {
    Write-Host `"❌ Unknown hook: `$hookName`"
    Write-Host 'Valid hooks: on-save, diagnostics, validation, after-edit, agent-reminders, all'
}
"@
```

**Output**:
```markdown
# 🔧 Hook Disabled

✅ **Hook '{{ARGS}}' has been DISABLED**

This hook will no longer trigger until re-enabled.

## Re-enable Hook
Use: `/hooks enable {{ARGS}}`

## View All Hooks
Use: `/hooks status`
```

---

### `/hooks enable [hook-name]` - Enable a Hook

Enable a specific development hook by removing its flag file.

**Requested Hook**: {{ARGS}}

**Valid Hook Names**:
- `on-save` - Quick style check on file save
- `diagnostics` - Pre-commit IDE diagnostics
- `validation` - Pre-commit validation enforcer
- `after-edit` - Documentation update reminders
- `agent-reminders` - Agent delegation suggestions
- `all` - Enable all hooks

**Implementation**:

For specific hook (e.g., `/hooks enable on-save`):
```bash
powershell.exe -Command @"
`$hookName = '{{ARGS}}'
`$flagFiles = @{
    'on-save' = '.claude/.skip-on-save-enabled'
    'diagnostics' = '.claude/.skip-diagnostics-enabled'
    'validation' = '.claude/.skip-validation-enabled'
    'after-edit' = '.claude/.skip-after-edit-enabled'
    'agent-reminders' = '.claude/.skip-agent-reminders-enabled'
}

if (`$hookName -eq 'all') {
    foreach (`$flag in `$flagFiles.Values) {
        if (Test-Path `$flag) {
            Remove-Item `$flag -Force
        }
    }
    Write-Host '✅ All hooks ENABLED'
} elseif (`$flagFiles.ContainsKey(`$hookName)) {
    if (Test-Path `$flagFiles[`$hookName]) {
        Remove-Item `$flagFiles[`$hookName] -Force
    }
    Write-Host `"✅ Hook '`$hookName' ENABLED`"
} else {
    Write-Host `"❌ Unknown hook: `$hookName`"
    Write-Host 'Valid hooks: on-save, diagnostics, validation, after-edit, agent-reminders, all'
}
"@
```

**Output**:
```markdown
# 🔧 Hook Enabled

✅ **Hook '{{ARGS}}' has been ENABLED**

This hook will now trigger according to its configuration.

## Disable Hook
Use: `/hooks skip {{ARGS}}`

## View All Hooks
Use: `/hooks status`
```

---

## Usage Examples

### Example 1: Disable On-Save Checks
```bash
/hooks skip on-save
```
Result: No more style checks when saving files

### Example 2: Disable All Pre-Commit Checks
```bash
/hooks skip diagnostics
/hooks skip validation
```
Or:
```bash
# (Not implemented yet, but could be)
/hooks skip pre-commit  # Disables both diagnostics and validation
```

### Example 3: Temporarily Disable Agent Suggestions
```bash
/hooks skip agent-reminders
# Do your work without agent delegation suggestions
/hooks enable agent-reminders
```

### Example 4: Check Current Status
```bash
/hooks status
```

### Example 5: Disable Everything During Experimentation
```bash
/hooks skip all
# Experiment freely
/hooks enable all
```

---

## Hook Details

### On-Save Hook
**File**: `.claude/hooks/on-file-save.ps1`
**Trigger**: File save events
**Purpose**: Immediate feedback on style issues
**Recommendation**: Keep enabled for best development experience

### Pre-Commit Diagnostics
**File**: `.claude/hooks/pre-commit-diagnostics.ps1`
**Trigger**: Before git commit
**Purpose**: Catch IDE warnings before commit
**Recommendation**: Disable only for urgent hotfixes

### Pre-Commit Validation
**File**: `.claude/hooks/before-commit.ps1`
**Trigger**: Before git commit
**Purpose**: Comprehensive validation before commit
**Recommendation**: Disable only when confident in changes

### After-Edit Reminders
**File**: `.claude/hooks/after-edit.ps1`
**Trigger**: After file edits
**Purpose**: Remind about documentation updates
**Recommendation**: Keep enabled to maintain doc sync

### Agent Delegation Reminders
**File**: `.claude/hooks/agent-reminder.ps1`
**Trigger**: Complex operations
**Purpose**: Suggest using specialized agents
**Recommendation**: Disable if you prefer manual control

---

## Notes

- All hooks use flag file pattern (`.claude/.skip-[name]-enabled`)
- Flag file exists = Hook DISABLED
- No flag file = Hook ENABLED (default)
- Changes take effect immediately
- No restart required
- All hooks are optional - project works without them

## Related Documentation

- `.claude/CONFIGURATION_BEST_PRACTICES.md` - Configuration patterns
- `.claude/hooks/README.md` - Hook system overview (if exists)