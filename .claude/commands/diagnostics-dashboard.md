# Diagnostics Dashboard

You are generating a comprehensive diagnostics health dashboard for the KatanaCombat project.

## Your Task

Create a visual dashboard showing overall codebase health, trends, and actionable insights.

---

## Step 1: Gather Data

### A. IDE Diagnostics
Use `mcp__ide__getDiagnostics` on key files:
- Core combat files (CombatComponent, CombatComponentV2)
- Animation files (AnimNotify*)
- Data files (AttackData, CombatSettings)

### B. Git Status
```bash
git status --short
git log -1 --format="%h %s"
```

### C. File Statistics
Count files by type:
- C++ headers (.h): [count]
- C++ source (.cpp): [count]
- Test files (*Test.cpp): [count]
- Documentation (.md): [count]

### D. TODO/FIXME Count
Search for:
- `// TODO:` - Feature requests
- `// FIXME:` - Known bugs
- `// HACK:` - Technical debt
- `// NOTE:` - Important comments

---

## Step 2: Process Diagnostics

Load `.claude/diagnostics-config.json` and apply filters:
1. Categorize by severity (Error, Warning, Info)
2. Filter false positives (Blueprint-exposed, etc.)
3. Categorize by type (Critical, Security, Performance, Style)
4. Group by file/component

---

## Step 3: Generate Dashboard

```markdown
# 🎯 KatanaCombat Diagnostics Dashboard

**Generated**: [timestamp]
**Last Commit**: [hash] [message]
**Active Context**: [mode name if any]

---

## 📊 Overall Health Score

[Calculate score: 100 - (errors*10 + warnings*2 + TODOs*0.5)]

### Score: [X]/100 [████████▒▒] [Rating: Excellent/Good/Fair/Poor]

**Health Rating**:
- 90-100: ✅ Excellent (production ready)
- 75-89: 🟢 Good (minor improvements needed)
- 50-74: 🟡 Fair (review recommended)
- 0-49: 🔴 Poor (immediate action required)

---

## ⚠️ Issue Summary

| Severity | Count | Change | Action |
|----------|-------|--------|--------|
| ❌ Errors | [N] | [↑↓→ vs last check] | Block commit |
| ⚠️ Warnings | [N] | [↑↓→] | Review |
| ℹ️ Info | [N] | [↑↓→] | Optional |
| 🔇 Filtered | [N] | [↑↓→] | Ignored (false positives) |

---

## 🔥 Critical Issues (Requires Immediate Action)

[If none:]
✅ **No critical issues detected**

[If any:]
### [Component/File]
1. **[File:Line]** - [Issue description]
   - **Fix**: [Specific action]
   - **Priority**: Critical
   - **Estimated Time**: [time]

---

## 📈 Health Trends

[Compare with previous state if available]

**Since Last Check**:
- Errors: [N] → [M] ([+/-X])
- Warnings: [N] → [M] ([+/-X])
- TODOs: [N] → [M] ([+/-X])

**Velocity**:
- Issues Resolved: [count]
- New Issues: [count]
- Net Change: [+/- count]

---

## 🎯 Focus Areas

[Based on patterns, identify problem areas:]

### [Component Name] ([severity])
- **Issue Count**: [N]
- **Primary Issues**: [list top 3]
- **Recommended Action**: [specific command or steps]

---

## 📂 Component Breakdown

[Group by component/system]

### Core Combat System
- ✅ CombatComponent: [N issues]
- ⚠️ CombatComponentV2: [N issues]
- ✅ ActionQueue: [N issues]

### Animation System
- ✅ AnimNotify_AttackPhaseTransition: [N issues]
- ✅ AnimNotifyState_ParryWindow: [N issues]

### Data Assets
- ✅ AttackData: [N issues]
- ✅ CombatSettings: [N issues]

[For each with issues, show breakdown:]
- Errors: [N]
- Warnings: [N]
- Most Common: [issue type]

---

## 📝 Technical Debt Tracker

### TODOs ([total count])
[Group by priority/component]
- **High Priority**: [count] - [list top 3]
- **Medium Priority**: [count]
- **Low Priority**: [count]

### FIXMEs ([total count])
[Known bugs to address]
- **[Component]**: [issue description] ([file:line])

### HACKs ([total count])
[Temporary solutions needing refactoring]
- **[Component]**: [hack description] ([file:line])

---

## 🔍 False Positive Report

**Total Filtered**: [count]

| Reason | Count | Examples |
|--------|-------|----------|
| Blueprint-exposed | [N] | OnAttackStarted, OnPhaseChanged |
| Editor-only code | [N] | EditorUtility functions |
| Macro expansions | [N] | GENERATED_BODY |
| Intentionally unused | [N] | Variables with // NOLINT |

---

## 💡 Recommended Actions

[Prioritized list based on dashboard data:]

### Immediate (Do Now)
1. [Action] - [reason] - **Estimated**: [time]
   - Command: `/fix-crash` or specific fix
   - Files: [list]

### Short Term (This Week)
1. [Action] - [reason]
   - Command: [relevant command]

### Long Term (Backlog)
1. [Action] - [reason]

---

## 🚀 Quick Wins

[Easy fixes that improve score quickly:]
1. [Fix description] - **Impact**: +[points] - **Time**: [minutes]
2. [Fix description] - **Impact**: +[points] - **Time**: [minutes]

---

## 📋 Pre-Commit Checklist

Based on current issues:

- [ ] No critical errors present
- [ ] Warnings reviewed and acknowledged
- [ ] TODOs updated (if adding new ones)
- [ ] Documentation synced (if changing APIs)
- [ ] Tests passing (if test files modified)

**Ready to Commit?** [YES/NO - with reasoning]

---

## 🔧 Useful Commands

Based on dashboard findings:
- `/check-warnings` - Detailed diagnostics analysis
- `/validate-combat` - Architecture compliance
- `/sync-docs` - Documentation alignment
- `/pre-commit` - Full pre-commit validation
- `/fix-crash` - Debug critical issues

---

## 📊 Statistics

### Codebase Metrics
- Total C++ Files: [N]
- Lines of Code: [estimate from file sizes]
- Test Coverage: [N test files / N source files * 100]%

### Quality Metrics
- Issues per 100 LOC: [calculate]
- Critical Issue Density: [critical / total files]
- Documentation Ratio: [.md files / .cpp files]

---

## 🎯 Next Dashboard

Run again:
- After fixing critical issues
- Before major commits
- Weekly for health tracking

**Trend Tracking**: Save this dashboard with timestamp to compare future runs.
```

---

## Step 4: Contextual Insights

If active context mode detected, add mode-specific section:

```markdown
## 🎯 Context-Specific Analysis: [MODE]

[For animation mode:]
- Phase notify order compliance: [X/Y files correct]
- Deprecated AnimNotifyState usage: [count]
- Timing validation needed: [files]

[For combat-logic mode:]
- State transition validation: [status]
- Input buffering compliance: [status]
- Timer vs Tick usage: [timer count] vs [tick count]

[For testing mode:]
- Test pass rate: [X/Y]
- Uncovered components: [list]
- Test assertions: [count]
```

---

## Step 5: Export Options

Offer to export dashboard:
```markdown
**Export Options**:
- 💾 Save to `.claude/dashboards/[timestamp].md`
- 📧 Copy markdown for sharing
- 📊 Generate CSV for tracking trends
```

---

## Notes

- Dashboard should be **actionable** - every issue should have a clear next step
- Use **visual indicators** (emojis, progress bars) for quick scanning
- **Compare trends** if previous dashboard exists
- **Prioritize** - not all warnings are equal
- **Context-aware** - focus on active context if applicable