# Check IDE Warnings

You are analyzing IDE diagnostics for the KatanaCombat project using intelligent filtering and categorization.

## Your Task

Run a comprehensive diagnostics check on combat system files, filtering out false positives and prioritizing actionable issues.

---

## Step 1: Load Configuration

Read `.claude/diagnostics-config.json` to get:
- Ignore patterns (Blueprint-exposed, editor-only, etc.)
- Severity levels and actions
- Categorization rules
- Context filters (if context mode active)

---

## Step 2: Gather Diagnostics

Use `mcp__ide__getDiagnostics` to check key files:

### Core Combat Files (Priority 1)
```
Source/KatanaCombat/Public/Core/CombatComponentV2.h
Source/KatanaCombat/Private/Core/CombatComponentV2.cpp
Source/KatanaCombat/Public/Core/CombatComponent.h
Source/KatanaCombat/Private/Core/CombatComponent.cpp
Source/KatanaCombat/Public/CombatTypes.h
```

### Animation Files (Priority 2)
```
Source/KatanaCombat/Public/Animation/AnimNotify_AttackPhaseTransition.h
Source/KatanaCombat/Public/Animation/AnimNotifyState_ParryWindow.h
Source/KatanaCombat/Public/Animation/AnimNotifyState_HoldWindow.h
Source/KatanaCombat/Public/Animation/AnimNotifyState_ComboWindow.h
```

### Data Files (Priority 3)
```
Source/KatanaCombat/Public/Data/AttackData.h
Source/KatanaCombat/Public/Data/AttackConfiguration.h
Source/KatanaCombat/Public/Data/CombatSettings.h
```

**Context-Aware**: If a context mode is active (check for recent `/mode` usage), prioritize files from that context's `includePatterns`.

---

## Step 3: Filter Diagnostics

For each diagnostic, apply filters from config:

### Ignore Patterns
Check if diagnostic matches any ignore pattern:
- Blueprint-exposed members (OnAttackStarted, OnPhaseChanged, etc.)
- Editor-only code (WITH_EDITOR blocks)
- Intentionally unused (// NOLINT comments)
- Unreal macro expansions (GENERATED_BODY, UPROPERTY)

**Example**:
```
⚠️ "Function 'OnAttackStarted' is never used"
→ Check: Is it BlueprintAssignable?
→ YES: Ignore (reason: "Used from Blueprint, not C++")
```

### Categorize
Assign category based on patterns:
- **Critical**: Null pointers, uninitialized, memory leaks
- **Security**: Buffer overflows, injection risks
- **Performance**: Inefficient operations, expensive loops
- **Style**: Naming, formatting, whitespace

---

## Step 4: Report Results

### Output Format

```markdown
# 🔍 Diagnostics Report - KatanaCombat

**Files Checked**: [count]
**Total Issues**: [count]
**Filtered Issues**: [count after ignore patterns]
**Actionable Issues**: [critical + security + performance count]

---

## ❌ Critical Issues (Block Commit)

[If none: ✅ No critical issues found]

[For each critical issue:]
### File: [path]:[line]
**Severity**: Error
**Message**: [diagnostic message]
**Category**: [critical/security]
**Action Required**: [specific fix suggestion]
**File Link**: [path:line]

---

## ⚠️ Warnings (Review Recommended)

[Group by file]

### [filename]
- **Line [N]**: [message] → **Fix**: [suggestion]
- **Line [N]**: [message] → **Fix**: [suggestion]

---

## 🔇 Filtered Out (False Positives)

**Ignored**: [count] issues
**Reasons**:
- Blueprint-exposed: [count]
- Editor-only: [count]
- Intentionally unused: [count]
- Macro expansions: [count]

<details>
<summary>View filtered issues</summary>

[List filtered issues with reasons]

</details>

---

## 💡 Recommendations

[Based on patterns found, suggest:]
1. [Specific actionable recommendations]
2. [Documentation to review]
3. [Commands to run (/fix-crash, /validate-combat, etc.)]

---

## ✅ Next Steps

[If critical issues:]
- ❌ **DO NOT COMMIT** until critical issues resolved
- Run: [specific commands to fix]

[If only warnings:]
- ⚠️ Review warnings before commit
- Consider: [optional improvements]

[If clean:]
- ✅ **Safe to commit**
- Run: `/pre-commit` for final validation
```

---

## Step 5: Context-Specific Insights

If context mode is active, provide mode-specific analysis:

**Animation Context**:
- Check for timing issues in notifies
- Validate phase transition order
- Suggest: Review `docs/PHASE_SYSTEM_MIGRATION.md`

**Combat-Logic Context**:
- Check for state machine issues
- Validate input handling
- Suggest: Review `docs/SYSTEM_PROMPT.md` design principles

**Testing Context**:
- Check test assertions
- Validate test coverage
- Suggest: Run automation tests

---

## Special Cases

### Known False Positives (CombatComponentV2.h)
From previous diagnostics, we know:
- `OnAttackStarted` - BlueprintAssignable (ignore)
- `OnPhaseChanged` - BlueprintAssignable (ignore)
- `OnComboWindowChanged` - BlueprintAssignable (ignore)
- `OnHoldActivated` - BlueprintAssignable (ignore)
- `OnMontageEvent` - BlueprintAssignable (ignore)
- `CancelActionsWithPriority` - Used in V2 system (verify actual usage)

### Auto-Fix Suggestions
For style issues, provide exact fixes:
```cpp
// Before:
int unusedVar = 5;

// After:
[[maybe_unused]] int unusedVar = 5;
// OR remove if truly unused
```

---

## Performance Note

Only check files within active context mode to minimize overhead. If no context active, check Priority 1 files only.

---

## Integration with Other Commands

Suggest follow-up commands based on findings:
- Critical issues → `/fix-crash`
- Architecture violations → `/validate-combat`
- Doc mismatches → `/sync-docs`
- Before commit → `/pre-commit`