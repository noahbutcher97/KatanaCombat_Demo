# Full Combat System Audit

You are performing a comprehensive audit of the KatanaCombat combat system. This combines validation, documentation sync checking, and test generation into a single workflow.

## Your Task

Execute a complete audit in three phases:

---

## Phase 1: Combat System Validation

Perform comprehensive validation of the combat system implementation.

### Validation Checklist

#### 1. State Transition Validation
- Check all `SetCombatState()` calls for valid transitions
- Verify state machine has no deadlocks
- Validate recovery paths from all states

#### 2. Input Buffering Logic
- Verify input ALWAYS buffers during attacks
- Check combo window only controls timing (not whether to buffer)
- Validate snappy vs responsive execution paths

#### 3. Hold System
- Check `OpenHoldWindow()` matches `CurrentAttackInputType` with held button
- Verify `ReleaseHeldLight()` and `ReleaseHeldHeavy()` use `ExecuteComboAttack()`
- Ensure hold release returns to normal buffering

#### 4. Attack Execution
- Verify `ExecuteAttack()` only accepts Idle state
- Check `ExecuteComboAttack()` works from Attacking state
- Validate null checks for `CurrentAttackData`

#### 5. Parry System
- Confirm parry window on ATTACKER's montage
- Verify defender checks enemy's `IsInParryWindow()`
- Validate fallback to blocking

#### 6. Memory Safety
- Check all `CurrentAttackData->` accesses have null guards
- Verify timer cleanup
- Identify potential crashes

### Output Phase 1

```markdown
# Phase 1: Combat System Validation

## ✅ Passed Validations
[List what passed]

## 🔴 Critical Issues
[Crashes, major bugs with file:line]

## 🟡 Medium Issues
[Logic errors with file:line]

## 🟢 Low Issues
[Code quality improvements]
```

---

## Phase 2: Documentation Sync Check

Compare implementation against documentation.

### Documentation Validation

#### 1. Core Design Principles
- **Phases vs Windows**: Verify phases are exclusive, windows overlap
- **Input Buffering**: Confirm always buffers during attacks
- **Parry Detection**: Validate defender-side checking
- **Hold Mechanics**: Check button state at window start

#### 2. Default Values Audit
Compare against `ARCHITECTURE_QUICK.md`:
- ComboInputWindow: 0.6s
- ParryWindow: 0.3s
- MaxPosture: 100.0f
- PostureRegenRate_Attacking: 50.0f
- Etc.

#### 3. Component Responsibilities
- CombatComponent: State machine, attacks, combos
- TargetingComponent: Cone targeting, motion warp
- WeaponComponent: Hit detection
- HitReactionComponent: Damage reception

#### 4. Common Mistakes Check
- ❌ Hold as attack phase
- ❌ Combo window gating buffering
- ❌ Parry window on defender
- Etc.

### Output Phase 2

```markdown
# Phase 2: Documentation Sync

## ✅ Compliant Areas
[What matches docs]

## ⚠️ Discrepancies Found
[Code vs docs mismatches with file:line]

## 📝 Documentation Updates Needed
[Where docs are outdated]

## 📊 Compliance Rate
[X% compliant]
```

---

## Phase 3: Test Generation

Generate targeted tests based on findings from Phase 1 and 2.

### Test Generation Strategy

1. **For each Critical issue found**: Generate regression test
2. **For each Medium issue found**: Generate validation test
3. **For recently fixed features**: Generate integration tests
4. **For core functionality**: Generate unit tests

### Test Priorities

Based on Phase 1 findings:
- **Priority 1**: Tests for Critical issues (must fix)
- **Priority 2**: Tests for Medium issues (should fix)
- **Priority 3**: Tests for Low issues (nice to fix)
- **Priority 4**: General coverage tests

### Output Phase 3

```markdown
# Phase 3: Test Generation

## 🔴 Critical Issue Tests (Priority 1)
[Tests for crashes/major bugs found in Phase 1]

## 🟡 Medium Issue Tests (Priority 2)
[Tests for logic errors found in Phase 1]

## 🟢 Coverage Tests (Priority 3)
[General tests for features validated in Phase 2]

## Test Code Templates
[Copy-paste ready C++ test code]
```

---

## Final Summary Report

After all three phases, provide a consolidated report:

```markdown
# KatanaCombat Full Audit Report
*Generated: [Date/Time]*

---

## Executive Summary

**Overall Health**: [Excellent/Good/Fair/Poor]
**Validation Pass Rate**: X%
**Documentation Compliance**: X%
**Tests Generated**: X

**Action Required**: [Yes/No]
**Blocking Issues**: X critical issues found

---

## Critical Findings (Action Required)

### [Issue Name]
- **Type**: [Crash/Logic Error/Documentation Mismatch]
- **Location**: [File:line]
- **Impact**: [Description]
- **Fix**: [Recommendation]
- **Test Generated**: [Test name]

[Repeat for each critical issue]

---

## Medium Priority Findings

[Same format, medium issues]

---

## Low Priority Improvements

[Same format, low issues]

---

## Documentation Sync Status

### Needs Updating
1. [Doc file] - [What needs updating]
2. [Doc file] - [What needs updating]

### Compliant
- [List compliant areas]

---

## Generated Tests Summary

**Total Tests**: X
- Critical issue tests: X
- Medium issue tests: X
- Coverage tests: X

**Next Steps**:
1. Implement Priority 1 tests (critical)
2. Fix critical issues
3. Re-run audit to verify fixes

---

## Recommended Actions

**Immediate** (Fix within 1 day):
- [Critical fixes]

**Short-term** (Fix within 1 week):
- [Medium fixes]
- [Doc updates]

**Long-term** (Nice to have):
- [Low priority improvements]

---

## Audit Metrics

- **Files Audited**: X
- **Lines of Code Reviewed**: ~X
- **Validation Checks Performed**: X
- **Documentation Sections Checked**: X
- **Tests Generated**: X

---

## Re-Audit Recommendation

Next audit: [Timeframe based on findings]
- If critical issues: Re-audit after fixes
- If medium issues only: Re-audit in 1 week
- If all passed: Re-audit in 1 month
```

---

## Execution Instructions

1. **Read source files** (use Read, Grep, Glob tools):
   - CombatComponent.h/.cpp
   - All AnimNotifyState files
   - CombatTypes.h
   - Documentation files

2. **Execute Phase 1** (Validation):
   - Run all validation checks
   - Categorize findings by severity
   - Note file:line for each issue

3. **Execute Phase 2** (Doc Sync):
   - Compare code vs docs
   - Identify discrepancies
   - Calculate compliance rate

4. **Execute Phase 3** (Test Generation):
   - Generate tests for Phase 1 issues
   - Generate coverage tests for Phase 2 compliant areas
   - Prioritize tests by severity

5. **Generate Final Report**:
   - Consolidate all findings
   - Provide executive summary
   - Give actionable recommendations

---

## Special Instructions

- **Be thorough**: This is a comprehensive audit, take time to check everything
- **Prioritize by severity**: Focus on critical issues first
- **Provide file:line references**: Every finding must have location
- **Generate actionable tests**: Tests should be copy-paste ready
- **Give clear recommendations**: Tell exactly what to do next

---

Begin full audit now.