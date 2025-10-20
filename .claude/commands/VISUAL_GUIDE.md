# KatanaCombat Command Visual Guide

## 🎯 Complete Command System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                   KATANACOMBAT COMMANDS                     │
│                    8 Specialized Agents                     │
└─────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│  💬 PLANNING & REQUIREMENTS                                  │
├──────────────────────────────────────────────────────────────┤
│  /clarify                                                    │
│  ├─ Asks multiple-choice questions                          │
│  ├─ Gathers requirements interactively                       │
│  ├─ Prevents misunderstandings                              │
│  └─ Recommends implementation approaches                     │
│                                                              │
│  USE WHEN: "Add a new attack", "Fix the combat",            │
│            "Make it feel better" (vague requests)            │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│  📋 VALIDATION & DOCUMENTATION                               │
├──────────────────────────────────────────────────────────────┤
│  /validate-combat          (~1-2 min)                        │
│  ├─ State machine validation                                │
│  ├─ Input buffering checks                                  │
│  ├─ Memory safety audit                                     │
│  └─ Reports: Critical/Medium/Low issues                     │
│                                                              │
│  /sync-docs                (~1-2 min)                        │
│  ├─ Compares code vs documentation                          │
│  ├─ Validates design principles                             │
│  ├─ Checks default values                                   │
│  └─ Reports: Discrepancies + doc updates needed             │
│                                                              │
│  /generate-tests [feature] (~1-2 min)                        │
│  ├─ Generates 48+ test scenarios                            │
│  ├─ Creates C++ test code templates                         │
│  ├─ Produces Blueprint test actors                          │
│  └─ Outputs: Copy-paste ready test code                     │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│  🔧 DEBUGGING & FIXES                                        │
├──────────────────────────────────────────────────────────────┤
│  /fix-crash                (~1-2 min)                        │
│  ├─ Analyzes crash logs & stack traces                      │
│  ├─ Identifies root cause                                   │
│  ├─ Generates fix with null guards                          │
│  ├─ Searches similar crash patterns                         │
│  └─ Creates regression test                                 │
│                                                              │
│  /post-fix                 (~1 min)                          │
│  ├─ Verifies bug is fixed                                   │
│  ├─ Checks for regressions                                  │
│  ├─ Tests related systems                                   │
│  └─ Generates regression test                               │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│  ⚡ WORKFLOW AUTOMATION (COMBO COMMANDS)                     │
├──────────────────────────────────────────────────────────────┤
│  /full-audit              (~2-3 min) ★ COMPREHENSIVE         │
│  ├─ Phase 1: Validate combat system                         │
│  ├─ Phase 2: Check documentation sync                       │
│  ├─ Phase 3: Generate targeted tests                        │
│  └─ Outputs: Executive summary + action items               │
│                                                              │
│  /pre-commit              (~30 sec) ★ FAST                   │
│  ├─ Quick null pointer checks                               │
│  ├─ Critical state machine issues                           │
│  ├─ Memory leak detection                                   │
│  └─ Outputs: PASS/WARN/FAIL status                          │
└──────────────────────────────────────────────────────────────┘

```

---

## 📊 Decision Tree: Which Command Should I Use?

```
START: What do you need to do?
│
├─ "I'm not sure what I want exactly"
│  └─> /clarify (asks questions to clarify)
│
├─ "The game just crashed"
│  └─> /fix-crash (analyzes crash log)
│
├─ "I just fixed a bug"
│  └─> /post-fix (validates fix)
│
├─ "About to commit code"
│  └─> /pre-commit (quick validation)
│
├─ "Just finished a feature"
│  └─> /full-audit (comprehensive check)
│
├─ "Weekly health check"
│  └─> /full-audit (thorough analysis)
│
├─ "Need to write tests"
│  └─> /generate-tests [feature]
│
├─ "Combat system feels buggy"
│  └─> /validate-combat (find issues)
│
└─ "Docs might be outdated"
   └─> /sync-docs (check documentation)
```

---

## 🔄 Common Workflow Patterns

### 🎮 Pattern 1: New Feature Development

```
Step 1: Plan
┌─────────────────┐
│   /clarify      │ ← Start here if request is vague
└────────┬────────┘
         │ Answer questions
         ↓
Step 2: Implement
┌─────────────────┐
│  [Code here]    │
└────────┬────────┘
         │
         ↓
Step 3: Validate
┌─────────────────┐
│  /full-audit    │ ← Comprehensive validation
└────────┬────────┘
         │
         ↓
Step 4: Fix Issues
┌─────────────────┐
│  [Fix issues]   │
└────────┬────────┘
         │
         ↓
Step 5: Final Check
┌─────────────────┐
│  /pre-commit    │ ← Quick sanity check
└────────┬────────┘
         │
         ↓
      COMMIT ✓
```

### 💥 Pattern 2: Crash Recovery

```
┌─────────────────┐
│  CRASH! ☠️       │
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│   /fix-crash    │ ← Analyze crash log
└────────┬────────┘
         │ Identifies: Null pointer at line 532
         ↓
┌─────────────────┐
│  Apply Fix      │ ← Add null check
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│   /post-fix     │ ← Verify fix works
└────────┬────────┘
         │ Checks for regressions
         ↓
┌─────────────────┐
│  /pre-commit    │ ← Final validation
└────────┬────────┘
         │
         ↓
      COMMIT ✓
```

### 🐛 Pattern 3: Bug Fix

```
┌─────────────────┐
│  Bug Reported   │
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│ /validate-combat│ ← Find the issue
└────────┬────────┘
         │ Found: Input buffering broken
         ↓
┌─────────────────┐
│  Fix the Bug    │
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│   /post-fix     │ ← Validate fix + generate test
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│  /pre-commit    │ ← Quick check
└────────┬────────┘
         │
         ↓
      COMMIT ✓
```

### 📅 Pattern 4: Weekly Maintenance

```
Monday Morning
       │
       ↓
┌─────────────────┐
│  /full-audit    │ ← Comprehensive health check
└────────┬────────┘
         │
         ↓
    Review Report
         │
         ├─ Critical Issues? → Fix immediately
         ├─ Medium Issues? → Schedule this week
         └─ Low Issues? → Backlog
         │
         ↓
   Document Findings
```

---

## 🎨 Command Comparison Matrix

| Feature | /clarify | /validate | /sync-docs | /generate-tests | /fix-crash | /post-fix | /pre-commit | /full-audit |
|---------|----------|-----------|------------|-----------------|------------|-----------|-------------|-------------|
| **Interactive** | ✅ | ❌ | ❌ | ❌ | ✅ | ✅ | ❌ | ❌ |
| **Finds Bugs** | ❌ | ✅ | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ |
| **Generates Code** | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | ❌ | ✅ |
| **Checks Docs** | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| **Speed** | Variable | 1-2min | 1-2min | 1-2min | 1-2min | 1min | 30sec | 2-3min |
| **Use Frequency** | As needed | Often | Weekly | As needed | When crash | After fix | Every commit | Weekly |

---

## 💡 Pro Tips

### Chaining Commands
```bash
# ✅ DO: Run them sequentially, review each output
/validate-combat
# [Review results]
/sync-docs
# [Review results]
/generate-tests combo

# ✅ OR: Use combo command
/full-audit  # Runs all 3 phases automatically
```

### Command Customization
```bash
# All commands are .md files in .claude/commands/
# You can edit them to:
# - Add custom validation checks
# - Modify output format
# - Add project-specific patterns
# - Change execution behavior

# Example:
# Edit: .claude/commands/validate-combat.md
# Add new check in the markdown file
```

### Interpreting Results
```
🔴 Critical = Fix immediately (crashes, blocking bugs)
🟡 Medium   = Fix soon (logic errors, design violations)
🟢 Low      = Fix eventually (code quality, optimizations)
```

---

## 📈 Workflow Evolution

### Beginner (Week 1)
```
Just use: /pre-commit before every commit
```

### Intermediate (Week 2-4)
```
Add: /validate-combat when things feel buggy
Add: /fix-crash when crashes occur
```

### Advanced (Month 2+)
```
Add: /full-audit weekly
Add: /clarify before complex features
Add: /sync-docs after refactors
Use: /generate-tests for critical features
```

### Expert (Month 3+)
```
Customize commands for project needs
Chain commands in custom workflows
Integrate with CI/CD pipelines
Create new project-specific commands
```

---

## 🎯 Quick Start Checklist

- [ ] Verify commands exist in `.claude/commands/`
- [ ] Test `/pre-commit` to see output format
- [ ] Try `/validate-combat` on current code
- [ ] Run `/full-audit` for comprehensive check
- [ ] Bookmark this guide for reference
- [ ] Customize commands as needed

---

For complete documentation: [README.md](README.md)
For quick reference: [COMMAND_SUMMARY.md](COMMAND_SUMMARY.md)