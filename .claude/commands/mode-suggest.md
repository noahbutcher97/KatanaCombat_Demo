# Mode Suggest - Get Context Recommendation

This is a shortcut for `/mode suggest`.

Analyze conversation and suggest optimal mode using holistic multi-factor analysis.

**Action**: Perform conversation analysis + file detection + learned patterns to recommend mode.

**Implementation Steps**:
1. **Extract conversation text**: Get last 10 messages from conversation
2. **Call holistic detector**: Pass conversation text to holistic-mode-detector.ps1
3. **Analyze recent files**: Check which files were recently accessed
4. **Calculate confidence**: Use multi-factor scoring

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

---

## Quick Actions

- `/mode [suggested-mode]` - Switch to suggested mode
- `/mode-status` - View current mode status
- `/mode-list` - See all available modes
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