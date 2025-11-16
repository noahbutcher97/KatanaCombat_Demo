# Intelligent Context Switching - Usage Guide

**Status**: Production Ready (v1.2.0)
**Features**: Auto-detection, confidence scoring, history tracking, analytics

---

## Quick Start

### 1. Enable Auto-Switching (Recommended)

**PowerShell**:
```powershell
$env:CLAUDE_AUTO_SWITCH_CONTEXT = "1"
```

**Bash/Zsh**:
```bash
export CLAUDE_AUTO_SWITCH_CONTEXT=1
```

**Permanent (PowerShell Profile)**:
```powershell
# Add to your PowerShell profile ($PROFILE):
$env:CLAUDE_AUTO_SWITCH_CONTEXT = "1"
```

### 2. Use Manual Commands

- `/mode animation` - Switch to animation mode
- `/mode combat-logic` - Switch to combat logic mode
- `/mode data-assets` - Switch to data assets mode
- `/mode testing` - Switch to testing mode
- `/mode documentation` - Switch to documentation mode
- `/mode editor-ui` - Switch to editor UI mode
- `/mode full` - Switch to full context (no filtering)

### 3. Check Status

- `/mode status` - See current mode, history, and analytics
- `/mode list` - Show all available modes

---

## How Auto-Switching Works

### Confidence Levels

When you open a file, the system analyzes the path and assigns a confidence score:

| Confidence | Action | Example |
|------------|--------|---------|
| **High (≥80%)** | Auto-switch immediately | `AnimNotify_Phase.h` → animation (95%) |
| **Medium (≥50%)** | Auto-switch with notification | `CombatComponent.cpp` → combat-logic (75%) |
| **Low (<50%)** | Show hint only, no auto-switch | `Input.h` → combat-logic (50%) hint |

### File Detection Examples

**Animation Files** (90-95% confidence):
- `AnimNotify*.{h,cpp}` - Animation notifies
- `AnimNotifyState*.{h,cpp}` - Notify states
- `Animation/*.{h,cpp}` - Animation directory files
- `MontageUtility*` - Montage utilities

**Combat Logic Files** (75-95% confidence):
- `CombatComponent*.{h,cpp}` - Combat components
- `ActionQueue*.{h,cpp}` - Action queue system
- `TargetingComponent*.{h,cpp}` - Targeting system
- `Core/*.{h,cpp}` - Core combat files

**Data Assets Files** (85-95% confidence):
- `AttackData.{h,cpp}` - Attack data
- `AttackConfiguration.{h,cpp}` - Attack configs
- `CombatSettings.{h,cpp}` - Combat settings
- `Data/*.{h,cpp}` - Data directory files

**Testing Files** (90-95% confidence):
- `*Test.{h,cpp}` - Test files
- `*Spec.{h,cpp}` - Spec files
- `Tests/*` - Test directory

**Editor Files** (90-95% confidence):
- `Editor/*` - Editor directory
- `*AssetEditor.*` - Asset editors
- `*Customization.*` - UI customizations
- `*Factory.*` - Asset factories

**Documentation Files** (85-95% confidence):
- `*.md` - Markdown files
- `docs/*` - Documentation directory
- `README*` - Readme files

---

## Context History & Analytics

### View History

```powershell
powershell .claude/scripts/context-tracker.ps1 -Action status
```

**Shows**:
- Current mode
- Auto-switch status
- Recent switches (last 5)
- Total switch count
- Mode usage statistics

### View Analytics

```powershell
powershell .claude/scripts/context-tracker.ps1 -Action analytics
```

**Shows**:
- Mode usage distribution (%)
- Switch triggers (reasons)
- Average time between switches

### Export History

```powershell
powershell .claude/scripts/context-tracker.ps1 -Action export
```

Creates timestamped JSON export: `.claude/context-history-export-YYYY-MM-DD-HHMMSS.json`

### Clear History

```powershell
powershell .claude/scripts/context-tracker.ps1 -Action clear
```

Clears all history but keeps current mode (requires confirmation).

---

## Advanced Features

### Test Mode Detection

Test what mode a file would trigger:

```powershell
powershell .claude/scripts/detect-mode.ps1 -FilePath "path/to/file.cpp" -ShowDetails
```

**Output**:
```
=== Mode Detection Analysis ===
File: Source/KatanaCombat/Private/Core/CombatComponent.cpp

Suggested Mode: combat-logic
Confidence: 75% (medium)
Reason: Matched 2 pattern(s): CombatComponent(?!.*Test), Core/(?!.*Test)

All Mode Scores:
  combat-logic   : ############### 75%
```

### Test Auto-Context Hook

Test the hook with multiple files:

```powershell
powershell .claude/scripts/test-auto-context.ps1 -EnableAutoSwitch
```

Tests 6 representative files and shows final tracker state.

---

## Troubleshooting

### Auto-Switch Not Working

1. **Check environment variable**:
   ```powershell
   $env:CLAUDE_AUTO_SWITCH_CONTEXT
   ```
   Should output: `1`

2. **Check if hook is active**:
   - Auto-context hook must be configured in `.claude/config.json`
   - It's active by default in this project

3. **Test file detection**:
   ```powershell
   powershell .claude/scripts/detect-mode.ps1 -FilePath "your/file.cpp" -ShowDetails
   ```

### Switches Too Frequent

If auto-switching is too aggressive:

1. **Increase confidence threshold** (edit `.claude/hooks/auto-context.ps1`):
   ```powershell
   # Line 139-140
   $highThreshold = 0.90  # Was 0.80
   $mediumThreshold = 0.70  # Was 0.50
   ```

2. **Disable auto-switch**:
   ```powershell
   $env:CLAUDE_AUTO_SWITCH_CONTEXT = "0"
   ```

### Wrong Mode Detected

File patterns can be customized in `.claude/scripts/detect-mode.ps1`:

1. **Find the mode patterns** (lines 18-93)
2. **Adjust pattern weights** (0.0-1.0 scale)
3. **Add new patterns** for your use case

Example:
```powershell
'combat-logic' = @(
    @{ pattern = 'MyCustomComponent'; weight = 0.90 }  # Add this
    @{ pattern = 'CombatComponent(?!.*Test)'; weight = 0.95 }
    # ... rest of patterns
)
```

### Hook Not Running

1. **Check `.claude/config.json`**:
   ```json
   {
     "hooks": {
       "afterFileOpen": ".claude/hooks/auto-context.ps1"
     }
   }
   ```

2. **Check PowerShell execution policy**:
   ```powershell
   Get-ExecutionPolicy
   ```
   Should allow script execution.

---

## Best Practices

### When to Use Auto-Switch

**Recommended**:
- Working on focused tasks (animation tuning, combat balancing, test writing)
- Jumping between different subsystems frequently
- Want contextual reminders about relevant docs

**Not Recommended**:
- Debugging cross-system issues (use `full` mode)
- Refactoring that touches multiple domains
- Initial codebase exploration

### When to Use Manual Mode

Set `CLAUDE_AUTO_SWITCH_CONTEXT=0` and use `/mode [name]` when:
- You need complete control over context
- Working on architectural changes spanning multiple modes
- Prefer explicit mode switching over automatic

### Optimal Workflow

1. **Enable auto-switch** for daily work
2. **Use `/mode status`** periodically to see switch patterns
3. **Use `/mode full`** when working across domains
4. **Check analytics** to understand your workflow patterns

---

## Performance Notes

### Impact

- **File open latency**: <100ms additional (pattern matching + JSON parsing)
- **Hook execution**: Runs on file open only (not on every edit)
- **Memory usage**: Negligible (context history capped at 50 switches)

### Optimization

The system is optimized for production use:
- **Fail-safe**: Errors in hooks never break workflow (exit silently)
- **Case-insensitive**: Works on Windows paths
- **Regex safety**: Try-catch around all pattern matching
- **Input validation**: Empty/null paths handled gracefully

---

## Reference

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `CLAUDE_AUTO_SWITCH_CONTEXT` | `0` (off) | Enable/disable auto-switching |

### Files

| File | Purpose |
|------|---------|
| `.claude/scripts/context-tracker.ps1` | History tracking and analytics |
| `.claude/scripts/detect-mode.ps1` | File-to-mode detection |
| `.claude/hooks/auto-context.ps1` | Auto-switch hook (runs on file open) |
| `.claude/.context-history.json` | Persistent history (auto-created) |
| `.claude/context-modes/*.json` | Mode configurations |

### Commands

| Command | Description |
|---------|-------------|
| `/mode [name]` | Switch to specific mode |
| `/mode list` | Show all modes |
| `/mode status` | Show detailed status with history |
| `/mode auto enable` | Instructions to enable auto-switch |
| `/mode auto disable` | Instructions to disable auto-switch |

---

## Support

- **Documentation**: See `.claude/INDEX.md` for full infrastructure overview
- **Changelog**: See `.claude/CHANGELOG.md` for version history
- **Issues**: Check `.claude/hooks/README.md` for hook troubleshooting

**Version**: 1.2.0
**Last Updated**: 2025-11-15
