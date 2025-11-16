# Intelligent Auto-Context Switching

> ⚠️ **PLANNED FEATURE - PARTIALLY IMPLEMENTED**
>
> **Current Status**: File-based auto-context switching is ACTIVE via `auto-context.ps1` hook.
>
> **Planned Enhancements** (documented below, not yet implemented):
> - Multi-factor analysis (conversation topics, task complexity, trends)
> - Confidence-based auto-switching thresholds
> - Context switch analytics via `context-tracker.ps1`
> - Advanced commands: `/mode status` (analytics), `/mode suggest` (AI recommendations)
>
> **What Works Now**:
> - ✅ File-based context detection (e.g., open `AnimNotify*.cpp` → suggests `animation` mode)
> - ✅ Manual mode switching via `/mode [name]`
> - ✅ Environment variable `CLAUDE_AUTO_SWITCH_CONTEXT` (controls file-based switching)
>
> **Roadmap**: This document describes the vision for intelligent context switching. Implementation tracked in project backlog.

---

**New Feature**: Context modes now switch intelligently based on multiple factors beyond just file opens.

---

## Overview

The intelligent context system analyzes:
1. **📁 File Opens** (90% confidence) - Traditional file-based detection
2. **💬 Conversation Topics** (40-80% confidence) - Keyword analysis of recent messages
3. **📈 Task Complexity** (Influences mode) - Detects if task spans multiple systems
4. **🔄 Conversation Trends** (Pattern analysis) - Analyzes topic distribution over time

---

## How to Enable

###Opt-In Auto-Switching

**PowerShell**:
```powershell
$env:CLAUDE_AUTO_SWITCH_CONTEXT = "1"
```

**Bash/Zsh**:
```bash
export CLAUDE_AUTO_SWITCH_CONTEXT=1
```

**Permanent** (add to PowerShell profile):
```powershell
# Edit: notepad $PROFILE
$env:CLAUDE_AUTO_SWITCH_CONTEXT = "1"
```

### Disable Auto-Switching

```powershell
$env:CLAUDE_AUTO_SWITCH_CONTEXT = "0"
# or
Remove-Item Env:CLAUDE_AUTO_SWITCH_CONTEXT
```

---

## Commands

### `/mode status` - Show Current Status
Displays:
- Current mode
- Auto-switch status (enabled/disabled)
- Recent context switches
- Conversation analysis
- How to enable/disable

### `/mode auto enable` - Enable Instructions
Shows detailed instructions for enabling auto-switching

### `/mode auto disable` - Disable Instructions
Shows how to disable and return to manual mode

### `/mode suggest` - Get Recommendation
Analyzes current conversation and suggests optimal mode

### `/mode [name]` - Manual Switch
Manually switch to a specific context mode

---

## How It Works

### Multi-Factor Analysis

#### Factor 1: File Opens (High Confidence - 90%)
```
Open AnimNotify_Phase.cpp
→ Detected: animation context
→ Confidence: 90%
→ Trigger: File path matches animation patterns
```

**File Patterns**:
- `AnimNotify*`, `Animation/` → animation
- `CombatComponent*`, `ActionQueue*`, `Core/` → combat-logic
- `AttackData*`, `Data/` → data-assets
- `Editor/`, `*Customization*`, `Slate` → editor-ui
- `*Test.cpp`, `Tests/` → testing
- `docs/*.md`, `README` → documentation

---

#### Factor 2: Conversation Topics (Medium Confidence - 40-80%)

Analyzes recent messages for keywords:

**animation** keywords:
- AnimNotify, montage, animation, phase, notify, blend, transition

**combat-logic** keywords:
- combat, attack, combo, input, state, CombatComponent, ActionQueue

**data-assets** keywords:
- AttackData, CombatSettings, property, asset, UPROPERTY

**editor-ui** keywords:
- editor, Slate, customization, panel, UI, widget, AssetEditor

**testing** keywords:
- test, spec, automation, assert, mock, fixture, unit

**documentation** keywords:
- docs, documentation, README, guide, tutorial, markdown

**Example**:
```
Conversation: "Let's add a new AnimNotify for the combo window.
               We need to modify the montage timeline and blend settings."

Analysis:
- "AnimNotify": 1 match (animation)
- "combo": 1 match (combat-logic)
- "montage": 1 match (animation)
- "timeline": 1 match (animation)
- "blend": 1 match (animation)

Result: animation = 4, combat-logic = 1
Detected: animation (80% confidence)
```

---

#### Factor 3: Task Complexity (Influences Mode)

**High Complexity** indicators:
- multi-system, architecture, pipeline, major, refactor, overhaul

**Medium Complexity** indicators:
- implement, feature, add system, refactor, modify

**Low Complexity** indicators:
- add property, fix typo, update value, change, tweak

**Effect on Mode**:
- **High complexity** → Suggests staying in `full` mode (spans multiple systems)
- **Low/Medium complexity** → Uses detected specific mode

**Example**:
```
Task: "Implement dodge roll with i-frames, animation blending, and input handling"

Keywords: "implement" (medium), "i-frames" (animation), "blending" (animation), "input" (combat)
Complexity: HIGH (spans animation + combat + input systems)

Decision: Suggest 'full' mode instead of specific mode
```

---

#### Factor 4: Conversation Trends (Pattern Analysis)

Analyzes topic distribution over last 10 messages:

**High Confidence** (>60% one topic):
```
Messages 1-10:
- animation: 7 mentions (70%)
- combat-logic: 2 mentions (20%)
- other: 1 mention (10%)

Result: Strong animation trend
Action: Auto-switch to animation (confidence: 70%)
```

**Medium Confidence** (40-60% one topic):
```
Messages 1-10:
- combat-logic: 5 mentions (50%)
- data-assets: 3 mentions (30%)
- other: 2 mentions (20%)

Result: Moderate combat-logic trend
Action: Suggest combat-logic (confidence: 50%)
```

**Low Confidence** (<40% any topic):
```
Messages 1-10:
- animation: 3 mentions (30%)
- combat-logic: 3 mentions (30%)
- testing: 2 mentions (20%)
- other: 2 mentions (20%)

Result: Mixed topics
Action: Stay in 'full' mode
```

---

## Decision Logic

### Confidence Thresholds

```
IF auto_switch_enabled:
    confidence = calculate_confidence(file, topic, complexity, trend)

    IF confidence >= 80%:  # High
        ✅ AUTO-SWITCH immediately
        Notify: "Switched to [mode] (Reason: [trigger])"

    ELIF confidence >= 50%:  # Medium
        💡 ASK USER: "Switch to [mode]? (Confidence: [X]%)"
        Options: Yes (/mode [mode]) or No (stay current)

    ELSE:  # Low (<50%)
        ℹ️ SHOW HINT: "Detected [topic] focus. Consider /mode [mode]"
```

### Confidence Calculation

```
confidence = 0

IF file_detected:
    confidence = 90  # High confidence

IF conversation_topic_match > 60%:
    IF confidence > 0:
        # Topic might override file if strong enough
        IF topic_confidence > 70%:
            confidence = topic_confidence
    ELSE:
        confidence = topic_confidence

IF task_complexity == "high":
    confidence = max(confidence - 20, 50)  # Reduce confidence, suggest 'full'
```

---

## Examples

### Example 1: File Open (High Confidence)

**Scenario**:
```
User opens: Source/KatanaCombat/Public/Animation/AnimNotify_Phase.cpp
```

**Analysis**:
- File path matches: animation pattern
- Confidence: 90%
- No conversation context needed

**Result** (auto-switch enabled):
```
✅ AUTO-SWITCHING to 'animation' mode (High confidence)

Trigger: File - AnimNotify_Phase.cpp
Confidence: 90%

Context now focused on animation system files.
```

---

### Example 2: Conversation Topic (Medium Confidence)

**Scenario**:
```
User: "Let's work on the combo system. I need to modify how input buffering
       works during the combo window and fix the state transitions."
```

**Analysis**:
- Keywords detected:
  - "combo": 2 (combat-logic)
  - "input buffering": 1 (combat-logic)
  - "combo window": 1 (combat-logic)
  - "state transitions": 1 (combat-logic)
- Topic: combat-logic (100% of keywords)
- Confidence: 65%

**Result** (auto-switch enabled):
```
💡 SUGGESTION: Switch to 'combat-logic' mode? (Medium confidence)

Analysis:
  - 💬 Topic-based: combat-logic (65%)
  - Scores: combat-logic=5, animation=0, other=0

Options:
  - Yes: /mode combat-logic
  - No: Stay in current mode
```

---

### Example 3: Mixed Signals

**Scenario**:
```
Currently in: full mode
User opens: AttackData.cpp
Conversation: Discussing animation blending and montage setup
```

**Analysis**:
- File: data-assets (90% confidence)
- Topic: animation (70% confidence)
- Conflict detected

**Result** (auto-switch enabled):
```
💡 Mixed Context Detected

File suggests: data-assets (90%)
Conversation suggests: animation (70%)

Since conversation topic is strong and file is supporting (AttackData for animation),
switching to: animation (Topic override)

Confidence: 70%

Switch to 'animation' mode?
  - Yes: /mode animation
  - No: Stay in full
```

---

### Example 4: High Complexity Task

**Scenario**:
```
User: "Implement a complete dodge system with i-frame windows, animation blending,
       input handling, stamina cost, and UI feedback."
```

**Analysis**:
- Keywords: "implement" (medium complexity)
- Systems affected: animation, combat-logic, data-assets, editor-ui
- Complexity: HIGH (multi-system)
- Topic distribution: Mixed (no dominant)

**Result** (auto-switch enabled):
```
ℹ️ High Complexity Task Detected

Systems involved: animation, combat-logic, data-assets, editor-ui

Recommendation: Stay in 'full' mode
Reason: Task spans multiple systems - full context needed

Confidence: 50% (reduced due to complexity)

Current mode 'full' is appropriate for this task.
```

---

## State Tracking

Context switches are tracked in `.claude/.context-history.json`:

```json
{
  "currentMode": "animation",
  "autoSwitchEnabled": true,
  "history": [
    {
      "timestamp": "2025-11-13 14:30:00",
      "from": "full",
      "to": "animation",
      "reason": "File: AnimNotify_Phase.cpp"
    }
  ],
  "statistics": {
    "totalSwitches": 15,
    "modeUsage": {
      "animation": 5,
      "combat-logic": 7,
      "full": 3
    }
  }
}
```

### Analytics

**View switch history**:
```powershell
powershell .claude/scripts/context-tracker.ps1 -Action status
```

**View analytics**:
```powershell
powershell .claude/scripts/context-tracker.ps1 -Action analytics
```

**Output**:
```
📊 Context Analytics

Mode Usage Distribution:
  combat-logic  : ████████████████ 46.7% (7 switches)
  animation     : ██████████ 33.3% (5 switches)
  full          : ████ 20.0% (3 switches)

Switch Triggers:
  File: AnimNotify_Phase.cpp: 3
  Conversation topic: 5
  Manual switch: 2

Average Time Between Switches: 12.5 minutes
```

---

## Best Practices

### 1. **Start with Auto-Switch Enabled**
Let the system learn your workflow patterns

### 2. **Monitor Switches**
Use `/context status` to see if switches make sense

### 3. **Manual Override When Needed**
If auto-switch goes to wrong mode, use `/mode [correct-mode]`

### 4. **High Complexity = Full Mode**
For major features spanning systems, `full` mode is best

### 5. **Trust Medium Confidence Suggestions**
If system suggests with 50-70% confidence, usually correct

---

## Troubleshooting

### "Switches too frequently"
**Cause**: Jumping between different file types
**Fix**: Use `full` mode for exploration, or disable auto-switch

### "Doesn't switch when expected"
**Cause**: Confidence below threshold
**Fix**: Check `/context status` to see analysis. Use `/mode` manually if needed.

### "Wrong mode detected"
**Cause**: Keywords matched wrong topic
**Fix**: Manually switch with `/mode [correct-mode]`. System will learn from trends.

### "Stays in full mode"
**Cause**: Mixed topics or high complexity detected
**Fix**: This is intentional. Use `/mode` if you want specific mode.

---

## Performance Impact

| Feature | Overhead | When It Runs |
|---------|----------|--------------|
| File-based detection | ~10ms | On file open |
| Topic analysis | ~50ms | On message send (if enabled) |
| Trend analysis | ~100ms | Every 5 messages (if enabled) |
| Complexity detection | ~30ms | On task description (if provided) |

**Total impact**: Minimal (~50-100ms per interaction when enabled)

---

## Migration from Old System

**Old behavior** (always active):
- File open → Show reminder, manual `/mode` needed

**New behavior** (opt-in):
- File open → Auto-switch if confidence high
- Conversation → Auto-switch if topic clear
- Mixed signals → Ask for confirmation

**To keep old behavior**: Don't set `CLAUDE_AUTO_SWITCH_CONTEXT`

---

**Intelligent context switching makes your workflow seamless and adaptive!** 🎯
