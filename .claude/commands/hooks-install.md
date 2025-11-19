# Install Git Hooks

**Purpose**: Automatically install Claude Code hooks into your git repository for automated validation.

---

## What This Does

Installs the following git hooks:
- **pre-commit** - Runs validation before commits (diagnostics + before-commit checks)
- **post-commit** - Runs post-commit actions (updates context history)

These hooks ensure code quality and prevent commits with critical issues.

---

## Usage

```
/hooks install
```

---

## What Gets Installed

### 1. Pre-Commit Hook (`.git/hooks/pre-commit`)
**Runs Before Every Commit**:
1. `pre-commit-diagnostics.ps1` - Advisory checks and checklists
2. `before-commit.ps1` - Comprehensive validation

**Exit Behavior**:
- Exit 0 = Commit proceeds
- Exit 1 = Commit blocked (use `--no-verify` to bypass)

### 2. Post-Commit Hook (`.git/hooks/post-commit`)
**Runs After Successful Commit**:
- Updates context history
- Records commit metadata
- Cleans up temporary files

---

## Implementation

**Step 1**: Check if hooks already exist
```powershell
$preCommitExists = Test-Path .git/hooks/pre-commit
$postCommitExists = Test-Path .git/hooks/post-commit

if ($preCommitExists) {
    Write-Host "⚠️  Pre-commit hook already exists" -ForegroundColor Yellow
    Write-Host "   Backup will be created: .git/hooks/pre-commit.backup"
}
```

**Step 2**: Create pre-commit hook
```bash
#!/bin/bash

echo "🔍 Running Claude Code pre-commit validation..."
echo ""

# Step 1: Advisory diagnostics
echo "📋 Running diagnostics check..."
powershell -ExecutionPolicy Bypass -File .claude/hooks/pre-commit-diagnostics.ps1
echo ""

# Step 2: Comprehensive validation
echo "🔍 Running validation..."
powershell -ExecutionPolicy Bypass -File .claude/hooks/before-commit.ps1

exit_code=$?

if [ $exit_code -ne 0 ]; then
    echo ""
    echo "❌ Commit blocked by validation"
    echo ""
    echo "Fix issues or bypass with: git commit --no-verify"
    exit 1
fi

echo ""
echo "✅ Validation passed"
exit 0
```

**Step 3**: Create post-commit hook
```bash
#!/bin/bash

echo "📝 Running post-commit actions..."

# Update context history
powershell -ExecutionPolicy Bypass -File .claude/hooks/post-commit.ps1

exit 0
```

**Step 4**: Make hooks executable (Unix-based systems)
```powershell
if ($IsLinux -or $IsMacOS) {
    chmod +x .git/hooks/pre-commit
    chmod +x .git/hooks/post-commit
}
```

**Step 5**: Verify installation
```powershell
Write-Host "✅ Git hooks installed successfully" -ForegroundColor Green
Write-Host ""
Write-Host "Installed hooks:" -ForegroundColor Cyan
Write-Host "  • .git/hooks/pre-commit   (validation)" -ForegroundColor Gray
Write-Host "  • .git/hooks/post-commit  (cleanup)" -ForegroundColor Gray
Write-Host ""
Write-Host "Test with: git add . && git commit -m 'test' --dry-run" -ForegroundColor Cyan
```

---

## Testing

After installation, test the hooks:

```bash
# Test pre-commit (dry run)
git add .
git commit -m "test commit" --dry-run

# Should see:
# 🔍 Running Claude Code pre-commit validation...
# 📋 Running diagnostics check...
# 🔍 Running validation...
# ✅ Validation passed
```

---

## Bypassing Hooks

When you need to bypass hooks temporarily:

```bash
# Skip all git hooks
git commit --no-verify -m "Quick fix"

# Or disable specific hook
export CLAUDE_SKIP_VALIDATION=1
git commit -m "Bypass validation only"
```

---

## Uninstalling

To remove git hooks:

```bash
# Remove hooks
rm .git/hooks/pre-commit
rm .git/hooks/post-commit

# Restore backups if they exist
mv .git/hooks/pre-commit.backup .git/hooks/pre-commit
mv .git/hooks/post-commit.backup .git/hooks/post-commit
```

---

## Troubleshooting

### "Hook not executing"
**Cause**: Hook not executable or incorrect line endings
**Fix**:
```bash
# Make executable
chmod +x .git/hooks/pre-commit

# Fix line endings (Windows)
dos2unix .git/hooks/pre-commit
```

### "PowerShell not found"
**Cause**: PowerShell not in PATH
**Fix**:
- **Windows**: Ensure PowerShell 5.1+ or PowerShell 7+ installed
- **Linux/Mac**: Install PowerShell: https://docs.microsoft.com/powershell/scripting/install/installing-powershell

### "Permission denied"
**Cause**: Hook files not executable
**Fix**:
```bash
chmod +x .git/hooks/pre-commit
chmod +x .git/hooks/post-commit
```

### "Validation too strict"
**Cause**: before-commit.ps1 blocking valid commits
**Fix**:
- Use `--no-verify` for one-time bypass
- Or disable validation: `$env:CLAUDE_SKIP_VALIDATION = "1"`

---

## Related Commands

- `/hooks status` - Check which hooks are enabled
- `/hooks skip [name]` - Temporarily disable a hook
- `/hooks enable [name]` - Re-enable a hook

---

## Notes

- Git hooks are **local** to your repository (not committed)
- Each developer must run `/hooks install` after cloning
- Consider adding to onboarding documentation
- Hooks respect `CLAUDE_SKIP_*` environment variables

---

**See Also**: `.claude/hooks/README.md`, `.claude/git-hooks-templates/INSTALL.md`