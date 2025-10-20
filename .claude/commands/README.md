# KatanaCombat Slash Commands

Custom Claude Code agents for the KatanaCombat project.

## Available Commands

### Core Commands (Individual)

### 💬 /clarify - Interactive Requirements Clarification
**Purpose**: Gather requirements through guided multiple-choice questions

**What it does**:
- Analyzes vague or complex requests
- Generates contextual multiple-choice questions based on codebase
- Uses AskUserQuestion to present options in Claude Code console
- Allows scrolling and selecting from options
- Synthesizes answers into clear requirements
- Recommends implementation approaches
- Prevents misunderstandings before coding starts

**Usage**:
```
/clarify
```
*(Works best when you provide a vague request like "add a new attack" or "fix the combo system")*

**When to use**:
- Before implementing complex features
- When request is vague or ambiguous
- To explore implementation options
- When you're not sure what you want
- To understand tradeoffs before coding
- When "it feels off" but can't articulate why

**Example Scenarios**:
- "Add a new attack" → Asks: type, combo integration, mechanics
- "Fix the combo system" → Asks: what aspect, which path, timing preference
- "Improve parrying" → Asks: what's wrong, rewards, difficulty
- "Make it feel better" → Asks: which aspect, when, compared to what

**Output**: Requirements summary with recommended approaches and next steps

**Execution time**: ~2-5 minutes (interactive)

---

### 📋 /sync-docs - Documentation Sync Checker
**Purpose**: Ensure code implementation matches project documentation

**What it does**:
- Compares actual code vs `docs/SYSTEM_PROMPT.md`, `docs/ARCHITECTURE.md`, `docs/ARCHITECTURE_QUICK.md`
- Validates core design principles (phases vs windows, input buffering, parry detection, hold mechanics)
- Checks default values match documented values
- Identifies outdated documentation
- Reports discrepancies with file:line references

**Usage**:
```
/sync-docs
```

**When to use**:
- After implementing new features
- Before major releases
- When onboarding new developers
- After refactoring sessions
- Periodically (weekly/monthly)

**Output**: Detailed report of compliant areas, discrepancies, and documentation update recommendations

---

### ✅ /validate-combat - Combat System Validator
**Purpose**: Comprehensive validation of combat system implementation

**What it does**:
- Validates state machine transitions (Idle → Attacking → Hold, etc.)
- Checks input buffering logic (always buffers, combo window timing)
- Verifies hold system (correct input type detection)
- Audits attack execution paths (ExecuteAttack vs ExecuteComboAttack)
- Validates parry system (defender-side detection)
- Identifies memory safety issues (null pointer dereferences)
- Checks delegate architecture
- Finds performance red flags

**Usage**:
```
/validate-combat
```

**When to use**:
- After making combat logic changes
- When debugging state transition issues
- Before committing major changes
- When experiencing crashes
- As part of pre-merge code review

**Output**: Validation report with critical/medium/low issues, file:line references, and fix recommendations

---

### 🧪 /generate-tests - Test Case Generator
**Purpose**: Generate comprehensive test cases and test code templates

**What it does**:
- Generates unit tests for individual components
- Creates integration test scenarios
- Produces state machine transition tests
- Generates input handling tests
- Creates edge case test scenarios
- Provides copy-paste ready C++ test code
- Generates Blueprint test actor templates

**Usage**:
```
/generate-tests                 # Generate full test suite (48 tests)
/generate-tests combo           # Generate combo system tests only
/generate-tests parry           # Generate parry system tests only
/generate-tests hold            # Generate hold system tests only
/generate-tests input           # Generate input buffering tests only
/generate-tests states          # Generate state machine tests only
```

**Supported features**:
- `combo` - Combo system (window, queuing, execution)
- `parry` - Parry system (detection, timing, posture)
- `hold` - Hold system (detection, release, directional)
- `input` - Input buffering and handling
- `states` - State machine transitions
- `posture` - Posture system
- `attacks` - Basic attack execution

**When to use**:
- Before implementing new features (TDD)
- After implementing features (regression tests)
- When fixing bugs (add test that reproduces bug)
- Before major releases
- When test coverage is low

**Output**: Test descriptions, C++ test code templates, Blueprint test setups, execution instructions

---

### Combo Commands (Workflow Automation)

### 🎯 /full-audit - Complete System Audit
**Purpose**: Comprehensive 3-phase audit (validate + sync + test)

**What it does**:
- **Phase 1**: Runs full combat system validation
- **Phase 2**: Checks documentation sync
- **Phase 3**: Generates targeted tests based on Phase 1 & 2 findings
- Produces consolidated executive summary report

**Usage**:
```
/full-audit
```

**When to use**:
- Weekly/monthly system health checks
- Before major releases
- After large refactoring sessions
- When preparing for code review
- Comprehensive project assessment

**Output**: Multi-phase report with executive summary, prioritized action items, and generated tests

**Execution time**: ~2-3 minutes (thorough)

---

### ⚡ /pre-commit - Quick Pre-Commit Check
**Purpose**: Fast validation before committing code

**What it does**:
- Checks for null pointer crashes
- Validates critical state machine issues
- Detects memory leaks
- Verifies input buffering logic
- Checks hold system correctness
- Scans for debug code left in

**Usage**:
```
/pre-commit
```

**When to use**:
- Before every git commit
- As part of pre-commit hook
- Quick sanity check during development
- Before pushing to shared branch

**Output**: Pass/Warn/Fail status with quick fixes

**Execution time**: ~30 seconds (fast)

---

### 🔧 /post-fix - Post-Fix Validation
**Purpose**: Validate bug fix and check for regressions

**What it does**:
- Verifies original bug is fixed
- Checks for new bugs introduced
- Tests related systems still work
- Generates regression test for the bug
- Provides fix approval verdict

**Usage**:
```
/post-fix
```
*(Will ask for bug details if not provided)*

**When to use**:
- After fixing a bug
- Before closing bug tickets
- To verify fix didn't break anything
- To generate regression tests

**Output**: Fix verification report with regression test code

**Execution time**: ~1 minute

---

### 💥 /fix-crash - Crash Fix Agent
**Purpose**: Analyze crashes, identify root cause, and generate fixes

**What it does**:
- Analyzes crash logs and stack traces
- Identifies root cause (null pointer, use-after-free, array bounds, etc.)
- Generates appropriate fix with null guards
- Searches for similar crash-prone code patterns
- Generates regression test to prevent crash from returning
- Provides prevention advice for similar crashes

**Usage**:
```
/fix-crash
```
*(Will ask for crash log location or stack trace)*

**When to use**:
- When application crashes
- After receiving crash reports
- To analyze crash dumps
- To fix memory access violations
- To prevent similar crashes

**Output**: Comprehensive crash analysis with fix code, regression test, and prevention guide

**Execution time**: ~1-2 minutes

---

## Command Workflow Examples

### Example 1: After Implementing New Feature
```bash
# Option A: Individual commands
/validate-combat
/sync-docs
/generate-tests [feature-name]

# Option B: Single combo command
/full-audit
```

### Example 2: Debugging Combat Issues
```bash
# 1. Find the issue
/validate-combat

# 2. Fix the bug
# [Make your fixes...]

# 3. Validate the fix
/post-fix
```

### Example 3: Weekly Maintenance
```bash
# Monday morning health check
/full-audit
```

### Example 4: Pre-Release Checklist
```bash
# Comprehensive check
/full-audit

# Review findings and fix issues
# [Fix critical issues...]

# Quick final check
/pre-commit
```

### Example 5: Daily Development Flow
```bash
# While coding...
# [Make changes...]

# Before commit
/pre-commit

# If issues found, fix and re-run
/pre-commit  # Until pass
```

### Example 6: Bug Fix Workflow
```bash
# 1. Identify bug
/validate-combat

# 2. Fix the bug
# [Write fix...]

# 3. Validate fix
/post-fix

# 4. Quick commit check
/pre-commit

# 5. Commit
git add .
git commit -m "Fix: [description]"
```

---

## Tips for Effective Use

### For /sync-docs
- Run after major refactors to catch documentation drift
- Use findings to update `docs/SYSTEM_PROMPT.md` and architecture docs
- Pay attention to "Documentation Updates Needed" section

### For /validate-combat
- Focus on Critical issues first (crashes, major bugs)
- Medium issues next (logic errors)
- Low issues for code quality improvements
- Re-run after fixing to ensure no regressions

### For /generate-tests
- Start with specific features first (`/generate-tests combo`)
- Use generated code as templates, customize as needed
- Implement Critical and High priority tests first
- Blueprint tests useful for visual debugging

---

## Customizing Commands

These commands are markdown files in `.claude/commands/`. You can:

1. **Edit command behavior**: Modify the `.md` file
2. **Add new checks**: Add sections to validation logic
3. **Change output format**: Modify output templates
4. **Add new features**: Create new test categories

Example customization:
```markdown
# In validate-combat.md, add new check:

### 11. **Custom Validation**

Check for project-specific patterns:
- ✅ All attacks have unique names
- ❌ Duplicate attack IDs
```

---

## Troubleshooting

**Q: Command not showing up**
- Ensure file is in `.claude/commands/`
- Filename must end with `.md`
- Restart Claude Code

**Q: Command runs but produces generic output**
- Check command has access to necessary tools (Read, Grep, Glob)
- Verify file paths in command are correct
- Check prompt instructions are clear

**Q: Want to modify command output**
- Edit the "Output Format" section in the `.md` file
- Customize markdown templates
- Add/remove validation categories

---

## Future Command Ideas

Consider adding these commands later:
- `/profile-performance` - Performance profiling
- `/audit-attacks` - AttackData validation
- `/debug-states <from> <to>` - State transition debugger
- `/map-components` - Component dependency mapper
- `/setup-notifies <attack>` - AnimNotify setup assistant

---

Happy testing! 🗡️