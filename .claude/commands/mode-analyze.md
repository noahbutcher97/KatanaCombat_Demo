# /mode analyze - Analyze Current File with Confidence Breakdown

Run the intelligent mode detection system on the currently open file and display a detailed confidence breakdown with visual feedback.

**Usage**:
```
/mode analyze
/mode analyze [file-path]
```

**Examples**:
- `/mode analyze` - Analyze currently open file (if available)
- `/mode analyze Source/KatanaCombat/Public/Animation/AnimNotify_Phase.h` - Analyze specific file

---

## What This Shows

### Visual Confidence Report
```
================================================================
      INTELLIGENT MODE DETECTION - CONFIDENCE REPORT
================================================================

SUGGESTED MODE: ANIMATION
CONFIDENCE:     54.5% (medium)

CONFIDENCE BAR: ###########################-----------------------

+-------------------------------------------------------------+
|  FACTOR CONTRIBUTIONS                                       |
+-------------------------------------------------------------+
|  file          #############------- 0.332  |
|    -> conf: 95% x weight: 35% -> animation                  |
|  learning      #-------------------  0.027  |
|    -> conf: 13.5% x weight: 20% -> animation                |
|  history       ###############-----  0.15   |
|    -> conf: 100% x weight: 15% -> animation                 |
|  time          #-------------------  0.035  |
|    -> conf: 70% x weight: 5% -> animation                   |
+-------------------------------------------------------------+

Algorithm: intelligent-v3
Reason:    Multi-factor analysis (file, learning, history, time) suggests animation

================================================================
  RECOMMENDATION: SWITCH TO ANIMATION
================================================================

  Current: full → Suggested: animation

---------------------------------------------------------------
  Use '/mode config' to adjust thresholds and weights
  Use '/mode learn' to view ML learning status
---------------------------------------------------------------
```

---

## Factor Breakdown Explained

### 1. **File Pattern** (35% weight)
- Analyzes file path, extension, and naming patterns
- Example: `AnimNotify_*.h` → 95% confidence animation mode

### 2. **Conversation Topics** (25% weight)
- Extracts keywords from recent conversation (when available)
- Not available during file-open time (always 0% in auto-context)

### 3. **Learned Patterns** (20% weight)
- Uses ML to match file patterns from past usage
- Includes Bayesian inference + temporal decay
- Correlation boost from topic associations

### 4. **Historical Success** (15% weight)
- Per-file and per-mode accuracy tracking
- Falls back to global accuracy if no specific history

### 5. **Time-Based Patterns** (5% weight)
- Work hours heuristic (9am-5pm = higher confidence)
- Supplementary factor to other signals

---

## Confidence Levels

- **High (≥80%)**: Strong signal, auto-switch recommended
- **Medium (50-79%)**: Good signal, auto-switch with notification
- **Low (<50%)**: Weak signal, show hint only

---

## Use Cases

### Debug Mode Detection
```
/mode analyze Source/KatanaCombat/Private/Core/CombatComponent.cpp
```
Shows why the system suggested a particular mode and confidence breakdown.

### Compare Files
```
/mode analyze Source/KatanaCombat/Public/Animation/AnimNotify_Phase.h
/mode analyze Source/KatanaCombat/Public/Data/AttackData.h
```
See how different files are classified and which factors contribute.

### Validate ML Learning
After training with `/mode train`, use `/mode analyze` to verify the system learned correctly.

### Tune Configuration
Use `/mode analyze` to see current factor weights, then adjust with `/mode config` if needed.

---

## Implementation

**Action**: Run intelligent mode detector with ShowDetails flag and display rich output.

**Steps**:
1. Get file path (from context or parameter)
2. Call `intelligent-mode-detector.ps1` with `-ShowDetails`
3. Parse JSON result
4. Call `display-confidence-breakdown.ps1` with full output
5. Display comprehensive report

---

## Related Commands

- `/mode config` - Adjust thresholds and weights
- `/mode learn` - View ML learning status
- `/mode train` - Manually train patterns
- `/mode status` - Current mode and auto-switch settings
- `/mode suggest` - Get mode recommendation

---

**Note**: This command provides transparency into the intelligent mode detection algorithm, showing exactly why a mode was suggested and with what confidence.