# Pre-Commit Validation

You are performing a quick pre-commit validation to ensure code quality before committing changes.

## Your Task

Run a fast, focused validation suitable for pre-commit hooks. This is a streamlined version of the full audit, focusing on breaking changes and critical issues only.

---

## Quick Validation Checklist

### 1. **Compilation Safety** (Critical)

Check for common compilation issues:

- ❌ Null pointer dereferences without guards
- ❌ Missing null checks before dereferencing
- ❌ Accessing members on potentially null objects
- ❌ Missing include guards
- ❌ Forward declaration issues

**Action**: Grep for patterns like:
```cpp
->MaxChargeTime     // Without prior null check
->AttackMontage     // Without prior null check
FindComponentByClass<  // Without null check after
```

### 2. **Critical State Machine Issues** (Critical)

Check for state deadlocks:

- ❌ States with no exit path
- ❌ `SetCombatState()` to same state repeatedly
- ❌ Missing state transition validation
- ❌ Infinite loops in state logic

**Action**: Read `SetCombatState()` and verify all states have exit paths

### 3. **Memory Leaks** (Critical)

Check for resource cleanup:

- ❌ Timers set but never cleared
- ❌ Delegates subscribed but never unsubscribed
- ❌ `NewObject` without proper ownership
- ❌ Missing `Super::` calls in overrides

**Action**: Look for timer handles without corresponding `ClearTimer` calls

### 4. **Input Buffering Breaking Changes** (High)

Verify core input logic hasn't regressed:

- ✅ Input buffers when `CurrentState == Attacking`
- ✅ Buffering happens regardless of `bCanCombo`
- ✅ `CanAttack()` only returns true for Idle
- ✅ `ExecuteAttack()` only callable from Idle

**Action**: Read `OnLightAttackPressed()` and `OnHeavyAttackPressed()`

### 5. **Hold System Breaking Changes** (High)

Verify hold logic is correct:

- ✅ `OpenHoldWindow()` checks `CurrentAttackInputType` matches held button
- ✅ `ReleaseHeldLight()` uses `ExecuteComboAttack()` not `ExecuteAttack()`
- ✅ `ReleaseHeldHeavy()` uses `ExecuteComboAttack()` not `ExecuteAttack()`
- ❌ Calling `ExecuteAttack()` from non-Idle states

**Action**: Read hold window and release functions

### 6. **Delegate Architecture** (Medium)

Quick delegate check:

- ❌ New `DECLARE_DYNAMIC_MULTICAST_DELEGATE` in component headers
- ✅ All declares in `CombatTypes.h`
- ❌ Broadcasting delegates before state actually changes

**Action**: Grep for `DECLARE_DYNAMIC_MULTICAST_DELEGATE` outside CombatTypes.h

### 7. **Debug Code Left In** (Medium)

Check for debug code:

- ❌ Hardcoded test values
- ❌ Commented-out code blocks (large sections)
- ❌ `UE_LOG` with sensitive info
- ❌ Test-only code paths without guards

**Action**: Look for `// TODO`, `// TEST`, `// HACK` comments

---

## Output Format

Provide a concise report suitable for terminal output:

```markdown
# Pre-Commit Validation

## Status: [✅ PASS | ⚠️ WARNINGS | ❌ FAIL]

---

## 🔴 Critical Issues (Must Fix Before Commit)
[List critical issues or "None found"]

---

## 🟡 Warnings (Should Fix)
[List warnings or "None found"]

---

## ℹ️ Info
- Files checked: X
- Validation time: ~Xs
- Recommendation: [COMMIT | FIX CRITICAL | FIX ALL]

---

## Quick Fixes
[For each issue, one-line fix instruction]

```

---

## Speed Optimization

This is a **fast validation** for pre-commit:

- **Don't** read entire files if not needed
- **Use** Grep for pattern matching
- **Focus** on critical issues only
- **Skip** comprehensive testing (that's for full audit)
- **Target** < 30 second execution time

---

## Validation Rules

**FAIL** (block commit) if:
- Critical issues found (crashes, null pointers without guards)
- State machine deadlocks detected
- Memory leaks identified

**WARN** (allow commit but notify) if:
- Medium issues found (code quality)
- Missing null checks in non-critical paths
- Debug code present

**PASS** (green light) if:
- No critical issues
- No medium issues (or already tracked)

---

Begin pre-commit validation now.