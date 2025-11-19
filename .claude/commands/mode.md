# Context Mode Switcher

You are managing the context mode system for the KatanaCombat project, including mode switching and intelligent auto-context controls.

## Available Modes

Read all mode configs from `.claude/context-modes/` to show current options:

1. **animation** - Animation system (montages, notifies, phase transitions)
2. **combat-logic** - Core combat (state machines, input, components)
3. **data-assets** - Data-driven config (AttackData, settings)
4. **editor-ui** - Editor tooling (custom panels, asset editors, UI)
5. **testing** - Test infrastructure (unit tests, integration tests)
6. **documentation** - Documentation work (writing, updating docs)
7. **full** - Full context (no filtering, default)

---

## Commands

### `/mode [name]` - Switch to Specific Mode

**Requested Mode**: {{ARGS}}

1. **Load Mode Config**: Read `.claude/context-modes/{{ARGS}}.json`
2. **Validate Mode**: Ensure the mode file exists, otherwise list available modes
3. **Record Switch**: Call context tracker to record the switch:
   ```bash
   powershell.exe -ExecutionPolicy Bypass -File .claude/scripts/context-tracker.ps1 -Action switch -Mode "{{ARGS}}" -Reason "Manual switch via /mode command"
   ```
4. **Display Mode Info**:
   ```
   📍 Context Mode: [NAME]
   📝 Description: [description]

   📂 Focused Files:
   [List includePatterns]

   🚫 Excluded:
   [List excludePatterns]

   📚 Relevant Docs:
   [List relevantDocs with descriptions]

   ⚡ Common Tasks:
   [List commonTasks]

   [Display any mode-specific metadata: keyPrinciples, dataStructure, etc.]
   ```

5. **Context Reminder**: Add a system reminder about current mode:
   ```
   <system-reminder>
   ACTIVE CONTEXT MODE: [NAME]
   Focus on files matching: [includePatterns]
   When using Glob/Grep tools, prefer patterns within this context.
   Priority docs: [relevantDocs]

   To switch modes: /mode [mode-name]
   To see all modes: /mode list
   </system-reminder>
   ```

6. **Glob Validation** (optional): Show a preview of files that match includePatterns (first 20 files)

---

### `/mode list` - Show All Available Modes

Show all available modes with descriptions.

---

### `/mode current` - Show Currently Active Mode

Show currently active mode (if tracked).

---

### `/mode status` - Context System Status

Show detailed status of the context mode system and auto-switching.

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
3. **Combine with Conversation Analysis**: Add topic detection and complexity analysis (see implementation below)

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

**Enable**: Set CLAUDE_AUTO_SWITCH_CONTEXT=1
**Disable**: Set CLAUDE_AUTO_SWITCH_CONTEXT=0

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

**Keywords Detected**:
- animation: [count]
- combat: [count]
- test: [count]
- [etc.]

## Complexity Analysis

**Current Task**: [inferred from conversation]
**Complexity**: [Low/Medium/High]
**Affected Systems**: [list]

**Recommended Mode**: [mode] ([confidence]%)

## Switch History (Last 5)
1. [timestamp]: full → combat-logic (File open: CombatComponent.h)
2. [timestamp]: combat-logic → animation (Topic shift: montage discussion)
3. [etc.]

## Files Recently Accessed
- CombatComponent.h (combat-logic)
- AnimNotify_Phase.cpp (animation)
- [etc.]

---

💡 **Suggestion**: [Based on analysis, suggest mode if different from current]

## Manual Switch
Use: `/mode [mode-name]` to manually switch context
```

**Implementation**:
- Check for `.claude/.auto-switch-enabled` flag file to determine auto-switch status:
  ```bash
  powershell.exe -Command "if (Test-Path '.claude/.auto-switch-enabled') { Write-Host 'ENABLED' } else { Write-Host 'DISABLED' }"
  ```
- Analyze last 10 messages for topic detection using keyword matching
- Use `.claude/scripts/context-tracker.ps1 -Action status` and `-Action analytics` for switch history and statistics

---

### `/mode suggest` - Get Context Recommendation

Analyze conversation and suggest optimal mode using holistic multi-factor analysis.

**Action**: Perform conversation analysis + file detection + learned patterns to recommend mode.

**Implementation Steps**:
1. **Extract conversation text**: Get last 10 messages from conversation (Claude has access to this)
2. **Call holistic detector**: Pass conversation text to holistic-mode-detector.ps1
3. **Record successful switch**: If user accepts suggestion, record pattern to learning tracker
4. **Display results**: Show confidence breakdown and recommendation

**Output**:
```markdown
# 💡 Context Mode Suggestion

## Analysis

**Conversation Topics (Last 10 messages)**:
- [Topic 1]: [X] mentions ([Y]%)
- [Topic 2]: [X] mentions ([Y]%)

**Recent Files**:
- [file1] → [suggested mode]
- [file2] → [suggested mode]

**Task Complexity**: [Low/Medium/High]

**Current Task**: [inferred description]

## Recommendation

**Suggested Mode**: [mode-name]
**Confidence**: [X]%
**Reason**: [explanation]

**Switch now?**
- Yes: `/mode [mode-name]`
- No: Stay in `[current-mode]`

---

**Alternative**: If task spans multiple domains, consider staying in `full` mode
```

**Topic Detection** (keyword matching):
```
animation: AnimNotify, montage, animation, phase, notify, blend
combat-logic: combat, attack, combo, input, state, transition
data-assets: AttackData, CombatSettings, property, asset, data
editor-ui: editor, Slate, customization, panel, UI, widget
testing: test, spec, automation, assert, mock, fixture
documentation: docs, documentation, README, guide, tutorial
```

**Complexity Signals**:
- Low: "add property", "fix typo", "update value"
- Medium: "implement feature", "add system", "refactor"
- High: "multi-system", "architecture", "pipeline", "major"

**Trend Detection**:
- Count topic keywords in last N messages
- Calculate percentage distribution
- If one topic >60% → High confidence
- If one topic 40-60% → Medium confidence
- If mixed <40% → Stay in full

---

### `/mode auto enable` - Enable Auto-Switching

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

✅ **Auto-switching has been ENABLED** (for this session)

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

## Session vs Permanent

**Current Status**: ✅ Enabled for this session only

**For Permanent Enablement** (survives terminal restarts):
Add this to your PowerShell profile (`$PROFILE`):
```powershell
$env:CLAUDE_AUTO_SWITCH_CONTEXT = "1"
```

**To Disable**: Use `/mode auto disable`

📖 **Full Documentation**: See `.claude/context-modes/INTELLIGENT_SWITCHING.md`
```

---

### `/mode auto disable` - Disable Auto-Switching

**Action**: Actually disable intelligent auto-switching by removing the flag file.

**Implementation Steps**:
1. Remove the auto-switch flag file:
   ```bash
   powershell.exe -Command "if (Test-Path '.claude/.auto-switch-enabled') { Remove-Item '.claude/.auto-switch-enabled' -Force }; Write-Host 'Auto-switching DISABLED'"
   ```
2. Verify the setting:
   ```bash
   powershell.exe -Command "if (Test-Path '.claude/.auto-switch-enabled') { Write-Host 'ENABLED' } else { Write-Host 'DISABLED' }"
   ```

**Note**: This uses a flag file approach instead of environment variables because environment variables don't persist across tool invocations in Claude Code.

**Output**:
```markdown
# 🔧 Disable Auto-Context Switching

✅ **Auto-switching has been DISABLED** (for this session)

## Manual Mode Active

Context will now stay fixed until you manually switch with:
- `/mode [mode-name]` - Switch to specific mode
- `/mode full` - Switch to unrestricted mode

Auto-detection reminders will still show, but won't auto-switch.

## Session vs Permanent

**Current Status**: ✅ Disabled for this session only

**For Permanent Disablement** (survives terminal restarts):
Edit your PowerShell profile (`$PROFILE`) and remove or comment out:
```powershell
# $env:CLAUDE_AUTO_SWITCH_CONTEXT = "1"
```

**To Re-Enable**: Use `/mode auto enable`
```

---

## Context Switch Decision Logic

**When auto-switching is enabled**:
```
confidence = calculate_confidence(file, topic, complexity, trend)

IF confidence >= 0.8:  # High
    switch_immediately(suggested_mode)
    notify_user("Switched to [mode] (Reason: [trigger])")

ELIF confidence >= 0.5:  # Medium
    ask_user("Switch to [mode]? (Confidence: [X]%)")

ELSE:  # Low
    show_hint("Detected [topic] focus. Consider /mode [mode]")
```

---

## Notes for Claude

### After Mode Switch:
- Prioritize reading files from includePatterns
- When using Glob tool, filter results by includePatterns
- Suggest relevant docs from mode config when answering questions
- Remind user if they request files outside current context (suggest mode switch)

### For Auto-Context Analysis:
- Track context switches in conversation (if possible)
- Analyze last 10 messages for topic detection
- Be transparent about confidence levels
- Don't over-switch (wait for clear signals)
- Respect user's manual switches (don't override immediately)

### Implementation Note

This is a "soft" context switch - it doesn't technically restrict file access, but provides strong guidance on what to focus on. Think of it as a lens through which to view the codebase.

### Related Files
- `.claude/hooks/intelligent-context.ps1` - Multi-factor context analysis hook
- `.claude/scripts/context-tracker.ps1` - Context switch tracking and analytics
- `.claude/context-modes/INTELLIGENT_SWITCHING.md` - Full documentation