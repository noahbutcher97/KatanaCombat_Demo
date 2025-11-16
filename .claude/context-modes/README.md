# Context Modes System

**Purpose**: Reduce token usage and improve focus by filtering codebase context to relevant domains.

---

## Quick Start

### Switch Context Manually
```
/mode animation          # Focus on animation system
/mode combat-logic       # Focus on core combat
/mode testing           # Focus on tests
/mode full              # Remove filtering (default)
```

### Auto-Detection
When you open a file, context is **automatically detected**:
- Open `AnimNotify_*.cpp` → Animation context activated
- Open `CombatComponent.cpp` → Combat-logic context activated
- Open `*Test.cpp` → Testing context activated

---

## Available Modes

| Mode | Focus Area | Use When |
|------|-----------|----------|
| **animation** | Montages, notifies, phase system | Working on animation integration, timing, notifies |
| **combat-logic** | State machines, input, components | Implementing combat mechanics, state transitions |
| **data-assets** | AttackData, configs, settings | Creating/modifying attack assets, tuning values |
| **editor-ui** | Custom panels, asset editors, Slate UI | Building editor tools, customizations |
| **testing** | Unit tests, integration tests | Writing tests, debugging test failures |
| **documentation** | Docs, guides, references | Updating documentation, writing guides |
| **full** | No filtering (default) | Cross-cutting changes, exploration, architecture work |

---

## How It Works

### 1. Include/Exclude Patterns
Each mode defines file patterns to focus on:

**Example (`combat-logic.json`)**:
```json
{
  "includePatterns": [
    "**/Core/CombatComponent*.{h,cpp}",
    "**/CombatTypes.h",
    "docs/SYSTEM_PROMPT.md"
  ],
  "excludePatterns": [
    "**/Animation/**",
    "**/Editor/**"
  ]
}
```

### 2. Relevant Documentation
Modes link to relevant docs with specific sections:

```json
{
  "relevantDocs": [
    "docs/SYSTEM_PROMPT.md - Core design principles",
    "docs/ARCHITECTURE.md:1-500 - Component architecture"
  ]
}
```

### 3. Context-Specific Guidance
Modes provide key principles and common tasks:

```json
{
  "keyPrinciples": [
    "Phases exclusive (Windup→Active→Recovery)",
    "Input ALWAYS buffered"
  ],
  "commonTasks": [
    "Implement new combat state",
    "Add input handling"
  ]
}
```

---

## Benefits

### Token Savings
**Before** (full context):
- 150+ files loaded
- ~80K tokens
- Slower responses

**After** (combat-logic mode):
- ~30 files loaded
- ~25K tokens
- 3x faster responses

### Improved Focus
- Relevant docs surfaced automatically
- Key principles reminded on file open
- Less distraction from unrelated code

### Better Suggestions
Claude prioritizes patterns from the active context, leading to more accurate suggestions.

---

## Workflow Examples

### Example 1: Adding New Attack
```bash
/mode data-assets                    # Switch to data context
# Work on AttackData.h
# Auto-reminder: "Three-tier architecture, validate in AttackData::Validate()"

/mode animation                      # Switch to animation
# Set up montage notifies
# Auto-reminder: "Use AnimNotify_AttackPhaseTransition, not deprecated AnimNotifyState_AttackPhase"

/mode testing                        # Switch to testing
# Write tests for new attack
```

### Example 2: Debugging Combat Bug
```bash
/mode combat-logic                   # Focus on combat system
# Investigate CombatComponentV2.cpp
# Relevant docs: SYSTEM_PROMPT.md, ARCHITECTURE.md

/mode animation                      # Suspect timing issue
# Check phase transition notifies

/mode full                          # Need to trace across systems
# Follow execution path through multiple domains
```

### Example 3: Building Editor Tool
```bash
/mode editor-ui                      # Switch to editor context
# Build AttackData asset editor
# Auto-reminder: "IDetailCustomization for property panels"

/mode data-assets                   # Need AttackData API
# Reference AttackData.h structure

/mode editor-ui                     # Back to editor
# Complete implementation
```

---

## Auto-Context Detection Rules

Triggered by file path patterns:

| File Pattern | Detected Mode | Auto-Reminders |
|--------------|---------------|----------------|
| `AnimNotify*`, `Animation/` | animation | Phase notify requirements, timing rules |
| `CombatComponent*`, `ActionQueue*` | combat-logic | Design principles, input buffering |
| `AttackData*`, `Data/` | data-assets | Three-tier architecture, validation |
| `Editor/`, `*Customization*` | editor-ui | Slate APIs, editor patterns |
| `*Test.cpp`, `Tests/` | testing | Test frameworks, existing test structure |
| `docs/*.md`, `README.md` | documentation | Doc standards, formatting rules |

---

## Configuration

### Mode Files
Located in `.claude/context-modes/*.json`

**Structure**:
```json
{
  "name": "mode-name",
  "description": "What this mode focuses on",
  "includePatterns": ["**/*.{h,cpp}"],
  "excludePatterns": ["**/Tests/**"],
  "relevantDocs": ["docs/GUIDE.md - Section description"],
  "commonTasks": ["Task 1", "Task 2"],
  "customMetadata": {
    "any additional context": "value"
  }
}
```

### Hooks
- **Auto-detection**: `.claude/hooks/auto-context.ps1`
  - Runs on file open
  - Provides contextual reminders

- **Manual switching**: `/mode` command
  - Full context switch
  - Shows mode details and file preview

### Override
Disable auto-context temporarily:
```powershell
$env:CLAUDE_DISABLE_AUTO_CONTEXT = "1"
```

---

## Tips

1. **Start specific, go broad**: Begin in focused mode, switch to `full` only when needed
2. **Use auto-detection**: Let the system switch context as you navigate files
3. **Check mode info**: Run `/mode [name]` to see what's included
4. **Create custom modes**: Copy existing `.json` and modify patterns
5. **Combine with agents**: Modes + agents = focused, efficient automation

---

## Troubleshooting

**Context not switching?**
- Check hook is enabled in `.claude/config.json`
- Verify PowerShell execution policy: `Get-ExecutionPolicy`
- Test hook manually: `powershell .claude/hooks/auto-context.ps1`

**Too restrictive?**
- Use `/mode full` for unrestricted access
- Adjust `includePatterns` in mode config

**Missing files?**
- Check if patterns match your file structure
- Use `**/*.ext` for recursive matching

---

## Performance Impact

| Context Size | Load Time | Response Time |
|--------------|-----------|---------------|
| Full (~150 files) | ~5s | ~8-12s |
| Focused (~30 files) | ~1s | ~3-5s |
| Minimal (~10 files) | ~0.5s | ~2-3s |

**Recommendation**: Stay in focused modes 80% of the time, use `full` for exploration/refactoring.

---

## Future Enhancements

- [ ] Track context switches in session
- [ ] Analytics on most-used modes
- [ ] Smart mode suggestions based on task description
- [ ] Context mode combos (animation + testing)
- [ ] IDE integration for visual mode indicator