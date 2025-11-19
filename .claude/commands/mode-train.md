# /mode train - Manually Train ML Learning System

Manually provide feedback to the learning system for a specific file pattern and mode combination.

**Usage**:
```
/mode train [pattern] [mode] [success|failure]
```

**Parameters**:
- `pattern` - File pattern name (e.g., AnimNotify, CombatComponent, AttackData)
- `mode` - Target mode (animation, combat-logic, data-assets, editor-ui, testing, documentation, full)
- `success|failure` - Whether the suggestion was correct

**Examples**:
- `/mode train AnimNotify animation success` - Record successful animation mode for AnimNotify files
- `/mode train CombatComponent combat-logic failure` - Record incorrect suggestion
- `/mode train CustomEditor editor-ui success` - Train new pattern

**When to Use**:
1. **Correct Misclassifications**: When auto-switch suggests wrong mode
2. **Bootstrap New Patterns**: Train system on new file types before they're used
3. **Fine-Tune Confidence**: Reinforce or weaken specific pattern-mode associations
4. **Override Low Confidence**: Manually confirm a low-confidence suggestion

**How It Works**:
- **Success**: Increments successCount, increases confidence, updates temporal decay
- **Failure**: Increments failureCount, decreases confidence for that mode
- **Bayesian Update**: Combines with prior data using probabilistic inference
- **Correlation Update**: If pattern exists, updates topic correlations

**Best Practices**:
- Provide feedback for edge cases (files that are ambiguous)
- Don't overtrain - system learns automatically from normal usage
- Use failure feedback to prevent wrong auto-switches
- Check `/mode learn` to see current confidence before training

**Example Workflow**:
```
1. Open AnimNotifyState_ComboWindow.h
2. System suggests "animation" but you want "combat-logic"
3. Run: /mode train AnimNotifyState combat-logic success
4. Future opens of similar files will have higher combat-logic confidence
```

**Note**: Training updates `.claude/.context-learning.json` immediately. No restart required.
