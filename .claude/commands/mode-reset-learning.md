# /mode reset-learning - Reset ML Learning System

Reset the ML learning system to default state, clearing all learned patterns, correlations, and statistics.

**Usage**:
```
/mode reset-learning
/mode reset-learning [pattern]
```

**Examples**:
- `/mode reset-learning` - Reset entire learning system (requires confirmation)
- `/mode reset-learning AnimNotify` - Reset only AnimNotify pattern

**What Gets Reset**:

**Full Reset** (`/mode reset-learning`):
- All learned patterns
- All topic correlations
- Global statistics (accuracy, switches, success/failure counts)
- Resets to fresh state as if system never learned anything

**Pattern-Specific Reset** (`/mode reset-learning AnimNotify`):
- Only the specified pattern's data
- Pattern's Bayesian stats (successCount, failureCount)
- Pattern's temporal decay factor
- Pattern's topic correlations
- Keeps global stats and other patterns intact

**When to Use**:
1. **After Major Refactoring**: File structure changed, old patterns obsolete
2. **Poor Accuracy**: System learned wrong patterns, better to start fresh
3. **Testing**: Validate learning system behavior from clean state
4. **Mode Reorganization**: Changed what modes represent, need retraining

**Confirmation Required**:
Full reset requires typing "yes" to confirm. This is a destructive operation.

**Example Scenarios**:

**Scenario 1: Major Codebase Restructure**
```
# You reorganized animation files into new directories
# Old patterns now point to wrong locations
/mode reset-learning
# Type "yes" to confirm
# Start fresh with new file structure
```

**Scenario 2: One Pattern is Wrong**
```
# AnimNotify pattern keeps suggesting wrong mode
/mode reset-learning AnimNotify
# Pattern cleared, will learn from scratch on next uses
```

**Scenario 3: Low Accuracy After Updates**
```
# Check accuracy: /mode learn
# If accuracy < 60%, consider reset
/mode reset-learning
# Confirm reset
# System will relearn from your usage patterns
```

**Recovery**:
Once reset, the system immediately starts learning again from your workflow. Within 5-10 context switches, accuracy should return to ~80%.

**Backup**:
Before resetting, you can backup learning data:
```
Copy .claude/.context-learning.json to .claude/.context-learning.backup.json
```

**Note**: Cannot undo reset. Make sure you really want to clear learning data before confirming.
