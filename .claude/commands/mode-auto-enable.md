# Mode Auto Enable - Enable Auto-Switching

This is a shortcut for `/mode auto enable`.

Enable intelligent auto-switching of context modes.

**Action**: Actually enable intelligent auto-switching by creating a flag file.

**Implementation Steps**:
1. Create the auto-switch flag file:
   ```bash
   powershell.exe -Command "New-Item -Path '.claude/.auto-switch-enabled' -ItemType File -Force | Out-Null; Write-Host 'Auto-switching ENABLED'"
   ```
2. Verify the setting:
   ```bash
   powershell.exe -Command "if (Test-Path '.claude/.auto-switch-enabled') { Write-Host 'ENABLED' } else { Write-Host 'DISABLED' }"
   ```

**Note**: This uses a flag file approach instead of environment variables because environment variables don't persist across tool invocations in Claude Code.

**Output**:
```markdown
# 🔧 Enable Auto-Context Switching

✅ **Auto-switching has been ENABLED**

## What This Does

Context will now automatically switch based on:

### 1. **File Opens** (High Confidence - 90%)
- Open `AnimNotify_*.cpp` → Switch to `animation`
- Open `CombatComponent.cpp` → Switch to `combat-logic`
- Open `*Test.cpp` → Switch to `testing`

### 2. **Conversation Topic** (Medium Confidence - 40-80%)
- Keywords: "animation", "montage", "notify" → `animation`
- Keywords: "combat", "attack", "combo" → `combat-logic`
- Keywords: "test", "automation", "spec" → `testing`

### 3. **Task Complexity** (Influences Mode)
- Simple property addition → Stay in current mode
- Multi-system feature → Switch to relevant primary mode
- Bug diagnosis → Switch to affected system mode

### 4. **Conversation Trends** (Pattern Analysis)
- 3+ messages about animation → Suggest `animation` mode
- Mixed topics → Stay in `full` mode
- Shifting focus → Switch when clear majority emerges

## Confidence Thresholds
- **High (≥80%)**: Auto-switch immediately
- **Medium (50-79%)**: Show suggestion, ask confirmation
- **Low (<50%)**: Show hint, no auto-switch

## Quick Actions

- **Disable**: `/mode auto disable` or `/mode-auto-disable`
- **Check status**: `/mode status` or `/mode-status`
- **Manual switch**: `/mode [name]`

📖 **Full Documentation**: See `.claude/context-modes/INTELLIGENT_SWITCHING.md`
```