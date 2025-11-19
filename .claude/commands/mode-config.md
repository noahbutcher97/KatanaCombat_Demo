# /mode config - Configure Intelligent Switching

Configure confidence thresholds, factor weights, and behavior settings for the intelligent mode switching system.

**Usage**:
```
/mode config
/mode config set [setting] [value]
/mode config preset [conservative|balanced|aggressive]
/mode config reset
```

**Examples**:
- `/mode config` - Show current configuration
- `/mode config set high 0.85` - Set high confidence threshold to 85%
- `/mode config set medium 0.55` - Set medium confidence threshold to 55%
- `/mode config preset aggressive` - Use aggressive preset (more auto-switching)
- `/mode config reset` - Reset to default (balanced)

---

## Current Configuration

**Confidence Thresholds**:
- **High**: 80% → Auto-switch immediately
- **Medium**: 50% → Auto-switch with notification
- **Low**: 30% → Show hint only

**Factor Weights** (how much each factor influences decision):
- File pattern: 35%
- Conversation topics: 25%
- Learned patterns: 20%
- Historical success: 15%
- Time-based: 5%

**Behavior**:
- Auto-switching: ✅ Enabled
- Confidence breakdown: ✅ Shown
- Negative feedback: ✅ Enabled (30s window)
- Passive learning: ✅ Enabled

---

## Available Presets

### 1. **Conservative** (fewer auto-switches)
```
High:   90% → Very confident before switching
Medium: 70% → Moderate confidence needed
Low:    50% → Higher threshold for hints
```
**Use when**: You want manual control, only switch on very clear signals

---

### 2. **Balanced** (default, recommended)
```
High:   80% → Confident auto-switching
Medium: 50% → Reasonable confidence needed
Low:    30% → Show hints for uncertain cases
```
**Use when**: You want smart automation with good accuracy

---

### 3. **Aggressive** (more auto-switches)
```
High:   70% → Switch on good confidence
Medium: 40% → Switch even with moderate confidence
Low:    20% → Show hints for most detections
```
**Use when**: You want maximum automation, willing to manually override occasionally

---

## Configuration Settings

### Confidence Thresholds
- `high` - Threshold for immediate auto-switch (0.0-1.0)
- `medium` - Threshold for auto-switch with notification (0.0-1.0)
- `low` - Threshold for hint display (0.0-1.0)

**Guidelines**:
- `high` should be 0.70-0.95 (too high = rarely switches)
- `medium` should be 0.40-0.70 (middle ground)
- `low` should be 0.20-0.50 (hint threshold)
- Keep: `high > medium > low`

### Factor Weights
- `file` - File pattern analysis weight (0.0-1.0)
- `conversation` - Conversation topic weight (0.0-1.0)
- `learning` - ML learned patterns weight (0.0-1.0)
- `history` - Historical success weight (0.0-1.0)
- `time` - Time-based patterns weight (0.0-1.0)

**Guidelines**:
- Total should equal 1.0 (100%)
- File pattern typically most reliable (0.30-0.40)
- Conversation adds context (0.20-0.30)
- Learning improves over time (0.15-0.25)
- History validates accuracy (0.10-0.20)
- Time is supplementary (0.05-0.10)

### Behavior Settings
- `feedbackWindow` - Seconds to detect manual override (10-60)
- `showConfidenceBreakdown` - Display factor details (true/false)
- `enablePassiveLearning` - Learn from workflow automatically (true/false)
- `enableNegativeFeedback` - Record manual overrides as failures (true/false)

---

## How to Configure

**Step 1**: Check current settings
```bash
/mode config
```

**Step 2**: Try a preset
```bash
/mode config preset aggressive  # More auto-switching
# or
/mode config preset conservative  # Less auto-switching
```

**Step 3**: Fine-tune individual settings
```bash
/mode config set high 0.85      # Raise high threshold
/mode config set file 0.40      # Increase file weight
```

**Step 4**: Test the changes
- Open a few files and observe auto-switching behavior
- Check `/mode learn` to see accuracy impact
- Adjust thresholds if too many/too few switches

---

## Advanced: Direct JSON Editing

Edit `.claude/.context-config.json` directly for full control:
```json
{
  "intelligentSwitching": {
    "confidenceThresholds": {
      "high": 0.80,
      "medium": 0.50,
      "low": 0.30
    },
    "factorWeights": {
      "file": 0.35,
      "conversation": 0.25,
      "learning": 0.20,
      "history": 0.15,
      "time": 0.05
    }
  }
}
```

---

## Troubleshooting

**Too many auto-switches?**
→ Use `/mode config preset conservative` or raise thresholds

**Too few auto-switches?**
→ Use `/mode config preset aggressive` or lower thresholds

**Wrong modes suggested?**
→ Check factor weights, increase `learning` weight as system learns

**Want manual control?**
→ Disable auto-switching: `/mode auto disable`

---

## Configuration File Location

**File**: `.claude/.context-config.json`
**Format**: JSON
**Applies**: Immediately on next file open or mode detection

**Backup**: Consider backing up before major changes
```bash
cp .claude/.context-config.json .claude/.context-config.backup.json
```

---

**Related Commands**:
- `/mode learn` - View learning system status
- `/mode status` - View current mode and settings
- `/mode auto enable/disable` - Toggle auto-switching
