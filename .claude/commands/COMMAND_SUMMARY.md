n# Quick Command Reference

## All Available Commands

### 💬 Planning & Clarification
- `/clarify` - Interactive requirements gathering with multiple-choice questions

### 📋 Documentation & Validation (Individual)
- `/sync-docs` - Check code matches documentation
- `/validate-combat` - Validate combat system implementation
- `/generate-tests [feature]` - Generate test cases and code

### 🔧 Debugging & Fixes (Individual)
- `/fix-crash` - Analyze and fix crashes
- `/post-fix` - Validate bug fix, check regressions

### ⚡ Workflow Automation (Combos)
- `/full-audit` - 3-phase comprehensive audit (2-3 min)
- `/pre-commit` - Quick pre-commit validation (30 sec)

---

## When To Use What

| Scenario | Command | Why |
|----------|---------|-----|
| **Not sure what I want** | `/clarify` | Asks questions to understand requirements |
| **Vague feature request** | `/clarify` | Gathers details through multiple-choice |
| **Crash happened** | `/fix-crash` | Analyzes crash log, generates fix + test |
| **Just fixed a bug** | `/post-fix` | Verifies fix, checks regressions |
| **Before committing** | `/pre-commit` | Fast check for critical issues |
| **After new feature** | `/full-audit` | Complete validation + docs + tests |
| **Weekly health check** | `/full-audit` | Comprehensive system assessment |
| **Docs out of date?** | `/sync-docs` | Find what needs updating |
| **Need tests** | `/generate-tests [feature]` | Get code templates |
| **Debugging issue** | `/validate-combat` | Find bugs and logic errors |

---

## Command Chaining Patterns

### Pattern 1: Crash Recovery
```
/fix-crash          # Analyze and fix
/post-fix           # Verify fix works
/pre-commit         # Final check
```

### Pattern 2: Feature Development
```
/full-audit         # Or run individually:
                    #   /validate-combat
                    #   /sync-docs
                    #   /generate-tests [feature]
```

### Pattern 3: Bug Fix
```
/validate-combat    # Find the bug
# [Fix it...]
/post-fix          # Verify fix
/pre-commit        # Quick check
```

### Pattern 4: New Feature (with Clarification)
```
/clarify           # Understand requirements first
# [Answer questions via multiple-choice]
# [Implement feature based on clarified requirements]
/full-audit        # Validate implementation
```

---

## Execution Times

| Command | Time | When to Use |
|---------|------|-------------|
| `/pre-commit` | ~30s | Every commit |
| `/post-fix` | ~1m | After fixing bugs |
| `/fix-crash` | ~1-2m | When crashes occur |
| `/validate-combat` | ~1-2m | After changes |
| `/sync-docs` | ~1-2m | After features/refactors |
| `/generate-tests` | ~1-2m | When need tests |
| `/full-audit` | ~2-3m | Weekly/before releases |

---

## Quick Start

**New to the commands?** Start here:

1. **Test them out**: Run `/pre-commit` to see the output format
2. **Explore**: Try `/validate-combat` to see what it finds
3. **Automate**: Use `/full-audit` for comprehensive checks
4. **Customize**: Edit `.claude/commands/*.md` files to adjust

---

## Files Created

```
.claude/commands/
├── README.md              # Full documentation
├── COMMAND_SUMMARY.md     # This quick reference
├── clarify.md             # ⭐ Interactive requirements gathering
├── sync-docs.md           # Doc sync checker
├── validate-combat.md     # Combat validator
├── generate-tests.md      # Test generator
├── full-audit.md          # Combined audit
├── pre-commit.md          # Quick commit check
├── post-fix.md            # Fix validator
└── fix-crash.md           # Crash analyzer
```

---

For full documentation, see [README.md](README.md)