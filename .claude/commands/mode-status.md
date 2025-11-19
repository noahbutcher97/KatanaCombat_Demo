# Mode Status - Context System Status

This is a shortcut for `/mode status`.

Shows detailed status of the context mode system and auto-switching.

**Action**: Display comprehensive status including auto-switch state and recent activity.

**Implementation Steps**:
1. **Call Context Tracker**: Execute context-tracker.ps1 to get current state and history:
   ```bash
   powershell.exe -ExecutionPolicy Bypass -File .claude/scripts/context-tracker.ps1 -Action status
   ```
2. **Get Analytics**: Get detailed analytics from tracker:
   ```bash
   powershell.exe -ExecutionPolicy Bypass -File .claude/scripts/context-tracker.ps1 -Action analytics
   ```
3. **Check auto-switch status**:
   ```bash
   powershell.exe -Command "if (Test-Path '.claude/.auto-switch-enabled') { Write-Host 'ENABLED' } else { Write-Host 'DISABLED' }"
   ```

**Output**:
```markdown
# 🎯 Context System Status

**Current Context Mode**: [mode name]
**Auto-Switching**: [ENABLED / DISABLED]

## Mode Details
- **Description**: [mode description]
- **Focused Files**: [count] files matching patterns
- **Active Since**: [timestamp if tracked]

## Auto-Switch Settings
- **Status**: [Enabled/Disabled]
- **Trigger Factors**:
  - 📁 File opens: [Yes/No]
  - 💬 Conversation topic: [Yes/No]
  - 📈 Task complexity: [Yes/No]
  - 🔄 Conversation trends: [Yes/No]

**Enable**: `/mode auto enable` or `/mode-auto-enable`
**Disable**: `/mode auto disable` or `/mode-auto-disable`

## Recent Context Switches (if any)
- [timestamp]: [old mode] → [new mode] (Reason: [trigger])

## Conversation Analysis (Last 10 Messages)

**Topic Distribution**:
- Animation: [X]%
- Combat Logic: [X]%
- Data Assets: [X]%
- Testing: [X]%
- Mixed/Other: [X]%

**Dominant Topic**: [topic] ([confidence]%)

## Recommended Mode
**Suggested Mode**: [mode] ([confidence]%)
**Reason**: [explanation]

---

💡 **Quick Actions**:
- `/mode [name]` - Switch to specific mode
- `/mode-list` - Show all modes
- `/mode-suggest` - Get AI recommendation
- `/mode-auto-enable` - Enable auto-switching
```