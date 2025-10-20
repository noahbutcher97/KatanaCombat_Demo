# Post-Fix Validation

You are validating that a bug fix correctly resolved the issue without introducing regressions.

## Your Task

After the user has fixed a bug, validate:
1. The original issue is resolved
2. No new bugs were introduced
3. Related systems still work correctly
4. Generate regression tests to prevent issue from returning

---

## Validation Workflow

### Step 1: Understand the Fix

Ask user to describe (if not already provided):
- What was the original bug?
- What files were changed?
- What was the root cause?
- What was the fix?

### Step 2: Verify Fix Correctness

Based on the bug type, validate the fix:

#### For Null Pointer Crashes
- ✅ Null check added before dereference
- ✅ Null check is in correct location
- ✅ All code paths have null protection
- ✅ Graceful handling (not just early return if logic needed)

#### For State Machine Issues
- ✅ State transition now follows valid path
- ✅ No orphaned states created
- ✅ Exit conditions properly defined
- ✅ State set before actions that depend on it

#### For Input Buffering Issues
- ✅ Input buffers in all attack states
- ✅ Combo window doesn't gate buffering
- ✅ Execution timing correct (snappy vs responsive)
- ✅ Buffer cleared appropriately

#### For Hold System Issues
- ✅ Correct input type checked
- ✅ Release uses `ExecuteComboAttack()` not `ExecuteAttack()`
- ✅ Returns to correct state after release
- ✅ Animation resumes/transitions properly

#### For Memory Leaks
- ✅ Resources cleaned up in destructor
- ✅ Timers cleared when no longer needed
- ✅ Delegates unsubscribed appropriately
- ✅ No circular references

### Step 3: Check for Regressions

Verify related systems still work:

**If fixed CombatComponent**:
- Check other attack execution paths
- Verify state transitions not affected
- Test combo system still works
- Validate hold system not broken

**If fixed input handling**:
- Test all input types (light, heavy, evade, block)
- Verify buffering works for all
- Check hold detection still works
- Test combo queuing

**If fixed state machine**:
- Verify all state transitions
- Check no new deadlocks introduced
- Test state-dependent logic
- Validate state broadcasts

**If fixed hold system**:
- Test light and heavy holds separately
- Verify directional input works
- Check release timing
- Test hold window detection

### Step 4: Generate Regression Test

Create a test that would catch this bug if it returns:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_Regression_[BugName],
    "KatanaCombat.Regression.[BugName]",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FTest_Regression_[BugName]::RunTest(const FString& Parameters)
{
    // ARRANGE
    [Setup that triggers the original bug]

    // ACT
    [Execute the action that caused the crash/bug]

    // ASSERT
    [Verify bug is fixed - should pass now]
    TestTrue(TEXT("[Bug should not occur]"), [Condition]);

    return true;
}
```

---

## Output Format

```markdown
# Post-Fix Validation Report

## 🐛 Original Bug
**Description**: [What was broken]
**Root Cause**: [Why it happened]
**Files Changed**: [List of modified files]

---

## ✅ Fix Verification

### Fix Correctness
- [✅/❌] Null checks added in correct locations
- [✅/❌] Logic handles edge cases
- [✅/❌] All code paths covered
- [✅/❌] No temporary workarounds (proper fix)

**Verdict**: [CORRECT / INCOMPLETE / INCORRECT]

---

## 🔍 Regression Check

### Related Systems Tested
- [System 1]: [✅ Working / ❌ Broken / ⚠️ Needs Testing]
- [System 2]: [✅ Working / ❌ Broken / ⚠️ Needs Testing]
- [System 3]: [✅ Working / ❌ Broken / ⚠️ Needs Testing]

### New Issues Introduced
[List any new bugs found, or "None detected"]

**Verdict**: [NO REGRESSIONS / REGRESSIONS FOUND]

---

## 🧪 Regression Test Generated

### Test: [Test Name]
**Purpose**: Ensure this bug doesn't return

**Code**:
```cpp
[Generated test code]
```

**Test Coverage**:
- [What this test validates]

---

## 📊 Final Verdict

**Fix Status**: [✅ APPROVED / ⚠️ NEEDS WORK / ❌ REJECTED]

**Recommendation**:
[What should be done next]

**Remaining Work**:
- [ ] [Any follow-up tasks]

---

## 🔄 Suggested Follow-Up

1. [Immediate actions]
2. [Short-term improvements]
3. [Long-term considerations]
```

---

## Validation Priorities

**Priority 1**: Verify original bug is fixed
**Priority 2**: Check for regressions in directly related code
**Priority 3**: Check for regressions in indirectly related code
**Priority 4**: Generate regression test

---

## Special Cases

### If Fix Involves Refactoring
- Verify behavior is identical to before
- Check all call sites updated
- Validate interfaces unchanged (or properly updated)
- Test edge cases more thoroughly

### If Fix Changes State Machine
- Verify ALL state transitions
- Check state-dependent logic thoroughly
- Test boundary conditions
- Validate no orphaned states

### If Fix Changes Timing/Windows
- Test timing-critical logic
- Verify windows open/close correctly
- Check overlapping windows
- Test rapid input scenarios

### If Fix Is "Hacky" or Temporary
- Flag for future proper fix
- Document why hack was needed
- Create task to revisit
- Generate test so proper fix can be validated

---

Begin post-fix validation now. If bug details not provided, ask user first.