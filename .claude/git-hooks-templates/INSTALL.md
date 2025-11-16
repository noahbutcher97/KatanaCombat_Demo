# Git Hooks Installation Guide

Integrate Claude Code hooks into your git workflow for automatic validation.

---

## Quick Installation

### Windows (PowerShell)
```powershell
# From project root
Copy-Item -Path .claude/git-hooks-templates/pre-commit -Destination .git/hooks/pre-commit -Force
Copy-Item -Path .claude/git-hooks-templates/post-commit -Destination .git/hooks/post-commit -Force

# Verify installation
ls .git/hooks/
```

### Linux/Mac (Bash)
```bash
# From project root
cp .claude/git-hooks-templates/pre-commit .git/hooks/pre-commit
cp .claude/git-hooks-templates/post-commit .git/hooks/post-commit

# Make executable
chmod +x .git/hooks/pre-commit
chmod +x .git/hooks/post-commit

# Verify installation
ls -la .git/hooks/
```

---

## Available Hooks

### pre-commit (Recommended)
**Purpose**: Runs validation checks before commits are allowed
**Script**: Calls `.claude/hooks/before-commit.ps1`

**What it checks**:
- Critical file modifications (CombatTypes.h, AttackData, etc.)
- Architecture compliance
- Mixed concerns in single commit
- Large commit warnings (>20 files)

**Exit codes**:
- `0` - Validation passed, commit proceeds
- `1` - Validation failed, commit blocked

**Bypass** (not recommended):
```bash
git commit --no-verify
```

---

### post-commit (Optional)
**Purpose**: Tracks commits for analytics (currently placeholder)
**Script**: Can be extended to call `context-tracker.ps1`

**Use cases**:
- Commit frequency tracking
- Context switch pattern analysis
- Development workflow insights

---

## What Happens When You Commit

### With Pre-Commit Hook Installed:

```
$ git commit -m "Add new attack"

🔍 Running pre-commit validation...

Checking critical files... ✅
Checking architecture compliance... ✅
Checking for mixed concerns... ✅
Checking commit size... ✅ (8 files)

✅ Pre-commit validation PASSED

[main abc1234] Add new attack
 8 files changed, 150 insertions(+), 20 deletions(-)
```

### If Validation Fails:

```
$ git commit -m "Major refactor"

🔍 Running pre-commit validation...

Checking critical files... ⚠️ Warning: CombatTypes.h modified
Checking architecture compliance... ❌ FAILED
Checking for mixed concerns... ⚠️ Warning: Animation + Combat files
Checking commit size... ⚠️ Warning: Large commit (25 files)

❌ Pre-commit validation FAILED

**Critical Issues**:
- Delegate declarations outside CombatTypes.h
- AnimNotify in wrong namespace

To bypass (NOT RECOMMENDED):
  git commit --no-verify

To fix issues:
  - Review validation errors above
  - Fix critical issues
  - Run /validate-combat or /check-warnings
```

---

## Hook Behavior

### before-commit.ps1 Validation Levels

**Critical** (Blocks commit):
- Critical file violations
- Architecture non-compliance
- Test failures in test files

**Warnings** (Allows commit):
- Large commit size (>20 files)
- Mixed concerns (multiple domains)
- Documentation out of sync

**Advisory** (Informational):
- Suggestions for improvement
- Best practice reminders

---

## Customization

### Adjust Validation Strictness

Edit `.claude/hooks/before-commit.ps1`:

```powershell
# Make warnings block commits
$STRICT_MODE = $true

# Increase large commit threshold
$LARGE_COMMIT_THRESHOLD = 30  # Default: 20

# Disable specific checks
$CHECK_MIXED_CONCERNS = $false
```

### Disable Temporarily

```powershell
# PowerShell
$env:CLAUDE_SKIP_VALIDATION = "1"
git commit -m "Message"
Remove-Item Env:CLAUDE_SKIP_VALIDATION
```

```bash
# Bash/Zsh
export CLAUDE_SKIP_VALIDATION=1
git commit -m "Message"
unset CLAUDE_SKIP_VALIDATION
```

### Disable Permanently

```bash
# Remove hook files
rm .git/hooks/pre-commit
rm .git/hooks/post-commit
```

---

## Troubleshooting

### Hook Not Running
**Symptom**: Commits go through without validation
**Solutions**:
1. Verify hook file exists: `ls .git/hooks/pre-commit`
2. Check file has no extension (not `.ps1`, `.sh`, etc.)
3. Verify executable (Linux/Mac): `chmod +x .git/hooks/pre-commit`
4. Check PowerShell execution policy (Windows):
   ```powershell
   Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
   ```

### Hook Fails with "Command Not Found"
**Symptom**: `powershell: command not found`
**Solutions** (Linux/Mac):
1. Install PowerShell: https://docs.microsoft.com/en-us/powershell/scripting/install/installing-powershell
2. Update hook shebang to: `#!/usr/bin/env pwsh`
3. Or use bash wrapper:
   ```bash
   #!/bin/bash
   pwsh -ExecutionPolicy Bypass -File .claude/hooks/before-commit.ps1
   ```

### Hook Runs But Always Passes
**Symptom**: No validation errors even with known issues
**Solutions**:
1. Check `CLAUDE_SKIP_VALIDATION` not set: `echo $env:CLAUDE_SKIP_VALIDATION`
2. Verify `before-commit.ps1` exists and has content
3. Run hook manually to see output:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .claude/hooks/before-commit.ps1
   ```

### Want More Verbose Output
**Solution**: Edit `before-commit.ps1`:
```powershell
$VERBOSE = $true  # Add at top of file
```

---

## Alternative: Manual Validation

If you prefer not to use git hooks, run validation manually before commits:

### Via Slash Command
```
/pre-commit
```

### Via PowerShell
```powershell
powershell -ExecutionPolicy Bypass -File .claude/hooks/before-commit.ps1
```

### Via Git Alias
Add to `.git/config` or `~/.gitconfig`:
```ini
[alias]
    precommit = !powershell -ExecutionPolicy Bypass -File .claude/hooks/before-commit.ps1
```

Then use:
```bash
git precommit
git commit
```

---

## Integration with CI/CD

The same `before-commit.ps1` script can be used in CI/CD pipelines:

### GitHub Actions
```yaml
- name: Run Pre-Commit Validation
  run: |
    pwsh -ExecutionPolicy Bypass -File .claude/hooks/before-commit.ps1
  shell: pwsh
```

### GitLab CI
```yaml
validate:
  script:
    - pwsh -ExecutionPolicy Bypass -File .claude/hooks/before-commit.ps1
```

---

## Related Documentation

- **Hook System**: [`.claude/hooks/README.md`](../hooks/README.md)
- **Validation Details**: See `before-commit.ps1` source
- **Commands**: [`.claude/commands/README.md`](../commands/README.md) (use `/pre-commit`)

---

**Recommendation**: Install `pre-commit` hook for safety. Skip `post-commit` unless you want analytics.

**Quick Start**:
```powershell
# Windows
Copy-Item -Path .claude/git-hooks-templates/pre-commit -Destination .git/hooks/pre-commit -Force
```

```bash
# Linux/Mac
cp .claude/git-hooks-templates/pre-commit .git/hooks/pre-commit && chmod +x .git/hooks/pre-commit
```
