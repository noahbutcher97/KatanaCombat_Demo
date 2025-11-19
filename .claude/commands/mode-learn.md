# /mode learn - View ML Learning System Status

Display comprehensive status of the ML learning system including learned patterns, confidence scores, correlations, and global statistics.

**Usage**:
```
/mode learn
/mode learn [pattern]
```

**Examples**:
- `/mode learn` - Show all learned patterns and system stats
- `/mode learn AnimNotify` - Show details for specific pattern
- `/mode learn CombatComponent` - Query combat-logic pattern

**What This Shows**:
1. **Global Statistics**:
   - Total context switches
   - Auto-switch accuracy rate
   - Success/failure counts
   - Last updated timestamp

2. **Learned Patterns** (per file pattern):
   - Suggested mode
   - Confidence score (Bayesian inference)
   - Success/failure ratio
   - Temporal decay factor
   - Feature weights (file vs conversation)

3. **Topic Correlations** (if available):
   - Topics associated with pattern
   - Modes correlated with pattern
   - Correlation strengths

4. **Recommendations**:
   - Patterns that need more training
   - High-confidence patterns
   - Stale patterns (old temporal decay)

**Learning System Overview**:

The ML learning system uses:
- **Bayesian Inference**: Combines prior knowledge with observed data
- **Temporal Decay**: Recent patterns weighted higher than old ones
- **Feature Adaptation**: Dynamically adjusts file vs conversation weights
- **Correlation Tracking**: Links conversation topics to file patterns

Data stored in: `.claude/.context-learning.json`

**Note**: Learning system is passive - it learns from your workflow automatically. No manual training required unless you want to reset or fine-tune.
