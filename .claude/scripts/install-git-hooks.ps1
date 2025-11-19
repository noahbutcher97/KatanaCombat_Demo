# Install Git Hooks Script
# Installs pre-commit and post-commit hooks for Claude Code validation

param(
    [switch]$Force,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

Write-Host "Installing Claude Code Git Hooks..." -ForegroundColor Cyan
Write-Host ""

# Check if .git directory exists
if (-not (Test-Path .git)) {
    Write-Host "[ERROR] Not a git repository" -ForegroundColor Red
    Write-Host "   Run this command from the repository root" -ForegroundColor Gray
    exit 1
}

# Check if hooks directory exists
if (-not (Test-Path .git/hooks)) {
    Write-Host "Creating .git/hooks directory..." -ForegroundColor Yellow
    New-Item -Path .git/hooks -ItemType Directory -Force | Out-Null
}

# Function to backup existing hook
function Backup-Hook {
    param([string]$HookPath)

    if (Test-Path $HookPath) {
        $backupPath = "$HookPath.backup"
        if (Test-Path $backupPath) {
            Remove-Item $backupPath -Force
        }
        Copy-Item $HookPath $backupPath -Force
        Write-Host "   [BACKUP] Backed up existing hook: $backupPath" -ForegroundColor Gray
    }
}

# Install pre-commit hook
Write-Host "Installing pre-commit hook..." -ForegroundColor Cyan

$preCommitPath = ".git/hooks/pre-commit"
if ((Test-Path $preCommitPath) -and -not $Force) {
    Write-Host "[WARNING] Pre-commit hook already exists" -ForegroundColor Yellow
    Backup-Hook -HookPath $preCommitPath
}

# Create pre-commit hook content
$preCommitContent = @'
#!/bin/bash

echo "Running Claude Code pre-commit validation..."
echo ""

# Step 1: Advisory diagnostics
echo "Running diagnostics check..."
powershell -ExecutionPolicy Bypass -File .claude/hooks/pre-commit-diagnostics.ps1
echo ""

# Step 2: Comprehensive validation
echo "Running validation..."
powershell -ExecutionPolicy Bypass -File .claude/hooks/before-commit.ps1

exit_code=$?

if [ $exit_code -ne 0 ]; then
    echo ""
    echo "[BLOCKED] Commit blocked by validation"
    echo ""
    echo "Fix issues or bypass with: git commit --no-verify"
    exit 1
fi

echo ""
echo "[SUCCESS] Validation passed"
exit 0
'@

# Write pre-commit hook
$preCommitContent | Out-File -FilePath $preCommitPath -Encoding ASCII -NoNewline
Write-Host "   [SUCCESS] Created: $preCommitPath" -ForegroundColor Green

# Install post-commit hook
Write-Host "Installing post-commit hook..." -ForegroundColor Cyan

$postCommitPath = ".git/hooks/post-commit"
if ((Test-Path $postCommitPath) -and -not $Force) {
    Write-Host "[WARNING] Post-commit hook already exists" -ForegroundColor Yellow
    Backup-Hook -HookPath $postCommitPath
}

# Check if post-commit.ps1 exists, if not create a simple one
if (-not (Test-Path .claude/hooks/post-commit.ps1)) {
    Write-Host "   Creating post-commit.ps1..." -ForegroundColor Gray

    $postCommitScriptContent = @'
# Post-Commit Hook
# Runs after successful commits

param()

# Record commit in context history
$commitHash = git rev-parse HEAD
$commitMsg = git log -1 --pretty=%B

Write-Host "Post-commit: $commitHash" -ForegroundColor Gray
Write-Host "   Message: $commitMsg" -ForegroundColor Gray

# TODO: Add post-commit actions here
# - Update context history
# - Clean temporary files
# - Notify external systems
'@

    Set-Content -Path .claude/hooks/post-commit.ps1 -Value $postCommitScriptContent -Encoding UTF8
}

# Create post-commit hook content
$postCommitContent = @'
#!/bin/bash

echo "Running post-commit actions..."

# Update context history
powershell -ExecutionPolicy Bypass -File .claude/hooks/post-commit.ps1

exit 0
'@

# Write post-commit hook
$postCommitContent | Out-File -FilePath $postCommitPath -Encoding ASCII -NoNewline
Write-Host "   [SUCCESS] Created: $postCommitPath" -ForegroundColor Green

# Make hooks executable (Linux, macOS, and Git Bash on Windows)
Write-Host "Making hooks executable..." -ForegroundColor Cyan
try {
    # Try using chmod (works on Linux, macOS, and Git Bash)
    $null = chmod +x $preCommitPath 2>&1
    $null = chmod +x $postCommitPath 2>&1
    Write-Host "   [SUCCESS] Hooks are now executable" -ForegroundColor Green
} catch {
    # If chmod fails, assume Windows without Git Bash
    Write-Host "   [INFO] chmod not available, hooks may need manual chmod on Git Bash" -ForegroundColor Gray
}

Write-Host ""
Write-Host "[SUCCESS] Git hooks installed successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "Installed hooks:" -ForegroundColor Cyan
Write-Host "  - $preCommitPath   (validation)" -ForegroundColor Gray
Write-Host "  - $postCommitPath  (cleanup)" -ForegroundColor Gray
Write-Host ""
Write-Host "Test with:" -ForegroundColor Yellow
Write-Host "   git add ." -ForegroundColor Gray
Write-Host "   git commit -m test --dry-run" -ForegroundColor Gray
Write-Host ""
Write-Host "Bypass hooks when needed:" -ForegroundColor Yellow
Write-Host "   git commit --no-verify -m message" -ForegroundColor Gray
Write-Host ""

if ($Verbose) {
    Write-Host "Verbose: Hook installation complete" -ForegroundColor DarkGray
    Write-Host "  Pre-commit hook: $preCommitPath" -ForegroundColor DarkGray
    Write-Host "  Post-commit hook: $postCommitPath" -ForegroundColor DarkGray
}